// LEX CLIENT — Overlay Engine: punto de entrada principal (Win32 + Direct2D).
// - Busca la ventana de Minecraft for Windows.
// - Lee %APPDATA%\lex-client\overlay-state.json.
// - Instala hooks (LLM/TGA).
// - Render loop propio (FPS del overlay).
// - Mini-HUD transparente: FPS, CPS, keystrokes.

#include "engine.h"
#include "hooks.h"
#include "bridge.h"
#include <windows.h>
#include <cwchar>

static BOOL CALLBACK enumMinecraftWindow(HWND hwnd, LPARAM lParam) {
  wchar_t title[512] = {0};
  const int len = GetWindowTextW(hwnd, title, 512);
  if (len > 0 && IsWindowVisible(hwnd) && wcsstr(title, L"Minecraft") != nullptr) {
    *reinterpret_cast<HWND*>(lParam) = hwnd;
    return FALSE;
  }
  return TRUE;
}

static HWND findMinecraftWindow() {
  HWND found = nullptr;
  EnumWindows(enumMinecraftWindow, reinterpret_cast<LPARAM>(&found));
  return found;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
  (void)AllocConsole();
  (void)SetConsoleTitleA("LEX Client Overlay Engine");

  HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"lex-overlay-engine");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // Ya está corriendo.
    return 0;
  }

  lex::engine::OverlayEngine engine;
  (void)engine.stats();
  (void)nCmdShow;

  HWND hwnd = findMinecraftWindow();
  if (!hwnd) {
    fprintf(stderr, "[LEX] no se encontró ventana de Minecraft\n");
    // No hay juego: salimos con error 1 para el launcher.
    return 1;
  }
  fprintf(stderr, "[LEX] Minecraft hwnd=%p\n", (void*)hwnd);

  // Hooks: teclado (LLM, sin bloquear) y mouse (LLM + CPS, sin bloquear).
  auto hooks = lex::engine::installHooks();

  int rc = engine.run();

  lex::engine::uninstallHooks(hooks);
  ReleaseMutex(hMutex);
  CloseHandle(hMutex);
  return rc;
}
