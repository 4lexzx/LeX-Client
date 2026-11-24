// LEX CLIENT — Cargador mínimo inyectado en Minecraft.
// LoadLibraryW normal falla dentro del proceso UWP (error 126: no encuentra
// libwinpthread-1.dll junto al DLL). Este cargador usa LoadLibraryExW con
// LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | SYSTEM32 | DEFAULT_DIRS.
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static HMODULE g_self = NULL;

static void logError(DWORD err) {
  FILE* f = fopen("C:\\Temp\\lex-bridge-loader.log", "a");
  if (f) { fprintf(f, "LoadLibraryExW error=%lu\n", err); fclose(f); }
}

static DWORD WINAPI loadBridge(LPVOID unused) {
  (void)unused;
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(g_self, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) {
    wcscpy(slash + 1, L"lex-game-bridge.dll");
  } else {
    wcscpy(path, L"lex-game-bridge.dll");
  }
  const DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                      LOAD_LIBRARY_SEARCH_SYSTEM32 |
                      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
  HMODULE h = LoadLibraryExW(path, NULL, flags);
  if (!h) logError(GetLastError());
  return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = h;
    DisableThreadLibraryCalls(h);
    CreateThread(NULL, 0, &loadBridge, NULL, 0, NULL);
  }
  return TRUE;
}
