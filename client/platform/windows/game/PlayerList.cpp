#include <winsock2.h>
#include <ws2tcpip.h>

#include "platform/windows/game/PlayerList.h"

#include <algorithm>
#include <cstring>
#include <thread>

#include "core/util/Logger.h"
#include "platform/windows/game/Memory.h"

namespace lex::platform::windows::game {
namespace {

// Nodo: siguiente, anterior, UUID, id de entidad, UUID otra vez, nombre, XUID...
constexpr std::uintptr_t kName = 0x38, kXuid = 0x58, kSkin = 0xC0;
constexpr std::uintptr_t kImage = 0x80;  // formato, ancho, alto, fondo, ..., datos, bytes
constexpr std::size_t kMaxPlayers = 400;
constexpr u_short kPort = 19190;

bool text(std::uintptr_t address, std::string& out) {
  std::uint64_t raw[4];
  if (!safeRead(address, raw, sizeof(raw))) return false;
  if (raw[2] > 96 || raw[3] < 15 || raw[3] > 4096 || raw[2] > raw[3]) return false;
  char buffer[97] = {};
  if (raw[3] < 16) std::memcpy(buffer, raw, static_cast<std::size_t>(raw[2]));
  else if (raw[2] > 0 && !safeRead(static_cast<std::uintptr_t>(raw[0]), buffer, static_cast<std::size_t>(raw[2]))) return false;
  out.assign(buffer, static_cast<std::size_t>(raw[2]));
  return true;
}

// Quita los codigos de color y los saltos de linea.
std::string clean(const std::string& in) {
  std::string out;
  for (std::size_t i = 0; i < in.size(); ++i) {
    const auto c = static_cast<unsigned char>(in[i]);
    if (c == 0xC2 && i + 1 < in.size() && static_cast<unsigned char>(in[i + 1]) == 0xA7) {
      i += 2;
      continue;
    }
    if (c == '\n') break;
    if (c >= 32) out += in[i];
  }
  return out;
}

bool validNode(std::uintptr_t node) {
  std::uint64_t head[7];
  std::string name;
  return safeRead(node, head, sizeof(head)) && head[2] != 0 && head[3] != 0 && head[2] == head[5] && head[3] == head[6] && text(node + kName, name);
}

bool readFace(std::uintptr_t node, std::array<std::uint32_t, 64>& face) {
  std::uintptr_t skin = 0;
  std::uint32_t shape[4];
  std::uint64_t blob[2];
  if (!safeRead(node + kSkin, &skin, 8) || skin == 0) return false;
  if (!safeRead(skin + kImage, shape, sizeof(shape)) || !safeRead(skin + kImage + 0x20, blob, sizeof(blob))) return false;
  const std::uint32_t w = shape[1], h = shape[2];
  if (w < 64 || w > 1024 || w % 64 != 0 || (h != w && h * 2 != w) || blob[1] != std::uint64_t{w} * h * 4) return false;
  const std::uint32_t k = w / 64;
  const auto at = [&](std::uint32_t u, std::uint32_t v) {
    std::uint32_t c = 0;
    safeRead(static_cast<std::uintptr_t>(blob[0]) + (std::size_t{v * k + k / 2} * w + u * k + k / 2) * 4, &c, 4);
    return c;
  };
  for (std::uint32_t y = 0; y < 8; ++y) {
    for (std::uint32_t x = 0; x < 8; ++x) {
      const std::uint32_t base = at(8 + x, 8 + y);
      const std::uint32_t hat = at(40 + x, 8 + y);
      face[y * 8 + x] = ((hat >> 24) > 127 ? hat : base) | 0xFF000000u;
    }
  }
  return true;
}

std::uint64_t hashId(const std::string& id) {
  std::uint64_t h = 1469598103934665603ull;
  for (const char c : "lex-client:" + id) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
  return h;
}

}  // namespace

PlayerList& PlayerList::instance() {
  static PlayerList* list = new PlayerList();
  return *list;
}

void PlayerList::touch() {
  lastTouch_.store(GetTickCount64());
  if (!started_.exchange(true)) std::thread([this]() { run(); }).detach();
}

std::vector<OnlinePlayer> PlayerList::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return players_;
}

