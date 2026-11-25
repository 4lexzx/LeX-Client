#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace lex::platform::windows::game {

struct OnlinePlayer {
  std::string name;
  std::string id;  // XUID, o el nombre si el servidor no lo envia
  std::array<std::uint32_t, 64> face{};  // 8x8 ABGR, con la capa del sombrero
  bool hasFace = false;
  bool self = false;
  bool lex = false;  // usa LeX Client
};

// Lista de jugadores que el juego ya recibe del mundo o servidor. Solo lectura:
// se localiza por la forma de sus nodos, sin offsets de una version.
class PlayerList {
 public:
  static PlayerList& instance();
  // Busca y refresca mientras se siga llamando (cada fotograma con el modulo activo).
  void touch();
  std::vector<OnlinePlayer> snapshot() const;

 private:
  PlayerList() = default;
  void run();
  bool locate();
  bool refresh();
  void announce();

  mutable std::mutex mutex_;
  std::vector<OnlinePlayer> players_;
  // Solo los usa el hilo de busqueda.
  std::set<std::string> ownNames_;
  std::string ownId_;
  std::vector<std::uintptr_t> maps_;
  std::map<std::uint64_t, unsigned long long> lanUsers_;
  std::uintptr_t socket_ = ~std::uintptr_t{0};
  bool socketTried_ = false;
  unsigned long long lastAnnounce_ = 0;

  std::atomic<unsigned long long> lastTouch_{0};
  std::atomic<bool> started_{false};
};

}  // namespace lex::platform::windows::game
