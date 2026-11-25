#include "platform/windows/hud/Crosshair.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "platform/windows/game/LiveTextures.h"
#include "platform/windows/game/Memory.h"

namespace lex::platform::windows::hud {

using namespace lex::core::settings;

namespace {

constexpr int kTexture = 67;
constexpr int kCell = 4;
constexpr int kMargin = 3;

template <typename S>
const S& get(const core::module::Module& m, const char* name) {
  return *static_cast<const S*>(m.settings().find(name));
}

std::vector<std::uint8_t> raster(const Crosshair::Grid& grid) {
  std::vector<std::uint8_t> out(kTexture * kTexture * 4, 0);
  for (int y = 0; y < Crosshair::kGrid * kCell; ++y) {
    for (int x = 0; x < Crosshair::kGrid * kCell; ++x) {
      if (grid[(y / kCell) * Crosshair::kGrid + x / kCell] == 0) continue;
      const int at = ((y + kMargin) * kTexture + x + kMargin) * 4;
      for (int c = 0; c < 4; ++c) out[at + c] = 255;
    }
  }
  return out;
}

// Plantillas en pixeles del juego (15x15, centro en 7,7).
Crosshair::Grid pattern(int index) {
  Crosshair::Grid g{};
  const auto put = [&](int x, int y) {
    if (x >= 0 && y >= 0 && x < Crosshair::kGrid && y < Crosshair::kGrid) g[y * Crosshair::kGrid + x] = 1;
  };
  const auto plus = [&](int from, int to) {
    for (int d = from; d <= to; ++d) {
      put(7 + d, 7);
      put(7 - d, 7);
      put(7, 7 + d);
      put(7, 7 - d);
    }
  };
  const auto ring = [&](float r) {
    for (int y = 0; y < Crosshair::kGrid; ++y) {
      for (int x = 0; x < Crosshair::kGrid; ++x) {
        if (std::fabs(std::hypot(x - 7.0f, y - 7.0f) - r) < 0.5f) put(x, y);
      }
    }
  };
  const auto square = [&](int r, int gap) {
    for (int d = -r; d <= r; ++d) {
      if (std::abs(d) < gap) continue;
      put(7 + d, 7 - r);
      put(7 + d, 7 + r);
      put(7 - r, 7 + d);
      put(7 + r, 7 + d);
    }
  };
  switch (index) {
    case 0: plus(0, 4); break;                                  // la del juego
    case 1: plus(0, 2); break;                                  // pequena
    case 2: plus(0, 6); break;                                  // grande
    case 3: plus(2, 5); break;                                  // abierta
    case 4: plus(2, 3); break;                                  // abierta corta
    case 5: plus(2, 4); put(7, 7); break;                       // abierta con punto
    case 6: put(7, 7); break;                                   // punto
    case 7: plus(0, 1); put(6, 6); put(8, 6); put(6, 8); put(8, 8); break;  // punto grueso
    case 8: ring(4.0f); break;                                  // circulo
    case 9: ring(4.0f); put(7, 7); break;                       // circulo con punto
    case 10: square(4, 0); break;                               // cuadrado
    case 11: square(4, 3); put(7, 7); break;                    // esquinas
    case 12: for (int d = -3; d <= 3; ++d) { put(7 + d, 7 + d); put(7 + d, 7 - d); } break;  // aspa
    case 13: for (int d = 2; d <= 4; ++d) { put(7 + d, 7 + d); put(7 - d, 7 + d); put(7 + d, 7 - d); put(7 - d, 7 - d); } put(7, 7); break;
    case 14: for (int d = 0; d <= 4; ++d) { put(7 + d, 7 + d); put(7 - d, 7 + d); } break;   // flecha
    case 15: for (int d = 0; d <= 4; ++d) { put(7 + d, 7); put(7 - d, 7); put(7, 7 + d); } break;  // T
    case 16: plus(0, 3); ring(6.0f); break;                     // mira completa
    default: for (int d = 0; d <= 3; ++d) { put(7 + d, 4 + d); put(7 - d, 4 + d); put(7 + d, 10 - d); put(7 - d, 10 - d); } break;  // rombo
  }
  return g;
}

}  // namespace

Crosshair::Grid Crosshair::presetGrid(int index) { return pattern(std::clamp(index, 0, kPresets - 1)); }

Crosshair::Crosshair() : ScreenModule("Crosshair", "Edita la mira del juego píxel a píxel", core::module::Category::Visual, "icon.module.crosshair") {
  settings().add<EnumSetting>("Color de la mira", "Cómo se colorea", 0).withOptions({"Como el juego", "Color fijo", "RGB"});
  settings().add<ColorSetting>("Color", "Color de la mira", Color{1.0f, 1.0f, 1.0f, 1.0f});
  settings().add<FloatSetting>("Tamaño", "Escala de la mira", 1.0f).withRange(0.4f, 3.0f);
  settings().add<BoolSetting>("Contorno", "Borde oscuro", false);
  grid_ = pattern(0);
}

void Crosshair::preset(int index) {
  grid_ = pattern(std::clamp(index, 0, kPresets - 1));
  dirty_ = true;
}

ImU32 Crosshair::color(float phase) const {
  if (get<EnumSetting>(*this, "Color de la mira").value() == 2) {
    return ImColor::HSV(std::fmod(static_cast<float>(ImGui::GetTime()) * 0.25f + phase, 1.0f), 0.7f, 1.0f);
  }
  if (get<EnumSetting>(*this, "Color de la mira").value() == 0) return IM_COL32_WHITE;
  const Color c = get<ColorSetting>(*this, "Color").value();
  return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
}

void Crosshair::publish() {
  const bool inverted = get<EnumSetting>(*this, "Color de la mira").value() == 0;
  game::LiveTextures::instance().set(game::Live::Crosshair, kTexture, kTexture,
                                     inverted ? raster(grid_) : std::vector<std::uint8_t>(kTexture * kTexture * 4, 0));
}

void Crosshair::onDisable() {
  // Devuelve la mira normal del juego.
  game::LiveTextures::instance().set(game::Live::Crosshair, kTexture, kTexture, raster(pattern(0)));
}

void Crosshair::paint(ImDrawList& list, ImVec2 screen, float uiScale) {
  const std::size_t mode = get<EnumSetting>(*this, "Color de la mira").value();
  if (dirty_ || mode != lastMode_) {
    publish();
    dirty_ = false;
    lastMode_ = mode;
  }
  if (mode == 0 || !game::crosshairVisible()) return;
  (void)uiScale;
  // Mismo tamano de pixel que la interfaz del juego.
  float offset = 0.0f;
  game::Options::instance().get("gfx_guiscale_offset", offset);
  const float gui = std::max(1.0f, std::floor(std::min(screen.x / 376.0f, screen.y / 250.0f)) + offset);
  const float cell = std::max(1.0f, std::round(gui * get<FloatSetting>(*this, "Tamaño").value()));
  const ImVec2 o(std::floor(screen.x * 0.5f - cell * kGrid * 0.5f), std::floor(screen.y * 0.5f - cell * kGrid * 0.5f));
  const auto each = [&](const auto& draw) {
    for (int y = 0; y < kGrid; ++y) {
      for (int x = 0; x < kGrid; ++x) {
        if (grid_[y * kGrid + x] != 0) draw(ImVec2(o.x + x * cell, o.y + y * cell), x + y);
      }
    }
  };
  if (get<BoolSetting>(*this, "Contorno").value()) {
    each([&](ImVec2 p, int) { list.AddRectFilled(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + cell + 1, p.y + cell + 1), IM_COL32(0, 0, 0, 200)); });
  }
  each([&](ImVec2 p, int i) { list.AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), color(i * 0.015f)); });
}

nlohmann::json Crosshair::saveExtra() const {
  std::string text;
  for (std::uint8_t v : grid_) text += v != 0 ? '1' : '0';
  return nlohmann::json{{"grid", text}};
}

void Crosshair::loadExtra(const nlohmann::json& value) {
  if (!value.is_object()) return;
  const std::string text = value.value("grid", std::string{});
  if (text.size() != grid_.size()) return;
  for (std::size_t i = 0; i < grid_.size(); ++i) grid_[i] = text[i] == '1' ? 1 : 0;
  dirty_ = true;
}

}  // namespace lex::platform::windows::hud
