// LEX CLIENT — Hooks: LLM teclado + TGA passthrough de mouse.
#include "hooks.h"
#include "engine.h"
#include <windowsx.h>

namespace lex {
namespace engine {

namespace {

LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = OverlayEngine::instance();
  if (nCode >= 0 && self) {
    if (wParam == WM_LBUTTONDOWN) {
      self->registerLocalClick();
    }
  }
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK keyProc(int nCode, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = OverlayEngine::instance();
  if (nCode >= 0 && self) {
    const KBDLLHOOKSTRUCT* ks = (const KBDLLHOOKSTRUCT*)lParam;
    if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && ks) {
      // Capturamos keystroke, NO bloqueamos: CallNextHookEx después.
      self->registerKeystroke(ks->vkCode);
    }
  }
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}  // namespace

HookHandles installHooks() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  HookHandles h;
  h.mouse = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, hInstance, 0);
  h.keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyProc, hInstance, 0);
  return h;
}

void uninstallHooks(const HookHandles& handles) {
  if (handles.mouse) UnhookWindowsHookEx(handles.mouse);
  if (handles.keyboard) UnhookWindowsHookEx(handles.keyboard);
}

}  // namespace engine
}  // namespace lex
