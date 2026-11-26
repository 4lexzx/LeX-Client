// Puente: este archivo compila como libminecraftpe.so dentro del APK de LeX.
// NativeActivity lo carga por el nombre "minecraftpe"; aqui se busca la
// libminecraftpe.so real (la del Minecraft instalado, ya cargada por las clases
// de Mojang), se instala LeX y se le pasa el arranque.
#include <android/native_activity.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <android/log.h>

#include "lex.h"

namespace lex {
namespace {
std::string g_logFile;
}

void setLogFile(const std::string& path) { g_logFile = path; }

void log(const char* format, ...) {
  char text[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  __android_log_print(ANDROID_LOG_INFO, "LeX", "%s", text);
  if (g_logFile.empty()) return;
  if (FILE* f = fopen(g_logFile.c_str(), "a")) {
    time_t now = time(nullptr);
    char stamp[16];
    strftime(stamp, sizeof(stamp), "%H:%M:%S", localtime(&now));
    fprintf(f, "[%s] [I] [nativo:%d] %s\n", stamp, getpid(), text);
    fclose(f);
  }
}

}  // namespace lex

namespace {

struct Found {
  std::string path;
  uintptr_t base = 0;
};

// La libminecraftpe.so cargada que no es la nuestra.
Found findRealLibrary() {
  Found out;
  Dl_info self{};
  dladdr(reinterpret_cast<void*>(&findRealLibrary), &self);
  const std::string own = self.dli_fname ? self.dli_fname : "";
  FILE* maps = fopen("/proc/self/maps", "r");
  if (maps == nullptr) return out;
  char line[1024];
  while (fgets(line, sizeof(line), maps)) {
    const char* path = strchr(line, '/');
    if (path == nullptr || strstr(path, "/libminecraftpe.so") == nullptr) continue;
    std::string p(path);
    while (!p.empty() && (p.back() == '\n' || p.back() == ' ')) p.pop_back();
    if (p == own || p.find("com.lexclient.android") != std::string::npos) continue;
    unsigned long start = 0, offset = 0;
    char perms[8] = {};
    if (sscanf(line, "%lx-%*lx %7s %lx", &start, perms, &offset) != 3 || offset != 0) continue;
    if (out.base == 0 || start < out.base) {
      out.base = start;
      out.path = p;
    }
  }
  fclose(maps);
  return out;
}

// Direccion de un simbolo exportado leyendo la tabla dinamica del archivo
// (sin dlopen: la libreria real vive en el espacio de nombres de Mojang).
uintptr_t exportedSymbol(const Found& lib, const char* name) {
  const int fd = open(lib.path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return 0;
  uintptr_t result = 0;
  Elf64_Ehdr eh{};
  if (pread(fd, &eh, sizeof(eh), 0) == sizeof(eh) && memcmp(eh.e_ident, ELFMAG, SELFMAG) == 0) {
    std::vector<Elf64_Shdr> sections(eh.e_shnum);
    pread(fd, sections.data(), sizeof(Elf64_Shdr) * eh.e_shnum, static_cast<off_t>(eh.e_shoff));
    for (const auto& s : sections) {
      if (s.sh_type != SHT_DYNSYM || s.sh_link >= sections.size()) continue;
      const auto& str = sections[s.sh_link];
      std::vector<char> names(str.sh_size);
      pread(fd, names.data(), str.sh_size, static_cast<off_t>(str.sh_offset));
      std::vector<Elf64_Sym> symbols(s.sh_size / sizeof(Elf64_Sym));
      pread(fd, symbols.data(), s.sh_size, static_cast<off_t>(s.sh_offset));
      for (const auto& sym : symbols) {
        if (sym.st_name < names.size() && sym.st_value != 0 && strcmp(&names[sym.st_name], name) == 0) {
          result = lib.base + sym.st_value;
          break;
        }
      }
    }
  }
  close(fd);
  return result;
}

}  // namespace

namespace {
// Arranque comun para NativeActivity y GameActivity (sus estructuras comparten
// los campos que se usan aqui: rutas, sdkVersion y assetManager).
void forward(const char* entry, ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  // El registro va al mismo lex.log que la parte Java.
  if (activity->externalDataPath) lex::setLogFile(std::string(activity->externalDataPath) + "/lex.log");
  lex::log("Puente de LeX cargado por %s (Android SDK %d)", entry, activity->sdkVersion);
  Found real = findRealLibrary();
  if (real.base == 0) {
    lex::log("ERROR: la libminecraftpe.so del juego no esta cargada");
    return;
  }
  lex::log("Minecraft real: %s en %p", real.path.c_str(), reinterpret_cast<void*>(real.base));
  using OnCreate = void (*)(ANativeActivity*, void*, size_t);
  auto start = reinterpret_cast<OnCreate>(exportedSymbol(real, entry));
  if (start == nullptr) {
    lex::log("ERROR: el juego no exporta %s (version distinta)", entry);
    return;
  }
  lex::installOverlay(real.path.c_str(), activity);
  lex::log("Arrancando el juego");
  start(activity, savedState, savedStateSize);
}
}  // namespace

extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  forward("ANativeActivity_onCreate", activity, savedState, savedStateSize);
}

extern "C" JNIEXPORT void GameActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  forward("GameActivity_onCreate", activity, savedState, savedStateSize);
}
