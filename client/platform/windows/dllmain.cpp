#include "platform/windows/Exports.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "core/ClientContext.h"
#include "core/gui/ClickGuiModel.h"
#include "core/module/ModuleManager.h"
#include "core/util/Logger.h"
#include "platform/windows/WindowsPlatform.h"
#include "platform/windows/resources/IconResource.h"
#include "platform/windows/gui/ClickGuiRenderer.h"
#include "platform/windows/gui/IconAtlas.h"
#include "platform/windows/gui/ImGuiLayer.h"
#include "platform/windows/game/Cosmetics.h"
#include "platform/windows/game/ItemPhysics.h"
#include "platform/windows/game/Memory.h"
#include "platform/windows/game/Telemetry.h"
#include "platform/windows/gui/TextureStore.h"
#include "platform/windows/hooks/DxgiHook.h"
#include "platform/windows/hooks/QueueHook.h"
#include "platform/windows/hud/HudModule.h"
#include "platform/windows/input/Input.h"

#include <MinHook.h>
#include <thread>

extern "C" LEX_API bool lexInitialize(const char*, const char*);

namespace {

HMODULE g_module = nullptr;
lex::platform::windows::WindowsPlatform g_platform;
lex::core::ClientContext g_context;
lex::core::gui::ClickGuiModel g_clickGui(
    lex::core::module::ModuleManager::instance());

std::filesystem::path defaultConfigDirectory() {
  const char* appData = std::getenv("APPDATA");
  if (appData != nullptr && *appData != '\0') {
    return std::filesystem::path(appData) / "lex-client";
  }
  return std::filesystem::path("config");
}

std::filesystem::file_time_type g_lastWrite{};

std::filesystem::file_time_type configTime() {
  std::error_code error;
  return std::filesystem::last_write_time(g_context.config().currentPath(), error);
}

void saveAll() {
  auto& config = g_context.config();
  lex::core::module::ModuleManager::instance().saveToConfig(config);
  config.root()["clickgui"] = g_clickGui.toJson();
  config.root()["clickgui"]["keyL"] = true;
  config.markDirty();
  config.save();
  g_lastWrite = configTime();
}

// El launcher escribe el mismo archivo: los cambios se aplican en caliente.
void reloadIfChanged(float delta) {
  static float clock = 0.0f;
  clock += delta;
  if (clock < 1.0f) return;
  clock = 0.0f;
  const auto time = configTime();
  if (time == g_lastWrite) return;
  g_lastWrite = time;
  if (!g_context.config().load()) return;
  lex::core::module::ModuleManager::instance().loadFromConfig(g_context.config());
}

bool hostIsMinecraft() {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return _wcsicmp(std::filesystem::path(path).filename().c_str(), L"Minecraft.Windows.exe") == 0;
}

DWORD WINAPI autoStart(LPVOID) {
  lexInitialize(nullptr, nullptr);
  return 0;
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
      (profile != nullptr && *profile != '\0') ? profile : "client";

  lex::platform::windows::hud::registerAllModules();
  std::error_code error;
  std::filesystem::create_directories(path, error);
  g_context.initialize(g_platform, path, selectedProfile);
  g_lastWrite = configTime();

  auto& icons = lex::platform::windows::gui::IconAtlas::instance();
  icons.setModule(g_module);
  icons.loadFromResources();

  auto& textures = lex::platform::windows::gui::TextureStore::instance();
  if (HRSRC found = FindResourceW(g_module, MAKEINTRESOURCEW(IDR_LOGO), MAKEINTRESOURCEW(10))) {
    if (SizeofResource(g_module, found) == 256 * 256 * 4) {
      const auto* big = static_cast<const std::uint8_t*>(LockResource(LoadResource(g_module, found)));
      textures.add("logo", 256, 256, big);
      // Version pequeña ya reducida: a 20 px la grande se veria dentada.
      std::vector<std::uint8_t> small(64 * 64 * 4);
      for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
          int sum[4] = {};
          for (int k = 0; k < 16; ++k) {
            const std::uint8_t* p = big + ((y * 4 + k / 4) * 256 + x * 4 + k % 4) * 4;
            for (int c = 0; c < 3; ++c) sum[c] += p[c] * p[3];
            sum[3] += p[3];
          }
          std::uint8_t* out = &small[static_cast<std::size_t>(y * 64 + x) * 4];
          for (int c = 0; c < 3; ++c) out[c] = static_cast<std::uint8_t>(sum[3] > 0 ? sum[c] / sum[3] : 0);
          out[3] = static_cast<std::uint8_t>(sum[3] / 16);
        }
      }
      textures.add("logo.small", 64, 64, small.data());
    }
  }
  auto& cosmetics = lex::platform::windows::game::Cosmetics::instance();
  cosmetics.load(path);
  {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> rgba;
    if (lex::platform::windows::game::Cosmetics::readImage(path / "skin.png", w, h, rgba) && w == h) textures.add("skin", w, h, rgba.data());
    std::error_code ignored;
    for (const auto& file : std::filesystem::directory_iterator(path / "items", ignored)) {
      if (file.path().extension() != ".png") continue;
      if (lex::platform::windows::game::Cosmetics::readImage(file.path(), w, h, rgba)) {
        // Los iconos son pixel art: se amplian sin difuminar.
        const int k = std::max(1, 64 / std::max(1, w));
        std::vector<std::uint8_t> big(static_cast<std::size_t>(w) * h * k * k * 4);
        for (int y = 0; y < h * k; ++y) {
          for (int x = 0; x < w * k; ++x) std::memcpy(&big[(static_cast<std::size_t>(y) * w * k + x) * 4], &rgba[(static_cast<std::size_t>(y / k) * w + x / k) * 4], 4);
        }
        textures.add("item." + file.path().stem().string(), w * k, h * k, big.data());
      }
    }
  }
  if (hostIsMinecraft()) lex::platform::windows::game::Options::instance().start();
  if (hostIsMinecraft()) lex::platform::windows::Input::instance().startClickCounter();
  if (hostIsMinecraft()) std::thread([]() { lex::platform::windows::game::ItemPhysics::install(); }).detach();
  if (hostIsMinecraft()) lex::platform::windows::mouse_guard::startGameInputGuard();

  nlohmann::json& root = g_context.config().root();
  if (root.is_object() && root.contains("clickgui")) {
    g_clickGui.fromJson(root["clickgui"]);
    g_clickGui.setOpen(false);
    // Las versiones anteriores usaban Shift derecho: ahora es L, una sola vez.
    if (!root["clickgui"].value("keyL", false)) g_clickGui.theme().menuKey = lex::core::gui::GuiTheme::defaultKey();
    g_clickGui.setSearch("");
  }

  auto& imgui = lex::platform::windows::gui::ImGuiLayer::instance();
  imgui.setModel(&g_clickGui);
  imgui.setPlatform(&g_platform);
  imgui.setFrameCallback([](float delta) {
    g_platform.onFrame(delta);
    lex::platform::windows::game::Cosmetics::instance().update(delta);
    const bool pending = g_context.config().dirty();
    g_context.tick(delta);
    // El autoguardado propio no debe tratarse como un cambio del launcher.
    if (pending && !g_context.config().dirty()) g_lastWrite = configTime();
    // Guarda al cerrar el menu: es el momento en que cambian los ajustes.
    static bool wasOpen = false;
    const bool open = g_clickGui.isOpen();
    if (wasOpen && !open) saveAll();
    wasOpen = open;
    if (!open) reloadIfChanged(delta);
  });
  MH_Initialize();
  if (!lex::platform::windows::mouse_guard::install()) {
    LEX_LOG_WARNING("No se pudo proteger el raton; el menu puede mover la camara");
  }
  lex::platform::windows::hooks::installQueueHook();
  lex::platform::windows::hooks::DxgiHook::instance().install(&imgui);

  return g_context.initialized();
}

