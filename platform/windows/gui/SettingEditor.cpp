#include "platform/windows/gui/SettingEditor.h"

#include <cstdint>
#include <string>
#include <vector>

#include "platform/windows/gui/VectorIcons.h"

namespace lex::platform::windows::gui {

SettingEditor& SettingEditor::instance() {
  static SettingEditor editor;
  return editor;
}

void SettingEditor::draw(core::settings::SettingBase& setting) {
  const bool wasChanged = changed_;
  setting.accept(*this);
  changed_ = wasChanged || changed_;
}

void SettingEditor::visit(core::settings::BoolSetting& setting) {
  bool value = setting.value();
  if (ImGui::Checkbox(setting.name().c_str(), &value)) {
    setting.setValue(value);
    changed_ = true;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", setting.description().c_str());
}

void SettingEditor::visit(core::settings::IntSetting& setting) {
  int value = setting.value();
  const int minimum = setting.hasRange() ? setting.minimum() : 0;
  const int maximum = setting.hasRange() ? setting.maximum() : 100;
  if (ImGui::SliderInt(setting.name().c_str(), &value, minimum, maximum)) {
    setting.setValue(value);
    changed_ = true;
  }
}

void SettingEditor::visit(core::settings::FloatSetting& setting) {
  float value = setting.value();
  const float minimum = setting.hasRange() ? setting.minimum() : 0.0f;
  const float maximum = setting.hasRange() ? setting.maximum() : 1.0f;
  if (ImGui::SliderFloat(setting.name().c_str(), &value, minimum, maximum,
                         "%.2f")) {
    setting.setValue(value);
    changed_ = true;
  }
}

void SettingEditor::visit(core::settings::ColorSetting& setting) {
  core::settings::Color value = setting.value();
  float rgba[4] = {value.r, value.g, value.b, value.a};
  if (ImGui::ColorEdit4(setting.name().c_str(), rgba,
                        ImGuiColorEditFlags_AlphaBar |
                            ImGuiColorEditFlags_NoInputs)) {
    setting.setValue(core::settings::Color{rgba[0], rgba[1], rgba[2], rgba[3]});
    changed_ = true;
  }
}

void SettingEditor::visit(core::settings::EnumSetting& setting) {
  const std::vector<std::string>& options = setting.options();
  std::vector<const char*> labels;
  labels.reserve(options.size());
  for (const std::string& option : options) labels.push_back(option.c_str());

  int current = static_cast<int>(setting.value());
  if (!options.empty() && ImGui::Combo(setting.name().c_str(), &current,
                                       labels.data(),
                                       static_cast<int>(labels.size()))) {
    setting.setValue(static_cast<std::size_t>(current));
    changed_ = true;
  }
}

void SettingEditor::visit(core::settings::KeybindSetting& setting) {
  const bool armed = armedKeybind_ == &setting;
  const std::string label =
      armed ? "Presiona una tecla..." : setting.value().label();
  if (ImGui::Button((setting.name() + ": " + label).c_str(),
                    ImVec2(-1.0f, 0.0f))) {
    armedKeybind_ = armed ? nullptr : &setting;
  }
}

}  // namespace lex::platform::windows::gui
