// Puente: este archivo compila como liblex.so dentro del APK de LeX.
// GameActivity lo carga por android.app.lib_name = "lex" y llama a su
// GameActivity_onCreate; aqui se busca la libminecraftpe.so real (ya cargada
// por LexApplication), se instala LeX y se le pasa el arranque.
#include <android/native_activity.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <signal.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

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

// La libminecraftpe.so cargada que no es la nuestra. Se busca entre las
// librerias cargadas (no en disco): asi vale tambien cuando Android la carga
// directamente de dentro del APK del juego (split_config.arm64_v8a.apk!/lib/...).
struct Found {
  std::string path;
  uintptr_t base = 0;
  const ElfW(Phdr)* phdr = nullptr;
  int phnum = 0;
};

int visitLibrary(dl_phdr_info* info, size_t, void* data) {
  auto* out = static_cast<Found*>(data);
  if (info->dlpi_name == nullptr || strstr(info->dlpi_name, "libminecraftpe.so") == nullptr) return 0;
  if (strstr(info->dlpi_name, "com.lexclient.android") != nullptr) return 0;
  out->path = info->dlpi_name;
  out->base = info->dlpi_addr;
  out->phdr = info->dlpi_phdr;
  out->phnum = info->dlpi_phnum;
  return 1;
}

Found findRealLibrary() {
  Found out;
  dl_iterate_phdr(&visitLibrary, &out);
  return out;
}

uint32_t gnuHash(const char* name) {
  uint32_t h = 5381;
  for (; *name; ++name) h = (h << 5) + h + static_cast<unsigned char>(*name);
  return h;
}

uint32_t elfHash(const char* name) {
  uint32_t h = 0;
  for (; *name; ++name) {
    h = (h << 4) + static_cast<unsigned char>(*name);
    const uint32_t g = h & 0xf0000000;
    if (g) h ^= g >> 24;
    h &= ~g;
  }
  return h;
}

// Direccion de un simbolo exportado, leida de la tabla dinamica ya en memoria
// (sin dlopen: la libreria real vive en el espacio de nombres de Mojang).
uintptr_t exportedSymbol(const Found& lib, const char* name) {
  const ElfW(Dyn)* dynamic = nullptr;
  for (int i = 0; i < lib.phnum; ++i) {
    if (lib.phdr[i].p_type == PT_DYNAMIC) dynamic = reinterpret_cast<const ElfW(Dyn)*>(lib.base + lib.phdr[i].p_vaddr);
  }
  if (dynamic == nullptr) return 0;
  const ElfW(Sym)* symtab = nullptr;
  const char* strtab = nullptr;
  const uint32_t* gnu = nullptr;
  const uint32_t* sysv = nullptr;
  for (const ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_SYMTAB) symtab = reinterpret_cast<const ElfW(Sym)*>(lib.base + d->d_un.d_ptr);
    if (d->d_tag == DT_STRTAB) strtab = reinterpret_cast<const char*>(lib.base + d->d_un.d_ptr);
    if (d->d_tag == DT_GNU_HASH) gnu = reinterpret_cast<const uint32_t*>(lib.base + d->d_un.d_ptr);
    if (d->d_tag == DT_HASH) sysv = reinterpret_cast<const uint32_t*>(lib.base + d->d_un.d_ptr);
  }
  if (symtab == nullptr || strtab == nullptr) return 0;
  const auto match = [&](uint32_t index) -> uintptr_t {
    const ElfW(Sym)& sym = symtab[index];
    if (sym.st_value == 0 || sym.st_shndx == SHN_UNDEF) return 0;
    return strcmp(strtab + sym.st_name, name) == 0 ? lib.base + sym.st_value : 0;
  };
  if (gnu != nullptr) {
    const uint32_t buckets = gnu[0], offset = gnu[1], words = gnu[2];
    const auto* bloom = reinterpret_cast<const ElfW(Addr)*>(gnu + 4);
    const uint32_t* bucket = reinterpret_cast<const uint32_t*>(bloom + words);
    const uint32_t* chain = bucket + buckets;
    const uint32_t h = gnuHash(name);
    for (uint32_t i = bucket[h % buckets]; i >= offset && i != 0; ++i) {
      const uint32_t hh = chain[i - offset];
      if ((hh | 1) == (h | 1)) {
        if (uintptr_t found = match(i)) return found;
      }
      if (hh & 1) break;
    }
  }
  if (sysv != nullptr) {
    const uint32_t buckets = sysv[0];
    const uint32_t* bucket = sysv + 2;
    const uint32_t* chain = bucket + buckets;
    for (uint32_t i = bucket[elfHash(name) % buckets]; i != 0; i = chain[i]) {
      if (uintptr_t found = match(i)) return found;
    }
  }
  return 0;
}

