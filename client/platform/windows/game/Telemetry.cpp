#include "platform/windows/game/Telemetry.h"

#include <windows.h>
#include <MinHook.h>

#include <array>

namespace lex::platform::windows::game {
namespace {

// Deben coincidir con launcher/shared/telemetry.mjs y cosmetics.mjs.
constexpr UINT kBaseW = 37;
constexpr UINT kBaseH = 211;
constexpr UINT kChannels = 39;
constexpr UINT kValues = 10;
constexpr UINT kExtraH = 300;
constexpr UINT kCountW = 103;
constexpr UINT kHudW = 104;
constexpr UINT kReachW = 105;
constexpr UINT kPeerW = 106;
constexpr UINT kReaches = 80;
constexpr UINT kCounts = 65;

struct LiveSpec {
  UINT w, h;
};
constexpr LiveSpec kLive[] = {{161 * 12, 83 * 12}, {67, 67}};
constexpr UINT kHealthW = 131;
constexpr UINT kHealthH = 36;
constexpr UINT kTrailW = 264;
constexpr UINT kTrailH = 40;

using SrvFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
SrvFn g_srv = nullptr;

std::array<std::atomic<int>, kChannels> g_values{};
std::atomic<ULONGLONG> g_seen{0};
std::atomic<ULONGLONG> g_hudSeen{0};
std::atomic<int> g_count{0};
std::atomic<ULONGLONG> g_countSeen{0};
std::atomic<ULONGLONG> g_hitSeen{0};
std::atomic<int> g_hitDistance{0};
std::atomic<unsigned> g_hitSerial{0};
std::atomic<bool> g_available{false};
std::atomic<int> g_peerMin{-1};
std::atomic<int> g_peerMax{-1};
std::atomic<ULONGLONG> g_peerSeen{0};

struct LiveSlot {
  std::atomic<ID3D12Resource*> resource{nullptr};
  std::atomic<ULONGLONG> seen{0};
};
std::array<LiveSlot, static_cast<std::size_t>(Live::Count)> g_live;

void noteLive(std::size_t index, ID3D12Resource* resource, ULONGLONG now) {
  LiveSlot& slot = g_live[index];
  slot.seen.store(now, std::memory_order_relaxed);
  ID3D12Resource* old = slot.resource.load();
  if (old == resource) return;
  // La referencia impide que el juego la libere mientras se repinta. La
  // anterior no se suelta: un fotograma en curso aun podria usarla.
  resource->AddRef();
  if (!slot.resource.compare_exchange_strong(old, resource)) resource->Release();
}

void note(ID3D12Resource* resource) {
  const D3D12_RESOURCE_DESC d = resource->GetDesc();
  if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) return;
  const UINT w = static_cast<UINT>(d.Width);
  const UINT h = d.Height;
  // Descarte rapido: casi ninguna textura del juego cae en estos rangos.
  if (w >= kBaseW && w < kBaseW + kChannels && h >= kBaseH && h < kBaseH + kValues) {
    g_values[w - kBaseW].store(static_cast<int>(h - kBaseH), std::memory_order_relaxed);
    g_seen.store(GetTickCount64(), std::memory_order_relaxed);
  } else if (w >= kCountW && w <= kPeerW && h >= kExtraH && h < kExtraH + kReaches) {
    const ULONGLONG now = GetTickCount64();
    const int value = static_cast<int>(h - kExtraH);
    if (w == kCountW && value < static_cast<int>(kCounts)) {
      g_count.store(value, std::memory_order_relaxed);
      g_countSeen.store(now, std::memory_order_relaxed);
    } else if (w == kHudW) {
      g_hudSeen.store(now, std::memory_order_relaxed);
    } else if (w == kPeerW) {
      const ULONGLONG before = g_peerSeen.exchange(now);
      if (before == 0 || now - before > 10000) {  // otro servidor: se empieza de cero
        g_peerMin.store(value);
        g_peerMax.store(value);
      } else {
        if (value < g_peerMin.load()) g_peerMin.store(value);
        if (value > g_peerMax.load()) g_peerMax.store(value);
      }
    } else if (w == kReachW) {
      // Un golpe se ve varios fotogramas seguidos: cuenta solo el primero.
      const ULONGLONG before = g_hitSeen.exchange(now);
      if (now - before > 200) {
        g_hitDistance.store(value);
        g_hitSerial.fetch_add(1);
      }
    }
  } else if (d.MipLevels == 1 && d.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
    for (std::size_t i = 0; i < std::size(kLive); ++i) {
      if (w == kLive[i].w && h == kLive[i].h) noteLive(i, resource, GetTickCount64());
    }
    if (w == kTrailW && h >= kTrailH && h < kTrailH + kTrails) {
      noteLive(static_cast<std::size_t>(Live::Trail) + (h - kTrailH), resource, GetTickCount64());
    }
    if (h == kHealthH && w >= kHealthW && w < kHealthW + kHealthSteps) {
      noteLive(static_cast<std::size_t>(Live::Health) + (w - kHealthW), resource, GetTickCount64());
    }
  }
}

void STDMETHODCALLTYPE srvDetour(ID3D12Device* self, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
  if (resource != nullptr) note(resource);
  g_srv(self, resource, desc, handle);
}

int value(int channel) { return g_values[channel].load(std::memory_order_relaxed); }

int number(int first, int digits) {
  int out = 0;
  for (int i = 0, scale = 1; i < digits; ++i, scale *= 10) out += value(first + i) * scale;
  return out;
}

bool fresh(ULONGLONG seen, ULONGLONG limit) { return seen != 0 && GetTickCount64() - seen < limit; }

}  // namespace

