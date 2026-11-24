#pragma once

#include <string_view>

#include <imgui.h>

namespace lex::platform::windows::gui {

// Iconos vectoriales propios (estilo linea). Se dibujan con ImDrawList, por lo
// que escalan a cualquier tamano y no requieren assets externos.
void drawIcon(ImDrawList* drawList, std::string_view iconId,
              const ImVec2& minimum, const ImVec2& maximum, ImU32 color,
              float thickness = 2.0f);

}  // namespace lex::platform::windows::gui
