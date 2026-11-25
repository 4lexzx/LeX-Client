#include <windows.h>
#include <unknwn.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>

#include "core/util/Logger.h"
#include "platform/windows/input/Input.h"

namespace lex::platform::windows::mouse_guard {
namespace {

// GameInput v2 (la version que usa el juego). Solo se declara lo necesario.
constexpr GUID kGameInputV2 = {0xBBAA66D2, 0x837A, 0x40F7, {0xA3, 0x03, 0x91, 0x7D, 0x50, 0x09, 0x55, 0xF4}};
constexpr std::uint32_t kMouse = 0x20;
constexpr int kGetCurrentReading = 4;
constexpr int kGetDevice = 5;
constexpr int kGetKeyCount = 12;
constexpr int kGetKeyState = 13;
constexpr int kGetMouseState = 14;

struct MouseState {
  std::uint32_t buttons, positions;
  std::int64_t x, y, absoluteX, absoluteY, wheelX, wheelY;
};

// Los contadores son acumulados: lo movido con el menu abierto se descuenta
// para siempre, asi la camara no salta al cerrarlo.
struct Freeze {
  bool held = false;
  MouseState at{};
  std::int64_t x = 0, y = 0, wheelX = 0, wheelY = 0;
};
std::mutex g_lock;
std::map<void*, Freeze> g_devices;

void** vtable(void* object) { return *reinterpret_cast<void***>(object); }

using MouseFn = bool(STDMETHODCALLTYPE*)(void*, MouseState*);
using KeyCountFn = std::uint32_t(STDMETHODCALLTYPE*)(void*);
using KeyStateFn = std::uint32_t(STDMETHODCALLTYPE*)(void*, std::uint32_t, void*);
MouseFn g_mouse = nullptr;
KeyCountFn g_keyCount = nullptr;
KeyStateFn g_keyState = nullptr;

bool STDMETHODCALLTYPE mouseDetour(void* self, MouseState* state) {
  const bool ok = g_mouse(self, state);
  if (!ok || state == nullptr) return ok;
  void* device = nullptr;
  reinterpret_cast<void(STDMETHODCALLTYPE*)(void*, void**)>(vtable(self)[kGetDevice])(self, &device);
  if (device != nullptr) static_cast<IUnknown*>(device)->Release();
  const MouseState real = *state;
  std::lock_guard<std::mutex> lock(g_lock);
  Freeze& f = g_devices[device];
  if (menuOpen()) {
    if (!f.held) {
      f.held = true;
      f.at = real;
      f.at.x -= f.x;
      f.at.y -= f.y;
      f.at.wheelX -= f.wheelX;
      f.at.wheelY -= f.wheelY;
    }
    f.x = real.x - f.at.x;
    f.y = real.y - f.at.y;
    f.wheelX = real.wheelX - f.at.wheelX;
    f.wheelY = real.wheelY - f.at.wheelY;
    *state = f.at;
    state->buttons = 0;
    return ok;
  }
  f.held = false;
  state->x -= f.x;
  state->y -= f.y;
  state->wheelX -= f.wheelX;
  state->wheelY -= f.wheelY;
  return ok;
}

std::uint32_t STDMETHODCALLTYPE keyCountDetour(void* self) { return menuOpen() ? 0 : g_keyCount(self); }
std::uint32_t STDMETHODCALLTYPE keyStateDetour(void* self, std::uint32_t count, void* keys) { return menuOpen() ? 0 : g_keyState(self, count, keys); }

bool hook(void* target, void* detour, void** original) {
  return MH_CreateHook(target, detour, original) == MH_OK && MH_EnableHook(target) == MH_OK;
}

bool tryInstall(IUnknown*& input) {
  const HMODULE module = GetModuleHandleW(L"GameInputRedist.dll");
  if (module == nullptr) return false;
  if (input == nullptr) {
    using Init = HRESULT(WINAPI*)(REFIID, void**);
    const auto init = reinterpret_cast<Init>(reinterpret_cast<void*>(GetProcAddress(module, "GameInputInitialize")));
    if (init == nullptr || FAILED(init(kGameInputV2, reinterpret_cast<void**>(&input))) || input == nullptr) return false;
  }
  IUnknown* reading = nullptr;
  using Current = HRESULT(STDMETHODCALLTYPE*)(void*, std::uint32_t, void*, IUnknown**);
  if (FAILED(reinterpret_cast<Current>(vtable(input)[kGetCurrentReading])(input, kMouse, nullptr, &reading)) || reading == nullptr) return false;
  void** table = vtable(reading);
  // Se comprueba la forma antes de enganchar: GetMouseState debe aceptar la lectura.
  MouseState probe{};
  const bool valid = reinterpret_cast<MouseFn>(table[kGetMouseState])(reading, &probe);
  bool done = false;
  if (valid) {
    done = hook(table[kGetMouseState], reinterpret_cast<void*>(&mouseDetour), reinterpret_cast<void**>(&g_mouse));
    hook(table[kGetKeyCount], reinterpret_cast<void*>(&keyCountDetour), reinterpret_cast<void**>(&g_keyCount));
    hook(table[kGetKeyState], reinterpret_cast<void*>(&keyStateDetour), reinterpret_cast<void**>(&g_keyState));
  }
  reading->Release();
  return done;
}

}  // namespace

void startGameInputGuard() {
  std::thread([]() {
    IUnknown* input = nullptr;
    for (int i = 0; i < 1200; ++i) {
      Sleep(500);
      if (tryInstall(input)) {
        LEX_LOG_INFO("GameInput protegido");
        return;
      }
    }
    LEX_LOG_WARNING("GameInput no disponible");
  }).detach();
}

}  // namespace lex::platform::windows::mouse_guard
