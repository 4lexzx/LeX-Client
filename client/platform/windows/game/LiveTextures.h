#pragma once

#include <d3d12.h>

#include <array>
#include <cstdint>
#include <vector>

#include "platform/windows/game/Telemetry.h"

namespace lex::platform::windows::game {

// Texturas completas del juego que LeX repinta: mira y placa de vida.
class LiveTextures {
 public:
  static LiveTextures& instance();
  void set(Live which, int width, int height, std::vector<std::uint8_t> rgba);
  void record(ID3D12Device* device, ID3D12GraphicsCommandList* list, std::uint64_t frame);
  void releaseGpu();
  // Vuelve a pintar todo en el siguiente fotograma (p. ej. al entrar a un mundo).
  void invalidate() {
    for (Slot& slot : slots_) slot.painted = nullptr;
  }

 private:
  struct Slot {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    bool dirty = false;
    ID3D12Resource* painted = nullptr;
  };
  struct Upload {
    ID3D12Resource* buffer = nullptr;
    std::uint64_t frame = 0;
  };
  LiveTextures() = default;
  std::array<Slot, static_cast<std::size_t>(Live::Count)> slots_;
  std::vector<Upload> uploads_;
};

}  // namespace lex::platform::windows::game
