#define IMGUI_DEFINE_MATH_OPERATORS
#include "platform/windows/gui/VectorIcons.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lex::platform::windows::gui {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct IconGeometry {
  ImVec2 center;
  float halfWidth;
  float halfHeight;
};

IconGeometry geometry(const ImVec2& minimum, const ImVec2& maximum) {
  return IconGeometry{
      ImVec2((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f),
      (maximum.x - minimum.x) * 0.5f, (maximum.y - minimum.y) * 0.5f};
}

void polygon(ImDrawList* list, const std::vector<ImVec2>& points, ImU32 color,
             bool filled, float thickness) {
  if (filled) {
    list->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()),
                              color);
  } else {
    list->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                      true, thickness);
  }
}

void drawStar(ImDrawList* list, const IconGeometry& g, ImU32 color,
              float thickness) {
  std::vector<ImVec2> points;
  points.reserve(10);
  for (int i = 0; i < 10; ++i) {
    const float radius = (i % 2 == 0) ? g.halfWidth * 0.95f : g.halfWidth * 0.40f;
    const float angle = -kPi / 2.0f +
                        static_cast<float>(i) * kPi / 5.0f;
    points.emplace_back(g.center.x + std::cos(angle) * radius,
                        g.center.y + std::sin(angle) * radius);
  }
  polygon(list, points, color, false, thickness);
}

void drawGear(ImDrawList* list, const IconGeometry& g, ImU32 color,
              float thickness) {
  const float radius = g.halfWidth * 0.62f;
  list->AddCircle(g.center, radius, color, 16, thickness);
  list->AddCircle(g.center, radius * 0.34f, color, 12, thickness);
  for (int i = 0; i < 8; ++i) {
    const float angle = static_cast<float>(i) * kPi / 4.0f;
    const ImVec2 direction(std::cos(angle), std::sin(angle));
    list->AddLine(g.center + direction * radius * 0.92f,
                  g.center + direction * radius * 1.35f, color, thickness);
  }
}

void drawEye(ImDrawList* list, const IconGeometry& g, ImU32 color,
             float thickness) {
  std::vector<ImVec2> outline;
  const int segments = 16;
  for (int i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float x = (t * 2.0f - 1.0f) * g.halfWidth * 0.95f;
    const float y = -std::sin(t * kPi) * g.halfHeight * 0.72f;
    outline.emplace_back(g.center.x + x, g.center.y + y);
  }
  for (int i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float x = (1.0f - t * 2.0f) * g.halfWidth * 0.95f;
    const float y = std::sin(t * kPi) * g.halfHeight * 0.72f;
    outline.emplace_back(g.center.x + x, g.center.y + y);
  }
  list->AddPolyline(outline.data(), static_cast<int>(outline.size()), color,
                    true, thickness);
  list->AddCircle(g.center, g.halfWidth * 0.24f, color, 12, thickness);
}

void drawShirt(ImDrawList* list, const IconGeometry& g, ImU32 color,
               float thickness) {
  const ImVec2 c = g.center;
  const float w = g.halfWidth * 0.82f;
  const float h = g.halfHeight * 0.80f;
  polygon(list,
          {ImVec2(c.x - w * 0.42f, c.y - h), ImVec2(c.x - w, c.y - h * 0.45f),
           ImVec2(c.x - w * 0.62f, c.y - h * 0.05f),
           ImVec2(c.x - w * 0.62f, c.y + h), ImVec2(c.x + w * 0.62f, c.y + h),
           ImVec2(c.x + w * 0.62f, c.y - h * 0.05f),
           ImVec2(c.x + w, c.y - h * 0.45f), ImVec2(c.x + w * 0.42f, c.y - h),
           ImVec2(c.x + w * 0.18f, c.y - h * 0.72f),
           ImVec2(c.x - w * 0.18f, c.y - h * 0.72f)},
          color, false, thickness);
}

void drawChart(ImDrawList* list, const IconGeometry& g, ImU32 color,
               float thickness, int bars) {
  const float totalWidth = g.halfWidth * 1.7f;
  const float barWidth = totalWidth / static_cast<float>(bars + 1);
  for (int i = 0; i < bars; ++i) {
    const float ratio = 0.35f + 0.6f * static_cast<float>((i * 7 + 3) % bars + 1) /
                                   static_cast<float>(bars);
    const float x = g.center.x - totalWidth * 0.5f + barWidth * (i + 0.5f);
    const float barHeight = g.halfHeight * 1.6f * ratio;
    list->AddRectFilled(ImVec2(x - barWidth * 0.3f, g.center.y + g.halfHeight * 0.8f),
                        ImVec2(x + barWidth * 0.3f,
                               g.center.y + g.halfHeight * 0.8f - barHeight),
                        color);
  }
  (void)thickness;
}

