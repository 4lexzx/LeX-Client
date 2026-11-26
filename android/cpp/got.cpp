// Enganche por GOT: cambia la direccion que una libreria usa para llamar a una
// funcion importada (eglSwapBuffers, AInputQueue_getEvent, glBindTexture...).
// No toca codigo: solo la tabla de punteros, asi que es estable entre versiones.
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>

#include "lex.h"

namespace lex {
namespace {

struct Search {
  const char* library;
  const char* symbol;
  void* replacement;
  void* original = nullptr;
  int patched = 0;
};

void patchSlot(uintptr_t slot, Search* s) {
  const uintptr_t page = slot & ~static_cast<uintptr_t>(getpagesize() - 1);
  if (mprotect(reinterpret_cast<void*>(page), getpagesize(), PROT_READ | PROT_WRITE) != 0) return;
  void** target = reinterpret_cast<void**>(slot);
  if (s->original == nullptr) s->original = *target;
  *target = s->replacement;
  __builtin___clear_cache(reinterpret_cast<char*>(page), reinterpret_cast<char*>(page + getpagesize()));
  ++s->patched;
}

int visit(dl_phdr_info* info, size_t, void* data) {
  auto* s = static_cast<Search*>(data);
  if (info->dlpi_name == nullptr || strstr(info->dlpi_name, s->library) == nullptr) return 0;
  // Solo la del juego, nunca nuestro puente con el mismo nombre.
  if (strstr(info->dlpi_name, "com.lexclient.android") != nullptr) return 0;
  const ElfW(Dyn)* dynamic = nullptr;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
  }
  if (dynamic == nullptr) return 0;
  const ElfW(Sym)* symtab = nullptr;
  const char* strtab = nullptr;
  const ElfW(Rela)* jmprel = nullptr;
  size_t jmprelSize = 0;
  const ElfW(Rela)* rela = nullptr;
  size_t relaSize = 0;
  for (const ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
    switch (d->d_tag) {
      case DT_SYMTAB: symtab = reinterpret_cast<const ElfW(Sym)*>(info->dlpi_addr + d->d_un.d_ptr); break;
      case DT_STRTAB: strtab = reinterpret_cast<const char*>(info->dlpi_addr + d->d_un.d_ptr); break;
      case DT_JMPREL: jmprel = reinterpret_cast<const ElfW(Rela)*>(info->dlpi_addr + d->d_un.d_ptr); break;
      case DT_PLTRELSZ: jmprelSize = d->d_un.d_val; break;
      case DT_RELA: rela = reinterpret_cast<const ElfW(Rela)*>(info->dlpi_addr + d->d_un.d_ptr); break;
      case DT_RELASZ: relaSize = d->d_un.d_val; break;
      default: break;
    }
  }
  if (symtab == nullptr || strtab == nullptr) return 0;
  const auto scan = [&](const ElfW(Rela)* table, size_t bytes) {
    if (table == nullptr) return;
    for (size_t i = 0; i < bytes / sizeof(ElfW(Rela)); ++i) {
      const auto type = ELF64_R_TYPE(table[i].r_info);
      if (type != R_AARCH64_JUMP_SLOT && type != R_AARCH64_GLOB_DAT && type != R_AARCH64_ABS64) continue;
      const auto index = ELF64_R_SYM(table[i].r_info);
      if (index == 0 || strcmp(strtab + symtab[index].st_name, s->symbol) != 0) continue;
      patchSlot(info->dlpi_addr + table[i].r_offset, s);
    }
  };
  scan(jmprel, jmprelSize);
  scan(rela, relaSize);
  return s->patched > 0 ? 1 : 0;
}

}  // namespace

void* hookImport(const char* library, const char* symbol, void* replacement) {
  Search s{library, symbol, replacement};
  dl_iterate_phdr(&visit, &s);
  log("Enganche %s: %s", symbol, s.patched ? "ok" : "no lo importa el juego");
  return s.original;
}

}  // namespace lex