PlayerData playerData() {
  PlayerData data;
  data.valid = fresh(g_seen.load(), 1500);
  if (!data.valid) return data;
  for (int i = 0; i < 4; ++i) {
    data.armor[i] = value(i);
    data.damage[i] = number(4 + i * 3, 3);
  }
  data.health = number(16, 2);
  data.x = number(19, 5) * (value(18) != 0 ? -1 : 1);
  data.y = number(24, 3) - 100;
  data.z = number(28, 5) * (value(27) != 0 ? -1 : 1);
  data.facing = value(33);
  data.speed = number(34, 3) / 10.0f;
  data.holdingBlock = value(37) != 0;
  data.hurt = value(38) != 0;
  data.count = fresh(g_countSeen.load(), 400) ? g_count.load() : 0;
  return data;
}

PeerHealth peerHealth() {
  PeerHealth out;
  out.seen = fresh(g_peerSeen.load(), 10000);
  out.lowest = g_peerMin.load();
  out.highest = g_peerMax.load();
  return out;
}

Hit lastHit() {
  Hit hit;
  hit.time = g_hitSeen.load();
  hit.distance = g_hitDistance.load() / 10.0f;
  hit.serial = g_hitSerial.load();
  return hit;
}

void setTelemetryAvailable(bool available) { g_available.store(available); }
bool inWorld() { return !g_available.load() || fresh(g_seen.load(), 1500); }
bool crosshairVisible() { return fresh(g_live[static_cast<std::size_t>(Live::Crosshair)].seen.load(), 150); }

ID3D12Resource* liveTexture(Live which) {
  const LiveSlot& slot = g_live[static_cast<std::size_t>(which)];
  // Si hace rato que no se dibuja, el juego pudo descartarla: no se toca.
  return fresh(slot.seen.load(), 1000) ? slot.resource.load() : nullptr;
}

bool installTextureHooks(ID3D12Device* device) {
  void* target = (*reinterpret_cast<void***>(device))[18];  // CreateShaderResourceView
  const bool ok = MH_CreateHook(target, reinterpret_cast<void*>(&srvDetour), reinterpret_cast<void**>(&g_srv)) == MH_OK &&
                  MH_EnableHook(target) == MH_OK;
  setTelemetryAvailable(ok);
  return ok;
}

}  // namespace lex::platform::windows::game