void drawCrosshair(ImDrawList* list, const IconGeometry& g, ImU32 color,
                   float thickness) {
  list->AddLine(ImVec2(g.center.x - g.halfWidth, g.center.y),
                ImVec2(g.center.x - g.halfWidth * 0.25f, g.center.y), color,
                thickness);
  list->AddLine(ImVec2(g.center.x + g.halfWidth * 0.25f, g.center.y),
                ImVec2(g.center.x + g.halfWidth, g.center.y), color, thickness);
  list->AddLine(ImVec2(g.center.x, g.center.y - g.halfHeight),
                ImVec2(g.center.x, g.center.y - g.halfHeight * 0.25f), color,
                thickness);
  list->AddLine(ImVec2(g.center.x, g.center.y + g.halfHeight * 0.25f),
                ImVec2(g.center.x, g.center.y + g.halfHeight), color, thickness);
}

void drawReset(ImDrawList* list, const IconGeometry& g, ImU32 color,
               float thickness) {
  const float radius = g.halfWidth * 0.70f;
  const int segments = 14;
  const float start = 0.55f;
  const float sweep = kPi * 1.65f;
  for (int i = 0; i < segments; ++i) {
    const float a0 = start + sweep * static_cast<float>(i) / segments;
    const float a1 = start + sweep * static_cast<float>(i + 1) / segments;
    list->AddLine(g.center + ImVec2(std::cos(a0), std::sin(a0)) * radius,
                  g.center + ImVec2(std::cos(a1), std::sin(a1)) * radius, color,
                  thickness);
  }
  const float end = start + sweep;
  const ImVec2 tip = g.center + ImVec2(std::cos(end), std::sin(end)) * radius;
  polygon(list,
          {tip, tip + ImVec2(-g.halfWidth * 0.42f, -g.halfHeight * 0.08f),
           tip + ImVec2(-g.halfWidth * 0.12f, -g.halfHeight * 0.46f)},
          color, true, thickness);
}

}  // namespace

void drawIcon(ImDrawList* drawList, std::string_view iconId,
              const ImVec2& minimum, const ImVec2& maximum, ImU32 color,
              float thickness) {
  if (drawList == nullptr) return;
  const IconGeometry g = geometry(minimum, maximum);

  if (iconId == "icon.category.hud" || iconId == "icon.module.fps") {
    drawChart(drawList, g, color, thickness, 4);
  } else if (iconId == "icon.category.visual" || iconId == "icon.module.zoom") {
    drawEye(drawList, g, color, thickness);
  } else if (iconId == "icon.category.utility") {
    drawCrosshair(drawList, g, color, thickness);
  } else if (iconId == "icon.category.cosmetics") {
    drawShirt(drawList, g, color, thickness);
  } else if (iconId == "icon.category.settings" || iconId == "icon.gear") {
    drawGear(drawList, g, color, thickness);
  } else if (iconId == "icon.search") {
    drawList->AddCircle(ImVec2(g.center.x - g.halfWidth * 0.18f,
                               g.center.y - g.halfHeight * 0.18f),
                        g.halfWidth * 0.48f, color, 14, thickness);
    drawList->AddLine(
        ImVec2(g.center.x + g.halfWidth * 0.16f, g.center.y + g.halfHeight * 0.16f),
        ImVec2(g.center.x + g.halfWidth * 0.85f, g.center.y + g.halfHeight * 0.85f),
        color, thickness * 1.4f);
  } else if (iconId == "icon.star") {
    drawStar(drawList, g, color, thickness);
  } else if (iconId == "icon.reset") {
    drawReset(drawList, g, color, thickness);
  } else if (iconId == "icon.close") {
    drawList->AddLine(
        ImVec2(g.center.x - g.halfWidth * 0.62f, g.center.y - g.halfHeight * 0.62f),
        ImVec2(g.center.x + g.halfWidth * 0.62f, g.center.y + g.halfHeight * 0.62f),
        color, thickness * 1.3f);
    drawList->AddLine(
        ImVec2(g.center.x + g.halfWidth * 0.62f, g.center.y - g.halfHeight * 0.62f),
        ImVec2(g.center.x - g.halfWidth * 0.62f, g.center.y + g.halfHeight * 0.62f),
        color, thickness * 1.3f);
  } else if (iconId == "icon.module.crosshair") {
    drawCrosshair(drawList, g, color, thickness);
  } else if (iconId == "icon.module.fullbright") {
    drawList->AddCircle(g.center, g.halfWidth * 0.34f, color, 14, thickness);
    for (int i = 0; i < 8; ++i) {
      const float angle = static_cast<float>(i) * kPi / 4.0f;
      const ImVec2 direction(std::cos(angle), std::sin(angle));
      drawList->AddLine(g.center + direction * g.halfWidth * 0.55f,
                        g.center + direction * g.halfWidth * 0.92f, color,
                        thickness);
    }
  } else {
    // Generico: cuadro redondeado con guion central.
    drawList->AddRect(minimum, maximum, color, 5.0f, 0, thickness);
    drawList->AddLine(ImVec2(g.center.x - g.halfWidth * 0.38f, g.center.y),
                      ImVec2(g.center.x + g.halfWidth * 0.38f, g.center.y), color,
                      thickness);
  }
}

}  // namespace lex::platform::windows::gui
