#pragma once

#include <imgui.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "platform/windows/game/Cosmetics.h"

namespace lex::platform::windows::gui {

// Dibuja en 3D la skin del jugador con cosmeticos puestos, con ImGui.
class Preview {
 public:
  static Preview& instance();
  void update();  // una vez por fotograma
  // zoomHead encuadra solo la cabeza.
  void draw(ImDrawList& list, ImVec2 min, ImVec2 size, const std::vector<const game::Cosmetics::Item*>& worn, float yaw, bool zoomHead, bool wide);

  // Carga la textura de un cosmetico en segundo plano.
  void request(const std::string& id);

 private:
  struct Loaded {
    std::string key;
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> rgba;
  };
  Preview() = default;
  std::mutex mutex_;
  std::vector<std::shared_ptr<Loaded>> ready_;
  std::set<std::string> requested_;
};

}  // namespace lex::platform::windows::gui