void PlayerList::run() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
  ULONGLONG nextScan = 0;
  DWORD wait = 6000;
  bool away = true;
  int misses = 0;
  for (;;) {
    Sleep(500);
    if (GetTickCount64() - lastTouch_.load() > 1500) {
      // Fuera del mundo (o en un menu): la proxima partida tiene otra lista,
      // asi que al volver se busca de nuevo aunque la anterior siga legible.
      wait = 6000;
      if (GetTickCount64() - lastTouch_.load() < 12000) continue;
      away = true;
      maps_.clear();
      std::lock_guard<std::mutex> lock(mutex_);
      players_.clear();
      continue;
    }
    if (refresh() && !away) {
      wait = 6000;
      misses = 0;
    } else if ((away || ++misses > 4) && GetTickCount64() >= nextScan) {
      away = false;
      if (locate()) refresh();
      nextScan = GetTickCount64() + wait;
      wait = std::min<DWORD>(wait * 2, 60000);
    }
    announce();
  }
}

bool PlayerList::locate() {
  // 1) Nodos candidatos y el nombre de la cuenta que tiene guardado el juego.
  std::vector<std::uintptr_t> nodes;
  std::set<std::string> names;
  forEachChunk([&](std::uintptr_t base, const std::uint64_t* q, std::size_t n) {
    Sleep(1);  // repartido: no debe notarse en los FPS
    for (std::size_t i = 0; i + 12 < n; ++i) {
      if (q[i + 2] == q[i + 5] && q[i + 3] == q[i + 6] && q[i + 2] != 0 && q[i + 3] != 0 && q[i] > 0x10000 && q[i + 1] > 0x10000 && q[i] % 8 == 0 && q[i + 9] <= 96 && q[i + 10] >= 15 && q[i + 10] < 4096) nodes.push_back(base + i * 8);
      if (q[i] == 0x67617472656d6147ull && q[i + 1] == 0x746e6948ull && q[i + 2] == 12 && q[i + 3] == 15) {
        std::string value;
        if (text(base + (i + 4) * 8, value) && !value.empty()) names.insert(value);
      }
    }
  });

  // 2) Cada lista es un anillo: sus miembros y cuantos son.
  std::map<std::uintptr_t, std::size_t> members;
  for (const std::uintptr_t node : nodes) {
    if (members.contains(node) || !validNode(node)) continue;
    std::vector<std::uintptr_t> ring;
    std::uintptr_t at = node;
    bool ok = true;
    do {
      std::uintptr_t next = 0, back = 0;
      ok = ring.size() <= kMaxPlayers && safeRead(at, &next, 8) && safeRead(next + 8, &back, 8) && back == at;
      ring.push_back(at);
      at = next;
    } while (ok && at != node);
    if (!ok || ring.size() < 2) continue;
    for (const std::uintptr_t r : ring) members[r] = ring.size() - 1;
  }
  if (members.empty()) return false;

  // 3) El mapa dueño: {1.0f, centinela, cantidad}.
  std::vector<std::uintptr_t> maps;
  forEachChunk([&](std::uintptr_t base, const std::uint64_t* q, std::size_t n) {
    Sleep(1);
    for (std::size_t i = 1; i + 1 < n; ++i) {
      if (static_cast<std::uint32_t>(q[i - 1]) != 0x3f800000u) continue;
      const auto it = members.find(static_cast<std::uintptr_t>(q[i]));
      if (it != members.end() && it->second == q[i + 1]) maps.push_back(base + i * 8);
    }
  });
  maps_ = maps;
  ownNames_ = names;
  LEX_LOG_INFO("Lista de jugadores: " + std::to_string(maps.size()) + " mapa(s)");
  return !maps.empty();
}

