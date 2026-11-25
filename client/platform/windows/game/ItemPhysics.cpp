#include "platform/windows/game/ItemPhysics.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/util/Logger.h"
#include "platform/windows/game/Memory.h"

namespace lex::platform::windows::game {
namespace {

struct Page {
  std::uint8_t stub[32];
  std::uintptr_t actor;
  float bob;  // sustituye al 0.1 con que el juego hace flotar los objetos
};

Page* g_page = nullptr;
std::atomic<bool> g_on{false};
std::atomic<float> g_tumble{1.0f};
std::atomic<float> g_height{0.0f};
std::int32_t g_seedOffset = 0;
using Rotate = void(*)(float* out, const float* matrix, float angle, const float* axis);
Rotate g_rotate = nullptr;

// Matrices por columnas: se combinan las columnas de los ejes afectados.
void spin(float* m, int a, int b, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  for (int i = 0; i < 4; ++i) {
    const float x = m[a * 4 + i], y = m[b * 4 + i];
    m[a * 4 + i] = x * c + y * s;
    m[b * 4 + i] = y * c - x * s;
  }
}

void itemRotate(float* out, const float* matrix, float angle, const float* axis) {
  if (!g_on.load(std::memory_order_relaxed) || g_page->actor == 0) {
    g_rotate(out, matrix, angle, axis);
    return;
  }
  float seed = 0.0f;
  safeRead(g_page->actor + static_cast<std::uintptr_t>(g_seedOffset), &seed, 4);
  std::memcpy(out, matrix, 16 * sizeof(float));
  const float age = angle + 0.025f;  // el giro del juego avanza un radian por segundo
  const float landing = 0.9f;
  const float left = std::clamp(1.0f - age / landing, 0.0f, 1.0f);
  const float height = g_height.load(std::memory_order_relaxed);
  for (int i = 0; i < 4; ++i) out[12 + i] += matrix[4 + i] * height;
  // Cada objeto cae con su propia orientacion y acaba plano.
  const float tumble = left * left * g_tumble.load(std::memory_order_relaxed) * 9.4f;
  spin(out, 2, 0, std::fmod(std::fabs(seed) * 57.0f, 6.2831853f) + tumble * 0.7f);
  spin(out, 1, 2, 1.5707963f + tumble);
}

bool constant(const std::uint8_t* p, std::size_t length, float value) {
  std::int32_t disp = 0;
  std::memcpy(&disp, p + length - 4, 4);
  float found = 0.0f;
  return safeRead(reinterpret_cast<std::uintptr_t>(p) + length + disp, &found, 4) && std::fabs(found - value) < 1e-4f;
}

void* allocateNear(std::uintptr_t origin) {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  const std::uintptr_t step = info.dwAllocationGranularity;
  for (std::uintptr_t distance = step; distance < 0x70000000; distance += step) {
    for (const std::uintptr_t at : {origin - distance, origin + distance}) {
      MEMORY_BASIC_INFORMATION region{};
      if (VirtualQuery(reinterpret_cast<void*>(at), &region, sizeof(region)) == 0 || region.State != MEM_FREE) continue;
      if (void* p = VirtualAlloc(reinterpret_cast<void*>(at & ~(step - 1)), 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) return p;
    }
  }
  return nullptr;
}

bool patch(std::uint8_t* at, const void* data, std::size_t size) {
  DWORD old = 0;
  if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old)) return false;
  std::memcpy(at, data, size);
  VirtualProtect(at, size, old, &old);
  FlushInstructionCache(GetCurrentProcess(), at, size);
  return true;
}

// addss xmm7,[-0.025]; maxss xmm7,xmm0; mulss xmm7,[57.2958]
std::uint8_t* findSite(std::uint8_t* first, std::size_t size) {
  std::uint8_t* found = nullptr;
  for (std::size_t i = 0x200; i + 0x100 < size; ++i) {
    const auto* next = static_cast<std::uint8_t*>(std::memchr(first + i, 0xF3, size - 0x100 - i));
    if (next == nullptr) break;
    i = static_cast<std::size_t>(next - first);
    std::uint8_t* p = first + i;
    if (p[0] != 0xF3 || p[1] != 0x0F || p[2] != 0x58 || p[3] != 0x3D || p[8] != 0xF3 || p[9] != 0x0F || p[10] != 0x5F || p[11] != 0xF8 || p[12] != 0xF3 || p[13] != 0x0F || p[14] != 0x59 || p[15] != 0x3D) continue;
    if (!constant(p, 8, -0.025f) || !constant(p + 12, 8, 57.295776f)) continue;
    if (found != nullptr) return nullptr;  // ambiguo: mejor no tocar nada
    found = p;
  }
  return found;
}

}  // namespace

