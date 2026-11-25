#pragma once

#include "core/gui/ClickGuiModel.h"
#include "core/platform/IPlatform.h"

namespace lex::platform::windows::hud {
class Crosshair;
}

namespace lex::platform::windows::gui {

class ClickGuiRenderer {
 public:
  static ClickGuiRenderer& instance();
  void draw(core::gui::ClickGuiModel& model, core::IPlatform& platform, float uiScale = 1.0f);

  bool hudEditing() const noexcept { return hudEditing_; }
  void setHudEditing(bool value) noexcept { hudEditing_ = value; }
  // Hay un ajuste esperando que se pulse una tecla.
  bool capturingKey() const noexcept { return capturing_ != nullptr && !hasCapture_; }
  void captureKey(int key);

 private:
  ClickGuiRenderer() = default;
  void drawSettings(core::gui::ClickGuiModel& model, core::module::Module& module, float s);
  void drawModules(core::gui::ClickGuiModel& model, float s);
  void drawCosmetics(float s);
  void drawCrosshairEditor(hud::Crosshair& crosshair, float s, float width);
  void drawHint(core::gui::ClickGuiModel& model, float s);
  void drawPreferences(core::gui::ClickGuiModel& model, float s);

  bool hudEditing_ = false;
  char search_[64] = {};
  const void* capturing_ = nullptr;
  core::settings::Keybind captured_;
  bool hasCapture_ = false;
  float hintTime_ = 0.0f;
  int cosmeticSlot_ = 0;
  float cosmeticYaw_ = 200.0f;
};

}  // namespace lex::platform::windows::gui
