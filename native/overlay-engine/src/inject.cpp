// LEX CLIENT — Inyección del puente de render en Minecraft.
// El usuario autorizó explícitamente inyección/hook para obtener FPS reales.
#include "engine.h"
#include <tlhelp32.h>
#include <cstdio>
#include <string>

namespace lex {
namespace engine {
namespace {

std::wstring bridgeDllPath() {
  wchar_t buf[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p(buf);
  const size_t slash = p.find_last_of(L"\\/");
  if (slash != std::wstring::npos) p = p.substr(0, slash + 1);
  else p = L"";
  return p + L"lex-bridge-loader.dll";
}

bool moduleLoaded(DWORD pid, const std::wstring& dll) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) return false;
  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  bool found = false;
  if (Module32FirstW(snap, &me)) {
    do {
      if (_wcsicmp(me.szExePath, dll.c_str()) == 0) { found = true; break; }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return found;
}

}  // namespace

void OverlayEngine::ensureGameBridge() {
  if (!gameHwnd_ || !IsWindow(gameHwnd_)) return;
  DWORD pid = 0;
  GetWindowThreadProcessId(gameHwnd_, &pid);
  ensureGameBridgePid(pid);
}

void OverlayEngine::ensureGameBridgePid(DWORD pid) {
  if (!pid || pid == GetCurrentProcessId()) return;

  const std::wstring dll = bridgeDllPath();
  if (moduleLoaded(pid, dll) || moduleLoaded(pid, L"lex-bridge-loader.dll") ||
      moduleLoaded(pid, L"lex-game-bridge.dll")) { injectedPid_ = pid; return; }
  // El cargador se descarga solo; comprobamos también el DLL final.
  wchar_t base[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, base, MAX_PATH);
  std::wstring b(base);
  const size_t slash = b.find_last_of(L"\\/");
  if (slash != std::wstring::npos) b = b.substr(0, slash + 1);
  if (moduleLoaded(pid, b + L"lex-game-bridge.dll")) { injectedPid_ = pid; return; }
  // Ya intentamos con este PID: reintentamos sólo si el módulo no llegó a cargarse.
  if (injectedPid_ == pid) injectedPid_ = 0;

  HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                         PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                         FALSE, pid);
  if (!h) return;

  const DWORD bytes = (DWORD)((dll.size() + 1) * sizeof(wchar_t));
  LPVOID remote = VirtualAllocEx(h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (remote) {
    if (WriteProcessMemory(h, remote, dll.c_str(), bytes, nullptr)) {
      HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
      FARPROC load = kernel ? GetProcAddress(kernel, "LoadLibraryW") : nullptr;
      if (load) {
        HANDLE th = CreateRemoteThread(h, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(load), remote, 0, nullptr);
        if (th) {
          WaitForSingleObject(th, 2000);
          DWORD code = 0;
          GetExitCodeThread(th, &code);
          char m[160];
          snprintf(m, sizeof(m), "inject pid=%lu exit=0x%lX", pid, (unsigned long)code);
          OutputDebugStringA(m);
          CloseHandle(th);
          // LoadLibraryW devuelve 0 en UWP (error 126) incluso cuando el
          // cargador pequeño funciona: no marcamos el PID como inyectado
          // definitivamente hasta ver el módulo cargado.
        }
      }
    }
    VirtualFreeEx(h, remote, 0, MEM_RELEASE);
  }
  CloseHandle(h);
}

}  // namespace engine
}  // namespace lex
