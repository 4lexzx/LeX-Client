#include "platform/windows/Exports.h"

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "core/ClientContext.h"
#include "core/module/ModuleManager.h"
#include "core/util/Logger.h"
#include "platform/windows/WindowsPlatform.h"

namespace {

lex::platform::windows::WindowsPlatform g_platform;
lex::core::ClientContext g_context;

std::filesystem::path defaultConfigDirectory() {
  const char* appData = std::getenv("APPDATA");
  if (appData != nullptr && *appData != '\0') {
    return std::filesystem::path(appData) / "LeX-Client";
  }
  return std::filesystem::path("config");
}

}  // namespace

extern "C" LEX_API bool lexInitialize(const char* configDirectory,
                                      const char* profile) {
  if (g_context.initialized()) return true;

  const std::filesystem::path path =
      (configDirectory != nullptr && *configDirectory != '\0')
          ? std::filesystem::path(configDirectory)
          : defaultConfigDirectory();

  const std::string selectedProfile =
      (profile != nullptr && *profile != '\0') ? profile : "default";

  g_context.initialize(g_platform, path, selectedProfile);
  return g_context.initialized();
}

extern "C" LEX_API void lexTick(float deltaSeconds) {
  g_platform.onFrame(deltaSeconds);
  g_context.tick(deltaSeconds);
}

extern "C" LEX_API void lexShutdown() { g_context.shutdown(); }

extern "C" LEX_API int lexModuleCount() {
  return static_cast<int>(lex::core::module::ModuleManager::instance().count());
}

extern "C" LEX_API bool lexSetModuleEnabled(const char* name, bool enabled) {
  if (name == nullptr) return false;
  lex::core::module::Module* module =
      lex::core::module::ModuleManager::instance().find(name);
  if (module == nullptr) return false;
  module->setEnabled(enabled);
  return true;
}

extern "C" LEX_API const char* lexVersion() { return "0.1.0"; }

extern "C" LEX_API float lexFps() { return g_platform.fps(); }

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
  return TRUE;
}
