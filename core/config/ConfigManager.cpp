#include "core/config/ConfigManager.h"

#include <fstream>
#include <system_error>

#include "core/util/Logger.h"

namespace lex::core::config {

ConfigManager::ConfigManager(std::filesystem::path baseDirectory)
    : baseDirectory_(std::move(baseDirectory)) {}

void ConfigManager::setBaseDirectory(std::filesystem::path path) {
  baseDirectory_ = std::move(path);
  dirty_ = true;
}

void ConfigManager::setProfile(std::string profile) {
  if (profile == profile_) return;
  profile_ = std::move(profile);
  dirty_ = true;
}

std::vector<std::string> ConfigManager::availableProfiles() const {
  std::vector<std::string> profiles;
  std::error_code error;
  if (!std::filesystem::exists(baseDirectory_, error)) return profiles;

  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(baseDirectory_, error)) {
    if (error) break;
    if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
      profiles.push_back(entry.path().stem().string());
    }
  }
  return profiles;
}

std::filesystem::path ConfigManager::currentPath() const {
  return baseDirectory_ / (profile_ + ".json");
}

nlohmann::json* ConfigManager::findModule(std::string_view moduleName) {
  if (!root_.is_object() || !root_.contains("modules")) return nullptr;
  nlohmann::json& modules = root_["modules"];
  if (!modules.is_object()) return nullptr;

  auto it = modules.find(std::string(moduleName));
  return it == modules.end() ? nullptr : &(*it);
}

const nlohmann::json* ConfigManager::findModule(
    std::string_view moduleName) const {
  if (!root_.is_object() || !root_.contains("modules")) return nullptr;
  const nlohmann::json& modules = root_["modules"];
  if (!modules.is_object()) return nullptr;

  auto it = modules.find(std::string(moduleName));
  return it == modules.end() ? nullptr : &(*it);
}

bool ConfigManager::load() {
  const std::filesystem::path path = currentPath();
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    root_ = nlohmann::json{{"version", 1},
                           {"profile", profile_},
                           {"modules", nlohmann::json::object()}};
    LEX_LOG_INFO("No existe config, se usa perfil por defecto: " + path.string());
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    LEX_LOG_ERROR("No se pudo abrir config: " + path.string());
    return false;
  }

  try {
    nlohmann::json parsed;
    file >> parsed;
    if (!parsed.is_object()) throw std::runtime_error("JSON raiz no es objeto");
    root_ = std::move(parsed);
  } catch (const std::exception& exception) {
    LEX_LOG_ERROR("Config corrupta '" + path.string() + "': " + exception.what());
    return false;
  }

  dirty_ = false;
  LEX_LOG_INFO("Config cargada: " + path.string());
  return true;
}

bool ConfigManager::save() const {
  if (!dirty()) return true;

  const std::filesystem::path path = currentPath();
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);

  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    LEX_LOG_ERROR("No se pudo guardar config: " + path.string());
    return false;
  }

  file << root_.dump(2) << '\n';
  const_cast<ConfigManager*>(this)->markSaved();
  LEX_LOG_INFO("Config guardada: " + path.string());
  return true;
}

}  // namespace lex::core::config
