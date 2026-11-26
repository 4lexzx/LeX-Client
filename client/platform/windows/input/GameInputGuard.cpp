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
constexpr std::uint32_t kKeyboard = 0x10;
constexpr int kGetCurrentTimestamp = 3;  // IGameInput
constexpr int kGetCurrentReading = 4;
constexpr int kGetTimestamp = 4;  // IGameInputReading
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
std::atomic<IUnknown*> g_input{nullptr};
// Momento (en el reloj de GameInput) en que se cerro el menu: las lecturas
// anteriores se guardaron con el menu abierto y no deben llegar al juego aunque
// las procese despues (asi Esc no cierra tambien el menu del juego).
std::atomic<std::uint64_t> g_closedAt{0};
std::atomic<bool> g_wasOpen{false};

std::uint64_t timestamp(void* reading) {
  return reinterpret_cast<std::uint64_t(STDMETHODCALLTYPE*)(void*)>(vtable(reading)[kGetTimestamp])(reading);
}

// Hay que ignorar esta lectura: menu abierto o tomada antes de cerrarlo.
bool blocked(void* reading) {
  const bool open = menuOpen();
  if (g_wasOpen.exchange(open) && !open) {
    IUnknown* input = g_input.load();
    if (input != nullptr) g_closedAt.store(reinterpret_cast<std::uint64_t(STDMETHODCALLTYPE*)(void*)>(vtable(input)[kGetCurrentTimestamp])(input));
  }
  return open || timestamp(reading) <= g_closedAt.load();
}
KeyCountFn g_keyCount = nullptr;
KeyStateFn g_keyState = nullptr;
// Las lecturas de teclado pueden ser de otra clase: sus propios originales.
KeyCountFn g_keyCount2 = nullptr;
KeyStateFn g_keyState2 = nullptr;

bool STDMETHODCALLTYPE mouseDetour(void* self, MouseState* state) {
  const bool ok = g_mouse(self, state);
  if (!ok || state == nullptr) return ok;
  void* device = nullptr;
  reinterpret_cast<void(STDMETHODCALLTYPE*)(void*, void**)>(vtable(self)[kGetDevice])(self, &device);
  if (device != nullptr) static_cast<IUnknown*>(device)->Release();
  const MouseState real = *state;
  const bool stale = blocked(self);
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
  if (stale) state->buttons = 0;
  state->x -= f.x;
  state->y -= f.y;
  state->wheelX -= f.wheelX;
  state->wheelY -= f.wheelY;
  return ok;
}

std::uint32_t STDMETHODCALLTYPE keyCountDetour(void* self) { return blocked(self) ? 0 : g_keyCount(self); }
std::uint32_t STDMETHODCALLTYPE keyStateDetour(void* self, std::uint32_t count, void* keys) { return blocked(self) ? 0 : g_keyState(self, count, keys); }
std::uint32_t STDMETHODCALLTYPE keyCountDetour2(void* self) { return blocked(self) ? 0 : g_keyCount2(self); }
std::uint32_t STDMETHODCALLTYPE keyStateDetour2(void* self, std::uint32_t count, void* keys) { return blocked(self) ? 0 : g_keyState2(self, count, keys); }

// Engancha tambien la clase de las lecturas de teclado si no es la del raton.
// Hace falta una lectura de teclado: llega con la primera tecla que se pulse.
// Devuelve 0 si aun no hay, 1 si es la misma clase, 2 si se engancho aparte.
bool hook(void* target, void* detour, void** original);
int tryKeyboard(IUnknown* input, void* mouseKeyState) {
  IUnknown* reading = nullptr;
  using Current = HRESULT(STDMETHODCALLTYPE*)(void*, std::uint32_t, void*, IUnknown**);
  if (FAILED(reinterpret_cast<Current>(vtable(input)[kGetCurrentReading])(input, kKeyboard, nullptr, &reading)) || reading == nullptr) return 0;
  void** table = vtable(reading);
  int result = 1;
  if (table[kGetKeyState] != mouseKeyState) {
    hook(table[kGetKeyCount], reinterpret_cast<void*>(&keyCountDetour2), reinterpret_cast<void**>(&g_keyCount2));
    hook(table[kGetKeyState], reinterpret_cast<void*>(&keyStateDetour2), reinterpret_cast<void**>(&g_keyState2));
    result = 2;
  }
  reading->Release();
  return result;
}
void* g_mouseKeyState = nullptr;

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
    g_mouseKeyState = table[kGetKeyState];
    hook(table[kGetKeyState], reinterpret_cast<void*>(&keyStateDetour), reinterpret_cast<void**>(&g_keyState));
  }
  reading->Release();
  if (done) g_input.store(input);
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
        for (int k = 0; k < 7200; ++k) {
          const int keyboard = tryKeyboard(input, g_mouseKeyState);
          if (keyboard == 1) LEX_LOG_INFO("GameInput: el teclado usa la misma clase que el raton");
          if (keyboard == 2) LEX_LOG_INFO("GameInput: teclado protegido aparte");
          if (keyboard != 0) return;
          Sleep(500);
        }
        return;
      }
    }
    LEX_LOG_WARNING("GameInput no disponible");
  }).detach();
}

}  // namespace lex::platform::windows::mouse_guard
