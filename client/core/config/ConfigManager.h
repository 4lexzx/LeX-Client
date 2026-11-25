#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace lex::core::config {

// Guarda perfiles en <baseDirectory>/<profile>.json.
// No usa Windows.h: la ruta la suministra la plataforma.
class ConfigManager {
 public:
  explicit ConfigManager(std::filesystem::path baseDirectory = "config");

  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  void setBaseDirectory(std::filesystem::path path);
  const std::filesystem::path& baseDirectory() const noexcept {
    return baseDirectory_;
  }

  void setProfile(std::string profile);
  const std::string& profile() const noexcept { return profile_; }

  std::vector<std::string> availableProfiles() const;
  std::filesystem::path currentPath() const;

  nlohmann::json& root() noexcept { return root_; }
  const nlohmann::json& root() const noexcept { return root_; }

  nlohmann::json* findModule(std::string_view moduleName);
  const nlohmann::json* findModule(std::string_view moduleName) const;

  bool load();
  bool save() const;

  void markDirty() noexcept { dirty_ = true; }
  void markSaved() noexcept { dirty_ = false; }
  bool dirty() const noexcept { return dirty_; }

  void setAutosaveSeconds(float seconds) noexcept { autosaveSeconds_ = seconds; }
  float autosaveSeconds() const noexcept { return autosaveSeconds_; }

 private:
  std::filesystem::path baseDirectory_;
  std::string profile_ = "default";
  nlohmann::json root_ = nlohmann::json::object();
  bool dirty_ = false;
  float autosaveSeconds_ = 30.0f;
};

}  // namespace lex::core::config
