#pragma once

#include <array>
#include <cstdint>

#include "platform/windows/hud/HudModule.h"

namespace lex::platform::windows::hud {

// Edita la mira del propio juego. En modo "Como el juego" se repinta su textura y
// el juego la dibuja con su efecto; en los demas la dibuja LeX en su sitio,
// solo cuando el juego mostraria la suya.
class Crosshair final : public ScreenModule {
 public:
  static constexpr int kGrid = 15;
  using Grid = std::array<std::uint8_t, kGrid * kGrid>;

  Crosshair();
  void onEnable() override { dirty_ = true; }
  void onDisable() override;
  void paint(ImDrawList& list, ImVec2 screen, float uiScale) override;
  nlohmann::json saveExtra() const override;
  void loadExtra(const nlohmann::json& value) override;

  Grid& grid() noexcept { return grid_; }
  void touch() noexcept { dirty_ = true; }
  void preset(int index);
  static constexpr int kPresets = 18;
  static Grid presetGrid(int index);
  ImU32 color(float phase = 0.0f) const;

 private:
  void publish();
  Grid grid_{};
  bool dirty_ = true;
  std::size_t lastMode_ = 99;
};

}  // namespace lex::platform::windows::hud