bool ItemPhysics::install() {
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  std::uint8_t* site = nullptr;
  for (int i = 0; i < nt->FileHeader.NumberOfSections && site == nullptr; ++i) {
    if (std::memcmp(section[i].Name, ".text", 6) == 0) site = findSite(reinterpret_cast<std::uint8_t*>(base + section[i].VirtualAddress), section[i].Misc.VirtualSize);
  }
  if (site == nullptr) {
    LEX_LOG_WARNING("Fisica de objetos: no disponible en esta version");
    return false;
  }
  // La llamada a glm::rotate: movaps xmm2,xmm7 / call
  std::uint8_t* call = nullptr;
  for (std::uint8_t* p = site + 20; p < site + 0x80; ++p) {
    if (p[0] == 0x0F && p[1] == 0x28 && p[2] == 0xD7 && p[3] == 0xE8) {
      call = p + 3;
      break;
    }
  }
  // Desfase aleatorio del objeto (addss xmm0,[rsi+X]) y cargas de 0.1.
  std::uint8_t* bobs[4] = {};
  int count = 0;
  for (std::uint8_t* p = site - 0x180; p < site; ++p) {
    if (p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x58 && p[3] == 0x86) std::memcpy(&g_seedOffset, p + 4, 4);
    if (p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x10 && (p[3] == 0x05 || p[3] == 0x0D) && constant(p, 8, 0.1f) && count < 4) bobs[count++] = p;
  }
  if (call == nullptr || count != 2 || g_seedOffset <= 0 || g_seedOffset > 0x2000) {
    LEX_LOG_WARNING("Fisica de objetos: codigo distinto al esperado");
    return false;
  }
  auto* page = static_cast<Page*>(allocateNear(reinterpret_cast<std::uintptr_t>(site)));
  if (page == nullptr) return false;
  std::int32_t rel = 0;
  std::memcpy(&rel, call + 1, 4);
  g_rotate = reinterpret_cast<Rotate>(call + 5 + rel);
  page->actor = 0;
  page->bob = 0.1f;
  // mov [rip+actor], rsi ; jmp [rip+0] ; direccion
  std::uint8_t code[21] = {0x48, 0x89, 0x35, 0, 0, 0, 0, 0xFF, 0x25, 0, 0, 0, 0};
  const std::int32_t toActor = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t*>(&page->actor) - (page->stub + 7));
  std::memcpy(code + 3, &toActor, 4);
  const auto target = reinterpret_cast<std::uintptr_t>(&itemRotate);
  std::memcpy(code + 13, &target, 8);
  std::memcpy(page->stub, code, sizeof(code));
  g_page = page;
  const auto reach = [](const void* to, const std::uint8_t* from) { return static_cast<std::int32_t>(static_cast<const std::uint8_t*>(to) - from); };
  const std::int32_t toStub = reach(page->stub, call + 5);
  bool ok = patch(call + 1, &toStub, 4);
  for (int i = 0; i < count; ++i) {
    const std::int32_t toBob = reach(&page->bob, bobs[i] + 8);
    ok = patch(bobs[i] + 4, &toBob, 4) && ok;
  }
  LEX_LOG_INFO(ok ? "Fisica de objetos lista" : "Fisica de objetos: no se pudo escribir");
  return ok;
}

bool ItemPhysics::available() { return g_page != nullptr; }

void ItemPhysics::configure(bool on, float tumble, float height) {
  if (g_page == nullptr) return;
  g_tumble.store(tumble);
  g_height.store(height);
  g_page->bob = on ? 0.0f : 0.1f;
  g_on.store(on);
}

}  // namespace lex::platform::windows::game