bool PlayerList::refresh() {
  std::vector<std::uintptr_t> best;
  for (const std::uintptr_t map : maps_) {
    std::uint64_t head[2];
    std::uint32_t factor = 0;
    if (!safeRead(map - 8, &factor, 4) || factor != 0x3f800000u || !safeRead(map, head, sizeof(head)) || head[1] > kMaxPlayers) continue;
    std::vector<std::uintptr_t> nodes;
    auto at = static_cast<std::uintptr_t>(head[0]);
    bool ok = true;
    for (std::uint64_t i = 0; i < head[1] && ok; ++i) {
      std::uintptr_t next = 0, back = 0;
      ok = safeRead(at, &next, 8) && safeRead(next + 8, &back, 8) && back == at && validNode(next);
      nodes.push_back(next);
      at = next;
    }
    if (ok && nodes.size() > best.size()) best = nodes;
  }
  if (best.empty()) return false;

  std::vector<OnlinePlayer> fresh;
  const ULONGLONG now = GetTickCount64();
  for (const std::uintptr_t node : best) {
    OnlinePlayer p;
    std::string raw;
    if (!text(node + kName, raw)) continue;
    p.name = clean(raw);
    if (p.name.empty()) continue;
    if (!text(node + kXuid, p.id) || p.id.empty()) p.id = raw;
    p.hasFace = readFace(node, p.face);
    p.self = ownNames_.contains(raw) || (!ownId_.empty() && p.id == ownId_);
    if (p.self && ownId_.empty()) ownId_ = p.id;
    const auto lan = lanUsers_.find(hashId(p.id));
    p.lex = p.self || (lan != lanUsers_.end() && now - lan->second < 15000);
    fresh.push_back(std::move(p));
  }
  std::sort(fresh.begin(), fresh.end(), [](const OnlinePlayer& a, const OnlinePlayer& b) {
    if (a.lex != b.lex) return a.lex;
    return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
  });
  std::lock_guard<std::mutex> lock(mutex_);
  players_ = std::move(fresh);
  return true;
}

// En la red local los LeX se reconocen entre si con un resumen del XUID
// (nunca el nombre), sin ningun servidor de por medio.
void PlayerList::announce() {
  SOCKET s = static_cast<SOCKET>(socket_);
  if (s == INVALID_SOCKET) {
    if (socketTried_) return;
    socketTried_ = true;
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    const BOOL yes = TRUE;
    u_long nonBlocking = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
    ioctlsocket(s, FIONBIO, &nonBlocking);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kPort);
    if (bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      closesocket(s);
      return;
    }
    socket_ = static_cast<std::uintptr_t>(s);
  }
  struct Packet {
    char magic[8];
    std::uint64_t id;
  } packet{};
  const ULONGLONG now = GetTickCount64();
  for (int i = 0; i < 64; ++i) {
    const int got = recv(s, reinterpret_cast<char*>(&packet), sizeof(packet), 0);
    if (got < 0 && WSAGetLastError() == WSAEWOULDBLOCK) break;
    if (got == static_cast<int>(sizeof(packet)) && std::memcmp(packet.magic, "LEXHERE1", 8) == 0 && lanUsers_.size() < 256) lanUsers_[packet.id] = now;
  }
  std::erase_if(lanUsers_, [&](const auto& u) { return now - u.second > 15000; });
  if (ownId_.empty() || now - lastAnnounce_ < 3000) return;
  lastAnnounce_ = now;
  std::memcpy(packet.magic, "LEXHERE1", 8);
  packet.id = hashId(ownId_);
  sockaddr_in all{};
  all.sin_family = AF_INET;
  all.sin_port = htons(kPort);
  all.sin_addr.s_addr = INADDR_BROADCAST;
  sendto(s, reinterpret_cast<const char*>(&packet), sizeof(packet), 0, reinterpret_cast<sockaddr*>(&all), sizeof(all));
}

}  // namespace lex::platform::windows::game
