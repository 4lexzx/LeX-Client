#include "platform/windows/game/Memory.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <thread>

#include "core/util/Logger.h"

namespace lex::platform::windows::game {
// Lectura que nunca falla con una excepcion aunque la pagina desaparezca.
bool safeRead(std::uintptr_t address, void* out, std::size_t bytes) {
  SIZE_T done = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, bytes, &done) && done == bytes;
}

static constexpr std::size_t kChunk = 4 * 1024 * 1024;

// Recorre la memoria privada escribible del proceso por trozos alineados.
void forEachChunk(const std::function<void(std::uintptr_t, const std::uint64_t*, std::size_t)>& visit) {
  auto* buffer = static_cast<std::uint64_t*>(VirtualAlloc(nullptr, kChunk, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (buffer == nullptr) return;
  const auto own = reinterpret_cast<std::uintptr_t>(buffer);
  MEMORY_BASIC_INFORMATION info{};
  std::uintptr_t address = 0;
  while (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == sizeof(info)) {
    const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE && info.Protect == PAGE_READWRITE && base != own) {
      for (std::size_t offset = 0; offset < info.RegionSize; offset += kChunk) {
        const std::size_t bytes = std::min<std::size_t>(kChunk, info.RegionSize - offset);
        if (safeRead(base + offset, buffer, bytes)) visit(base + offset, buffer, bytes / 8);
      }
    }
    const std::uintptr_t next = base + info.RegionSize;
    if (next <= address) break;
    address = next;
  }
  VirtualFree(buffer, 0, MEM_RELEASE);
}

namespace {

bool safeWrite(std::uintptr_t address, const void* data, std::size_t bytes) {
  SIZE_T done = 0;
  return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, bytes, &done) && done == bytes;
}

struct Range {
  std::uintptr_t first = 0;
  std::uintptr_t last = 0;
  bool contains(std::uintptr_t a) const { return a >= first && a < last; }
};

Range moduleRange() {
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  return {base, base + nt->OptionalHeader.SizeOfImage};
}

// std::string de MSVC: 16 bytes en linea o puntero, longitud y capacidad.
bool readString(std::uintptr_t address, std::string& out) {
  std::uint64_t raw[4];
  if (!safeRead(address, raw, sizeof(raw))) return false;
  const std::uint64_t size = raw[2];
  const std::uint64_t capacity = raw[3];
  if (size == 0 || size > 64 || capacity < 15 || capacity > 256 || size > capacity) return false;
  char text[65] = {};
  if (capacity < 16) std::memcpy(text, raw, size);
  else if (!safeRead(raw[0], text, size)) return false;
  for (std::uint64_t i = 0; i < size; ++i) {
    if (text[i] < 32 || text[i] > 126) return false;
  }
  out.assign(text, size);
  return true;
}

}  // namespace

Options& Options::instance() {
  static Options* options = new Options();
  return *options;
}

void Options::start() {
  if (started_.exchange(true)) return;
  std::thread([this]() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (int attempt = 0; attempt < 6 && !ready_.load(); ++attempt) {
      Sleep(attempt == 0 ? 4000 : 8000);
      scan();
    }
    if (!ready_.load()) LEX_LOG_WARNING("No se localizaron las opciones del juego");
  }).detach();
}

void Options::scan() {
  const Range module = moduleRange();
  const std::string fov = "gfx_field_of_view";

  // 1) El campo de vision tiene una forma inconfundible: 30, 110, valor, 60.
  struct Seed {
    std::uintptr_t address, info, vtable;
  };
  std::vector<Seed> seeds;
  forEachChunk([&](std::uintptr_t base, const std::uint64_t* q, std::size_t n) {
    for (std::size_t i = 1; i + 3 < n; ++i) {
      if (q[i + 1] != 0x42dc000041f00000ull) continue;  // {30.0f, 110.0f}
      if ((q[i + 2] >> 32) != 0x42700000ull) continue;  // defecto 60.0f
      if (module.contains(q[i - 1])) seeds.push_back({base + i * 8, q[i], q[i - 1]});
    }
  });

  // 2) Dentro de su "info" esta la etiqueta; su distancia vale para todas.
  Seed seed{};
  std::uintptr_t tagOffset = 0;
  for (const Seed& s : seeds) {
    for (std::uintptr_t offset = 0; offset < 0x400 && tagOffset == 0; offset += 8) {
      std::string text;
      if (readString(s.info + offset, text) && text == fov) {
        seed = s;
        tagOffset = offset;
      }
    }
    if (tagOffset != 0) break;
  }
  if (tagOffset == 0) return;

  // 3) Cualquier objeto {vtable del juego, puntero cercano a esa info} es una opcion.
  const Range infos{seed.info - 0x200000, seed.info + 0x200000};
  std::vector<Seed> found;
  forEachChunk([&](std::uintptr_t base, const std::uint64_t* q, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i) {
      if (infos.contains(q[i]) && module.contains(q[i - 1])) found.push_back({base + i * 8, q[i], q[i - 1]});
    }
  });

  std::map<std::string, std::pair<Entry, std::uintptr_t>> raw;
  for (const Seed& s : found) {
    std::string tag;
    if (!readString(s.info + tagOffset, tag) || tag.find('_') == std::string::npos) continue;
    raw.try_emplace(tag, Entry{s.address, s.info, Kind::Int}, s.vtable);
  }
  const auto boolean = raw.find("gfx_smoothlighting");
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [tag, item] : raw) {
    Entry entry = item.first;
    if (item.second == seed.vtable) entry.kind = Kind::Float;
    else if (boolean != raw.end() && item.second == boolean->second.second) entry.kind = Kind::Bool;
    entries_[tag] = entry;
  }
  ready_.store(entries_.contains(fov));
  LEX_LOG_INFO("Opciones del juego localizadas: " + std::to_string(entries_.size()));
}

bool Options::has(const std::string& tag) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.contains(tag);
}

bool Options::get(const std::string& tag, float& out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = entries_.find(tag);
  if (it == entries_.end()) return false;
  std::uintptr_t info = 0;
  if (!safeRead(it->second.address, &info, 8) || info != it->second.info) return false;
  if (it->second.kind == Kind::Float) return safeRead(it->second.address + 0x10, &out, 4);
  if (it->second.kind == Kind::Bool) {
    std::uint8_t value = 0;
    if (!safeRead(it->second.address + 8, &value, 1) || value > 1) return false;
    out = value;
    return true;
  }
  std::int32_t value = 0;
  if (!safeRead(it->second.address + 0x10, &value, 4)) return false;
  out = static_cast<float>(value);
  return true;
}

bool Options::set(const std::string& tag, float value) {
  float current = 0.0f;
  if (!get(tag, current)) return false;
  if (current == value) return true;
  std::lock_guard<std::mutex> lock(mutex_);
  const Entry& entry = entries_.at(tag);
  if (entry.kind == Kind::Float) {
    // El juego recorta al rango declarado: se ensancha lo justo.
    float range[2];
    if (safeRead(entry.address + 8, range, 8)) {
      if (value < range[0]) safeWrite(entry.address + 8, &value, 4);
      if (value > range[1]) safeWrite(entry.address + 0xC, &value, 4);
    }
    return safeWrite(entry.address + 0x10, &value, 4);
  }
  if (entry.kind == Kind::Bool) {
    const std::uint8_t byte = value != 0.0f ? 1 : 0;
    return safeWrite(entry.address + 8, &byte, 1);
  }
  const std::int32_t integer = static_cast<std::int32_t>(value);
  return safeWrite(entry.address + 0x10, &integer, 4);
}

}  // namespace lex::platform::windows::game
