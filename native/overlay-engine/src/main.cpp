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
  wchar_t cls[256] = {0};
  const int len = GetWindowTextW(hwnd, title, 512);
  GetClassNameW(hwnd, cls, 256);
  // Sólo la ventana Bedrock real; un navegador con "Minecraft" en el título
  // no debe capturar el overlay.
  const bool bedrock = wcscmp(cls, L"Bedrock") == 0;
  const bool browser = wcsstr(cls, L"Chrome_WidgetWin") != nullptr;
  if (len > 0 && IsWindowVisible(hwnd) && bedrock && !browser &&
      wcsstr(title, L"Minecraft") != nullptr) {
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
  // Sin consola: el motor corre 100% en segundo plano.
  HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"lex-overlay-engine");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    // Ya está corriendo.
    return 0;
  }

  lex::engine::OverlayEngine engine;
  (void)engine.stats();
  (void)nCmdShow;

  HWND hwnd = findMinecraftWindow();
  // El juego tarda unos segundos en crear su ventana: esperamos en segundo plano.
  // Espera indefinida: la activación UWP puede tardar mucho (o fallar una vez).
  // No inyectamos antes de que exista la ventana: hacerlo durante el arranque
  // DXGI de Bedrock provoca crashes en dxgi.dll. El puente se engancha después,
  // creando una swapchain propia para derivar la vtable de Present.
  while (!hwnd) {
    Sleep(200);
    hwnd = findMinecraftWindow();
  }
  fprintf(stderr, "[LEX] Minecraft hwnd=%p\n", (void*)hwnd);
  engine.setGameWindow(hwnd);

  // Hooks: teclado (LLM, sin bloquear) y mouse (LLM + CPS, sin bloquear).
  auto hooks = lex::engine::installHooks();

  int rc = engine.run();

  lex::engine::uninstallHooks(hooks);
  ReleaseMutex(hMutex);
  CloseHandle(hMutex);
  return rc;
}