extern "C" LEX_API void lexTick(float deltaSeconds) {
  g_platform.onFrame(deltaSeconds);
  g_context.tick(deltaSeconds);
}

extern "C" LEX_API void lexShutdown() {
  lex::platform::windows::hooks::DxgiHook::instance().shutdown();
  lex::platform::windows::gui::ImGuiLayer::instance().shutdown();
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();

  nlohmann::json& root = g_context.config().root();
  if (root.is_object()) {
    root["clickgui"] = g_clickGui.toJson();
    g_context.config().markDirty();
  }
  g_context.shutdown();
}

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

extern "C" LEX_API void lexToggleGui() { g_clickGui.toggle(); }

extern "C" LEX_API void lexEditHud(bool on) {
  lex::platform::windows::gui::ClickGuiRenderer::instance().setHudEditing(on);
}

extern "C" LEX_API void lexSelectModule(const char* name) { g_clickGui.selectModule(name); }

extern "C" LEX_API void lexSelectTab(int tab) {
  g_clickGui.setTab(static_cast<lex::core::gui::ClickGuiModel::Tab>(tab));
}

extern "C" LEX_API bool lexGuiOpen() { return g_clickGui.isOpen(); }

extern "C" LEX_API int lexHookPresentCalls() {
  return lex::platform::windows::hooks::DxgiHook::instance().presentCallCount();
}

extern "C" LEX_API const char* lexVersion() { return "0.5.0"; }

extern "C" LEX_API float lexFps() { return g_platform.fps(); }

extern "C" LEX_API int lexIconCount() {
  return lex::platform::windows::gui::IconAtlas::instance().loadedCount();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (hostIsMinecraft()) {
      HANDLE thread = CreateThread(nullptr, 0, &autoStart, nullptr, 0, nullptr);
      if (thread != nullptr) CloseHandle(thread);
    }
  }
  return TRUE;
}
