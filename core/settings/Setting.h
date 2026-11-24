#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/settings/Types.h"

namespace lex::core::settings {

enum class SettingType : std::uint8_t {
  Bool = 0,
  Int,
  Float,
  Color,
  Enum,
  Keybind
};

class SettingVisitor;

class SettingBase {
 public:
  SettingBase(std::string name, std::string description, SettingType type)
      : name_(std::move(name)),
        description_(std::move(description)),
        type_(type) {}

  virtual ~SettingBase() = default;
  SettingBase(const SettingBase&) = delete;
  SettingBase& operator=(const SettingBase&) = delete;

  const std::string& name() const noexcept { return name_; }
  const std::string& description() const noexcept { return description_; }
  SettingType type() const noexcept { return type_; }

  virtual void reset() = 0;
  virtual void accept(SettingVisitor& visitor) = 0;
  virtual nlohmann::json toJson() const = 0;
  virtual void fromJson(const nlohmann::json& value) = 0;

  // Callback sin tipo: lo usa el ModuleManager para marcar la config como
  // modificada sin conocer el tipo concreto del setting.
  void addChangeCallback(std::function<void()> callback) {
    genericCallbacks_.push_back(std::move(callback));
  }

 protected:
  void notifyGenericCallbacks() {
    for (const std::function<void()>& callback : genericCallbacks_) {
      if (callback) callback();
    }
  }

 private:
  std::string name_;
  std::string description_;
  SettingType type_;
  std::vector<std::function<void()>> genericCallbacks_;
};

namespace detail {

// Definido en SettingVisitor.cpp para evitar dependencias circulares.
void dispatchVisit(SettingVisitor& visitor, SettingBase& setting);

inline nlohmann::json toJsonValue(bool value) { return value; }
inline nlohmann::json toJsonValue(int value) { return value; }
inline nlohmann::json toJsonValue(std::size_t value) { return value; }
inline nlohmann::json toJsonValue(float value) { return value; }
inline nlohmann::json toJsonValue(const Color& value) {
  return value.toArgb();
}
inline nlohmann::json toJsonValue(const Keybind& value) {
  return value.toJson();
}

inline void fromJsonValue(const nlohmann::json& json, bool& out) {
  out = json.get<bool>();
}
inline void fromJsonValue(const nlohmann::json& json, int& out) {
  out = json.get<int>();
}
inline void fromJsonValue(const nlohmann::json& json, std::size_t& out) {
  out = json.get<std::size_t>();
}
inline void fromJsonValue(const nlohmann::json& json, float& out) {
  out = json.get<float>();
}
inline void fromJsonValue(const nlohmann::json& json, Color& out) {
  if (json.is_number_integer() || json.is_number_unsigned()) {
    out = Color::fromArgb(json.get<std::uint32_t>());
    return;
  }
  out.r = json.value("r", out.r);
  out.g = json.value("g", out.g);
  out.b = json.value("b", out.b);
  out.a = json.value("a", out.a);
}
inline void fromJsonValue(const nlohmann::json& json, Keybind& out) {
  out.fromJson(json);
}

}  // namespace detail

// Un tipo distinto por SettingType: asi el ClickGUI puede hacer overload de
// visit() sin ambiguedades (IntSetting y EnumSetting no son el mismo tipo).
template <SettingType Type, typename T>
class Setting final : public SettingBase {
 public:
  using Value = T;
  static constexpr SettingType kType = Type;

  Setting(std::string name, std::string description, T value)
      : SettingBase(std::move(name), std::move(description), Type),
        value_(value),
        default_(value) {}

  Setting& withRange(T minimum, T maximum) {
    minimum_ = minimum;
    maximum_ = maximum;
    hasRange_ = true;
    return *this;
  }

  Setting& withStep(T step) {
    step_ = step;
    return *this;
  }

  Setting& withOptions(std::vector<std::string> options) {
    options_ = std::move(options);
    return *this;
  }

  Setting& onChanged(std::function<void(const T&)> callback) {
    changeCallbacks_.push_back(std::move(callback));
    return *this;
  }

  const T& value() const noexcept { return value_; }
  const T& defaultValue() const noexcept { return default_; }
  T minimum() const noexcept { return minimum_; }
  T maximum() const noexcept { return maximum_; }
  T step() const noexcept { return step_; }
  bool hasRange() const noexcept { return hasRange_; }
  const std::vector<std::string>& options() const noexcept { return options_; }

  void setValue(const T& nextValue) {
    const T clamped = clampValue(nextValue);
    if (clamped == value_) return;
    value_ = clamped;
    notify();
  }

  void reset() override {
    if (default_ == value_) return;
    value_ = default_;
    notify();
  }

  void accept(SettingVisitor& visitor) override {
    detail::dispatchVisit(visitor, *this);
  }

  nlohmann::json toJson() const override {
    return detail::toJsonValue(value_);
  }

  void fromJson(const nlohmann::json& value) override {
    T parsed = value_;
    detail::fromJsonValue(value, parsed);
    setValue(parsed);
  }

 private:
  T clampValue(const T& value) const {
    T result = value;

    if constexpr (Type == SettingType::Enum) {
      if (!options_.empty() && result >= options_.size()) {
        result = options_.size() - 1;
      }
    }

    if constexpr (std::is_arithmetic_v<T>) {
      if (hasRange_) result = std::clamp(result, minimum_, maximum_);
    } else {
      (void)minimum_;
      (void)maximum_;
      (void)hasRange_;
    }
    return result;
  }

  void notify() {
    for (const std::function<void(const T&)>& callback : changeCallbacks_) {
      if (callback) callback(value_);
    }
    notifyGenericCallbacks();
  }

  T value_;
  T default_;
  T minimum_{};
  T maximum_{};
  T step_{};
  bool hasRange_ = false;
  std::vector<std::string> options_;
  std::vector<std::function<void(const T&)>> changeCallbacks_;
};

using BoolSetting = Setting<SettingType::Bool, bool>;
using IntSetting = Setting<SettingType::Int, int>;
using FloatSetting = Setting<SettingType::Float, float>;
using ColorSetting = Setting<SettingType::Color, Color>;
using EnumSetting = Setting<SettingType::Enum, std::size_t>;
using KeybindSetting = Setting<SettingType::Keybind, Keybind>;

}  // namespace lex::core::settings
