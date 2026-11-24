#pragma once

#include "core/gui/ClickGuiModel.h"
#include "core/platform/IPlatform.h"

namespace lex::platform::windows::gui {

// Dibuja el modelo del ClickGUI con ImGui. Vive 100% en la capa Windows.
class ClickGuiRenderer {
 public:
  static ClickGuiRenderer& instance();

  void draw(core::gui::ClickGuiModel& model, core::IPlatform& platform);

 private:
  ClickGuiRenderer() = default;
};

}  // namespace lex::platform::windows::gui
