// Carga LeXClient.dll en el Minecraft que el usuario tiene abierto.
// Uso: lex-injector <ruta dll> [segundos de espera]
// Salida: 0 cargado, 1 uso, 2 juego no encontrado, 3 sin permiso, 4 fallo de carga.
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

static DWORD findGame() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  DWORD pid = 0;
  for (BOOL ok = Process32FirstW(snap, &entry); ok; ok = Process32NextW(snap, &entry)) {
    if (_wcsicmp(entry.szExeFile, L"Minecraft.Windows.exe") == 0) {
      pid = entry.th32ProcessID;
      break;
    }
  }
  CloseHandle(snap);
  return pid;
}

static BOOL CALLBACK windowOf(HWND window, LPARAM param) {
  auto* data = reinterpret_cast<std::pair<DWORD, bool>*>(param);
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  RECT r{};
  if (pid == data->first && IsWindowVisible(window) && GetClientRect(window, &r) && r.right > 200) {
    data->second = true;
    return FALSE;
  }
  return TRUE;
}

static bool hasWindow(DWORD pid) {
  std::pair<DWORD, bool> data{pid, false};
  EnumWindows(&windowOf, reinterpret_cast<LPARAM>(&data));
  return data.second;
}

static bool loaded(DWORD pid, const std::wstring& name) {
  HANDLE snap = INVALID_HANDLE_VALUE;
  // ERROR_BAD_LENGTH es normal mientras el juego sigue cargando modulos.
  for (int i = 0; i < 10 && snap == INVALID_HANDLE_VALUE; ++i) {
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) Sleep(50);
  }
  if (snap == INVALID_HANDLE_VALUE) return false;
  MODULEENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  for (BOOL ok = Module32FirstW(snap, &entry); ok && !found; ok = Module32NextW(snap, &entry)) {
    found = _wcsicmp(entry.szModule, name.c_str()) == 0;
  }
  CloseHandle(snap);
  return found;
}

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) return 1;
  std::error_code error;
  const std::filesystem::path dll = std::filesystem::absolute(argv[1], error);
  if (!std::filesystem::exists(dll, error)) return 1;
  const ULONGLONG limit = GetTickCount64() + 1000ULL * (argc > 2 ? _wtoi(argv[2]) : 90);

  DWORD pid = 0;
  while (GetTickCount64() < limit) {
    pid = findGame();
    if (pid != 0 && hasWindow(pid)) break;
    pid = 0;
    Sleep(400);
  }
  if (pid == 0) {
    std::puts("game-not-found");
    return 2;
  }
  if (loaded(pid, dll.filename().wstring())) {
    std::puts("already-loaded");
    return 0;
  }
  Sleep(2500);  // deja terminar el arranque grafico del juego

  HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_VM_READ,
                               FALSE, pid);
  if (process == nullptr) {
    std::puts("access-denied");
    return 3;
  }
  const std::wstring path = dll.wstring();
  const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  bool finished = false;
  if (remote != nullptr && WriteProcessMemory(process, remote, path.c_str(), bytes, nullptr)) {
    auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32"), "LoadLibraryW")));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load, remote, 0, nullptr);
    if (thread != nullptr) {
      finished = WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0;
      CloseHandle(thread);
    }
  }
  // Si el hilo sigue vivo aun lee la ruta: en ese caso no se libera.
  if (remote != nullptr && finished) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  CloseHandle(process);
  bool ok = false;
  for (int i = 0; i < 80 && !ok; ++i) {
    ok = loaded(pid, dll.filename().wstring());
    if (!ok) Sleep(250);
  }
  std::puts(ok ? "ok" : "load-failed");
  return ok ? 0 : 4;
}