// Si las clases de Mojang aun no la cargaron, se intenta desde la ruta que
// apunta la parte Java (LEX_MC_LIB), con sus dependencias antes.
void loadRealLibrary() {
  const char* path = getenv("LEX_MC_LIB");
  if (path == nullptr || *path == 0) {
    lex::log("Sin ruta de la libreria del juego (LEX_MC_LIB)");
    return;
  }
  std::string dir(path);
  dir = dir.substr(0, dir.rfind('/') + 1);
  for (const char* dep : {"libc++_shared.so", "libfmod.so", "libMediaDecoders_Android.so", "libpairipcore.so"}) {
    void* handle = dlopen((dir + dep).c_str(), RTLD_NOW | RTLD_GLOBAL);
    lex::log("Carga previa %s: %s", dep, handle ? "ok" : dlerror());
  }
  void* handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  lex::log("Carga de %s: %s", path, handle ? "ok" : dlerror());
}

// Cierre por fallo nativo: se anota donde (libreria y desplazamiento) antes de
// dejar que Android lo trate como siempre.
struct sigaction g_previous[32];

void onCrash(int signal, siginfo_t* info, void* context) {
  uintptr_t pc = 0;
#if defined(__aarch64__)
  pc = static_cast<uintptr_t>(static_cast<ucontext_t*>(context)->uc_mcontext.pc);
#endif
  Dl_info where{};
  if (pc && dladdr(reinterpret_cast<void*>(pc), &where) && where.dli_fname) {
    lex::log("FALLO NATIVO senal %d en %s+0x%lx (%s) direccion %p", signal, where.dli_fname,
             static_cast<unsigned long>(pc - reinterpret_cast<uintptr_t>(where.dli_fbase)), where.dli_sname ? where.dli_sname : "?", info ? info->si_addr : nullptr);
  } else {
    lex::log("FALLO NATIVO senal %d en pc %p direccion %p", signal, reinterpret_cast<void*>(pc), info ? info->si_addr : nullptr);
  }
  sigaction(signal, &g_previous[signal], nullptr);
  raise(signal);
}

void watchCrashes() {
  struct sigaction action {};
  action.sa_sigaction = &onCrash;
  action.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (int s : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT}) sigaction(s, &action, &g_previous[s]);
}

// Arranque comun para NativeActivity y GameActivity (sus estructuras comparten
// los campos que se usan aqui: rutas, sdkVersion y assetManager).
void forward(const char* entry, ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  // El registro va al mismo lex.log que la parte Java.
  if (activity->externalDataPath) lex::setLogFile(std::string(activity->externalDataPath) + "/lex.log");
  lex::log("Puente de LeX cargado por %s (Android SDK %d)", entry, activity->sdkVersion);
  watchCrashes();
  Found real = findRealLibrary();
  if (real.base == 0) {
    lex::log("La libminecraftpe.so del juego aun no esta cargada; se carga ahora");
    loadRealLibrary();
    real = findRealLibrary();
  }
  if (real.base == 0) {
    lex::log("ERROR: no se pudo cargar la libminecraftpe.so del juego");
    return;
  }
  lex::log("Minecraft real: %s en %p", real.path.c_str(), reinterpret_cast<void*>(real.base));
  using OnCreate = void (*)(ANativeActivity*, void*, size_t);
  auto start = reinterpret_cast<OnCreate>(exportedSymbol(real, entry));
  if (start == nullptr) {
    // NativeActivity y GameActivity: se prueba la otra entrada por si acaso.
    const char* other = strcmp(entry, "ANativeActivity_onCreate") == 0 ? "GameActivity_onCreate" : "ANativeActivity_onCreate";
    start = reinterpret_cast<OnCreate>(exportedSymbol(real, other));
    lex::log("El juego no exporta %s; %s: %s", entry, other, start ? "si" : "tampoco");
  }
  if (start == nullptr) {
    lex::log("ERROR: no se encontro la entrada del juego (version distinta)");
    return;
  }
  lex::installOverlay(real.path.c_str(), activity);
  lex::registerInput(activity->env);
  lex::log("Arrancando el juego");
  start(activity, savedState, savedStateSize);
  lex::log("Arranque entregado al juego");
}
}  // namespace

extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  forward("ANativeActivity_onCreate", activity, savedState, savedStateSize);
}

extern "C" JNIEXPORT void GameActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
  forward("GameActivity_onCreate", activity, savedState, savedStateSize);
}
