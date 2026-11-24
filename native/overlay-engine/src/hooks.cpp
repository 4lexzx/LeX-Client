// LEX CLIENT — Hooks: LLM teclado + TGA passthrough de mouse.
#include "hooks.h"
#include "engine.h"
#include <windowsx.h>

namespace lex {
namespace engine {

namespace {

// El hook LLM no expone el bit de repeat; lo filtramos por transición de estado,
// igual que hace Flarial al ignorar WM_KEYDOWN repetidos en su WndProc.
bool llKeyDown[256] = {};

LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = OverlayEngine::instance();
  if (nCode >= 0 && self) {
    if (wParam == WM_LBUTTONDOWN) {
      self->registerKeyState(VK_LBUTTON, true);
      self->registerLocalClick(VK_LBUTTON);
    } else if (wParam == WM_LBUTTONUP) {
      self->registerKeyState(VK_LBUTTON, false);
    } else if (wParam == WM_RBUTTONDOWN) {
      self->registerKeyState(VK_RBUTTON, true);
      self->registerLocalClick(VK_RBUTTON);
    } else if (wParam == WM_RBUTTONUP) {
      self->registerKeyState(VK_RBUTTON, false);
    }
  }
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK keyProc(int nCode, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = OverlayEngine::instance();
  if (nCode >= 0 && self) {
    const KBDLLHOOKSTRUCT* ks = (const KBDLLHOOKSTRUCT*)lParam;
    if (ks) {
      const int vk = (int)ks->vkCode;
      if (vk >= 0 && vk < 256) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
          const bool freshPress = !llKeyDown[vk];
          llKeyDown[vk] = true;
          self->registerKeyState(vk, true);
          // Decidimos ANTES de procesar: registerKeystroke puede cerrar el menú
          // y entonces consumeKey ya devolvería false (p. ej. con ESC).
          const bool block = self->consumeKey(vk);
          // Un repeat del SO no es una pulsación nueva: no entra en keystrokes
          // ni puede volver a abrir/cerrar el menú.
          if (freshPress) self->registerKeystroke(vk);
          if (block) return 1;  // consumida: no llega al juego ni a otras apps
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
          llKeyDown[vk] = false;
          self->registerKeyState(vk, false);
          if (self->consumeKey(vk)) return 1;
        }
      }
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
