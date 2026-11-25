#include "platform/windows/input/Input.h"

#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <thread>

namespace lex::platform::windows {

namespace {
std::atomic<HWND> g_window{nullptr};
std::atomic<bool> g_hooked{false};
}  // namespace

Input& Input::instance() {
  static Input input;
  return input;
}

void Input::sample(HWND window) {
  focused_ = window != nullptr && GetForegroundWindow() == window;
  before_ = now_;
  CURSORINFO cursor{};
  cursor.cbSize = sizeof(cursor);
  gameplay_ = focused_ && GetCursorInfo(&cursor) && ((cursor.flags & CURSOR_SHOWING) == 0 || cursor.hCursor == nullptr);
  for (int key = 1; key < 256; ++key) {
    now_[key] = focused_ && (GetAsyncKeyState(key) & 0x8000) != 0;
  }

  g_window.store(window);
  const ULONGLONG time = GetTickCount64();
  std::lock_guard<std::mutex> lock(clicks_);
  if (!g_hooked.load() && !mouse_guard::menuOpen()) {
    if (pressed(VK_LBUTTON)) note(true, time);
    if (pressed(VK_RBUTTON)) note(false, time);
  }
  while (!left_.empty() && time - left_.front() >= 1000) left_.pop_front();
  while (!right_.empty() && time - right_.front() >= 1000) right_.pop_front();
}

void Input::note(bool left, ULONGLONG time) {
  (left ? left_ : right_).push_back(time);
  if (left) lastLeft_ = time;
  ++total_;
}

namespace {

LRESULT CALLBACK mouseHook(int code, WPARAM w, LPARAM l) {
  if (code == HC_ACTION && (w == WM_LBUTTONDOWN || w == WM_RBUTTONDOWN)) {
    Input::instance().hookClick(w == WM_LBUTTONDOWN);
  }
  return CallNextHookEx(nullptr, code, w, l);
}

}  // namespace

void Input::hookClick(bool left) {
  const HWND window = g_window.load();
  if (window == nullptr || GetForegroundWindow() != window || mouse_guard::menuOpen()) return;
  std::lock_guard<std::mutex> lock(clicks_);
  note(left, GetTickCount64());
}

void Input::startClickCounter() {
  std::thread([]() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&mouseHook), &module);
    const HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, &mouseHook, module, 0);
    if (hook == nullptr) return;
    g_hooked.store(true);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) DispatchMessageW(&message);
    UnhookWindowsHookEx(hook);
  }).detach();
}

namespace mouse_guard {
namespace {

std::atomic<bool> g_open{false};
RECT g_savedClip{};
bool g_hasSavedClip = false;

BOOL(WINAPI* g_clipCursor)(const RECT*) = nullptr;
BOOL(WINAPI* g_setCursorPos)(int, int) = nullptr;
BOOL(WINAPI* g_getCursorPos)(LPPOINT) = nullptr;
POINT g_frozen{};
UINT(WINAPI* g_getRawInputData)(HRAWINPUT, UINT, LPVOID, PUINT, UINT) = nullptr;

BOOL WINAPI clipCursorDetour(const RECT* rect) {
  if (g_open.load()) return TRUE;
  return g_clipCursor(rect);
}

BOOL WINAPI setCursorPosDetour(int x, int y) {
  if (g_open.load()) return TRUE;
  return g_setCursorPos(x, y);
}

// El juego ve el cursor quieto mientras el menu esta abierto; LeX ve el real.
BOOL WINAPI getCursorPosDetour(LPPOINT point) {
  if (g_open.load() && point != nullptr) {
    static const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    static const std::uintptr_t size = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew)->OptionalHeader.SizeOfImage;
    const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    if (caller >= base && caller < base + size) {
      *point = g_frozen;
      return TRUE;
    }
  }
  return g_getCursorPos(point);
}

UINT WINAPI rawInputDetour(HRAWINPUT input, UINT command, LPVOID data,
                           PUINT size, UINT headerSize) {
  const UINT result = g_getRawInputData(input, command, data, size, headerSize);
  if (g_open.load() && data != nullptr && command == RID_INPUT &&
      result != static_cast<UINT>(-1) && result >= sizeof(RAWINPUTHEADER)) {
    auto* raw = static_cast<RAWINPUT*>(data);
    // Se conserva la cabecera valida, pero el movimiento/teclas van a cero.
    if (raw->header.dwType == RIM_TYPEMOUSE) raw->data.mouse = RAWMOUSE{};
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
      raw->data.keyboard = RAWKEYBOARD{};
    }
  }
  return result;
}

template <typename T>
bool hookExport(const char* name, void* detour, T* original) {
  return MH_CreateHookApi(L"user32", name, detour,
                          reinterpret_cast<void**>(original)) == MH_OK;
}

}  // namespace

bool install() {
  const bool clip = hookExport("ClipCursor", reinterpret_cast<void*>(&clipCursorDetour), &g_clipCursor);
  const bool pos = hookExport("SetCursorPos", reinterpret_cast<void*>(&setCursorPosDetour), &g_setCursorPos);
  const bool raw = hookExport("GetRawInputData", reinterpret_cast<void*>(&rawInputDetour), &g_getRawInputData);
  hookExport("GetCursorPos", reinterpret_cast<void*>(&getCursorPosDetour), &g_getCursorPos);
  return MH_EnableHook(MH_ALL_HOOKS) == MH_OK && clip && pos && raw;
}

bool menuOpen() { return g_open.load(); }

void setMenuOpen(bool open) {
  if (g_open.load() == open) return;
  if (open) {
    g_hasSavedClip = GetClipCursor(&g_savedClip) != FALSE;
    if (g_clipCursor != nullptr) g_clipCursor(nullptr);
    GetCursorPos(&g_frozen);
    ReleaseCapture();
    g_open.store(true);
  } else {
    g_open.store(false);
    if (g_hasSavedClip && g_clipCursor != nullptr) g_clipCursor(&g_savedClip);
  }
}

}  // namespace mouse_guard
}  // namespace lex::platform::windows
