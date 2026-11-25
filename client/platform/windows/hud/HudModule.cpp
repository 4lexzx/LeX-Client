#include "platform/windows/hud/HudModule.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "core/module/ModuleManager.h"
#include "platform/windows/game/Telemetry.h"

namespace lex::platform::windows::hud {

using namespace lex::core::settings;

Fonts& fonts() {
  static Fonts value;
  return value;
}

namespace {

template <typename S>
const S* get(const core::module::Module& module, const char* name) {
  return static_cast<const S*>(module.settings().find(name));
}

ImU32 toU32(const Color& c) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
}

}  // namespace

HudModule::HudModule(std::string name, std::string description,
                     std::string icon, float x, float y, bool onByDefault)
    : Module(std::move(name), std::move(description),
             core::module::Category::Hud, std::move(icon)),
      defaultX_(x),
      defaultY_(y),
      x_(x),
      y_(y) {
  settings().add<FloatSetting>("Escala", "Tamaño del elemento", 1.0f).withRange(0.5f, 3.0f);
  settings().add<BoolSetting>("Fondo", "Muestra la caja de fondo", true);
  settings().add<ColorSetting>("Color de fondo", "Relleno de la caja", Color{0.03f, 0.05f, 0.09f, 0.62f});
  settings().add<ColorSetting>("Color de texto", "Color principal", Color{0.95f, 0.97f, 1.0f, 1.0f});
  settings().add<ColorSetting>("Acento", "Color de realce", Color{0.38f, 0.75f, 1.0f, 1.0f});
  settings().add<FloatSetting>("Redondeo", "Radio de las esquinas", 6.0f).withRange(0.0f, 20.0f);
  settings().add<BoolSetting>("Sombra", "Sombra bajo el texto", true);
  settings().add<EnumSetting>("Modo de color", "Cómo se colorea el texto", 0).withOptions({"Fijo", "RGB", "RGB en ola", "Degradado"});
  settings().add<ColorSetting>("Segundo color", "Final del degradado", Color{0.62f, 0.45f, 1.0f, 1.0f});
  settings().add<FloatSetting>("Velocidad RGB", "Rapidez del ciclo", 1.0f).withRange(0.1f, 4.0f);
  settings().add<BoolSetting>("Borde", "Contorno de la caja", false);
  settings().add<BoolSetting>("Borde RGB", "El contorno recorre el arcoíris", false);
  if (onByDefault) setEnabled(true);
}

float HudModule::scale() const { return get<FloatSetting>(*this, "Escala")->value(); }

void HudModule::setScale(float value) {
  static_cast<FloatSetting*>(settings().find("Escala"))->setValue(value);
}

float HudModule::rounding() const { return get<FloatSetting>(*this, "Redondeo")->value(); }
bool HudModule::shadow() const { return get<BoolSetting>(*this, "Sombra")->value(); }
ImU32 HudModule::fillColor() const { return toU32(get<ColorSetting>(*this, "Color de fondo")->value()); }
ImU32 HudModule::accentColor() const { return toU32(get<ColorSetting>(*this, "Acento")->value()); }

ImU32 HudModule::rainbow(float offset) const {
  const float speed = get<FloatSetting>(*this, "Velocidad RGB")->value();
  float hue = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.22f * speed - offset, 1.0f);
  if (hue < 0.0f) hue += 1.0f;
  return ImColor::HSV(hue, 0.72f, 1.0f);
}

ImU32 HudModule::textColor(float offset) const {
  const std::size_t mode = get<EnumSetting>(*this, "Modo de color")->value();
  if (mode == 1 || mode == 2) return rainbow(mode == 2 ? offset : 0.0f);
  if (mode == 3) {
    const Color a = get<ColorSetting>(*this, "Color de texto")->value();
    const Color b = get<ColorSetting>(*this, "Segundo color")->value();
    const float t = std::clamp(offset, 0.0f, 1.0f);
    return toU32(Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a});
  }
  return toU32(get<ColorSetting>(*this, "Color de texto")->value());
}

void HudModule::label(ImDrawList& list, ImVec2 at, float size,
                      const std::string& text, float offset) const {
  ImFont* font = fonts().bold != nullptr ? fonts().bold : ImGui::GetFont();
  if (shadow()) {
    const float d = std::max(1.0f, size * 0.06f);
    list.AddText(font, size, ImVec2(at.x + d, at.y + d), IM_COL32(0, 0, 0, 150), text.c_str());
  }
  const std::size_t mode = get<EnumSetting>(*this, "Modo de color")->value();
  if (mode < 2) {
    list.AddText(font, size, at, textColor(offset), text.c_str());
    return;
  }
  // Ola y degradado: cada caracter (UTF-8) lleva su propio color.
  const float total = std::max(1.0f, font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x);
  float x = at.x;
  for (std::size_t i = 0; i < text.size();) {
    std::size_t n = 1;
    while (i + n < text.size() && (static_cast<unsigned char>(text[i + n]) & 0xC0) == 0x80) ++n;
    const char* first = text.c_str() + i;
    const float along = (x - at.x) / total;
    list.AddText(font, size, ImVec2(x, at.y), textColor(offset + (mode == 2 ? along * 0.45f : along)), first, first + n);
    x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, first, first + n).x;
    i += n;
  }
}

void HudModule::box(ImDrawList& list, ImVec2 min, ImVec2 max, float s) const {
  if (get<BoolSetting>(*this, "Fondo")->value()) {
    list.AddRectFilled(min, max, fillColor(), rounding() * s);
  }
  if (get<BoolSetting>(*this, "Borde")->value()) {
    const ImU32 c = get<BoolSetting>(*this, "Borde RGB")->value() ? rainbow(min.x / 900.0f) : accentColor();
    list.AddRect(min, max, c, rounding() * s, 0, std::max(1.0f, 1.5f * s));
  }
}

ImVec2 HudModule::positionOn(ImVec2 screen) const {
  return ImVec2(std::floor(x_ * std::max(0.0f, screen.x - size_.x)),
                std::floor(y_ * std::max(0.0f, screen.y - size_.y)));
}

void HudModule::moveTo(ImVec2 p, ImVec2 screen) {
  const float fx = std::max(1.0f, screen.x - size_.x);
  const float fy = std::max(1.0f, screen.y - size_.y);
  x_ = std::clamp(p.x / fx, 0.0f, 1.0f);
  y_ = std::clamp(p.y / fy, 0.0f, 1.0f);
}

void HudModule::resetPosition() {
  x_ = defaultX_;
  y_ = defaultY_;
}

void HudModule::render(ImDrawList& list, ImVec2 screen, float uiScale) {
  const float s = scale() * uiScale;
  const ImVec2 base = measure();
  size_ = ImVec2(base.x * s, base.y * s);
  paint(list, positionOn(screen), s);
}

nlohmann::json HudModule::saveExtra() const {
  return nlohmann::json{{"x", x_}, {"y", y_}};
}

void HudModule::loadExtra(const nlohmann::json& value) {
  if (!value.is_object()) return;
  x_ = std::clamp(value.value("x", x_), 0.0f, 1.0f);
  y_ = std::clamp(value.value("y", y_), 0.0f, 1.0f);
}

ImVec4& coveredArea() {
  static ImVec4 area(0, 0, 0, 0);
  return area;
}

void drawHud(bool editing, float uiScale) {
  auto& manager = core::module::ModuleManager::instance();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImDrawList& list = *ImGui::GetBackgroundDrawList();

  coveredArea() = ImVec4(0, 0, 0, 0);
  for (const auto& module : manager.modules()) {
    if (!module->isEnabled()) continue;
    if (auto* effect = dynamic_cast<ScreenModule*>(module.get())) {
      effect->paint(list, screen, uiScale);
    }
  }

  static HudModule* dragging = nullptr;
  static ImVec2 grab;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  if (!editing || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging = nullptr;

  if (editing) {
    list.AddRectFilled(ImVec2(0, 0), screen, IM_COL32(4, 8, 16, 110));
    const float step = 40.0f * uiScale;
    for (float x = step; x < screen.x; x += step) list.AddLine(ImVec2(x, 0), ImVec2(x, screen.y), IM_COL32(120, 180, 255, 12));
    for (float y = step; y < screen.y; y += step) list.AddLine(ImVec2(0, y), ImVec2(screen.x, y), IM_COL32(120, 180, 255, 12));
  }

  // Fuera de un mundo no se dibuja nada, salvo al editar.
  if (!editing && !game::inWorld()) return;
  HudModule* hovered = nullptr;
  for (const auto& module : manager.modules()) {
    auto* hud = dynamic_cast<HudModule*>(module.get());
    if (hud == nullptr || !hud->isEnabled() || !hud->visible()) continue;
    const ImVec4 hide = coveredArea();
    const ImVec2 p = hud->positionOn(screen);
    const ImVec2 q(p.x + hud->lastSize().x, p.y + hud->lastSize().y);
    if (!editing && hide.z > hide.x && p.x < hide.z && q.x > hide.x && p.y < hide.w && q.y > hide.y) continue;
    hud->render(list, screen, uiScale);
    if (editing && !ImGui::GetIO().WantCaptureMouse && mouse.x >= p.x &&
        mouse.x <= q.x && mouse.y >= p.y && mouse.y <= q.y) {
      hovered = hud;
    }
  }
  if (!editing) return;

  if (dragging == nullptr && hovered != nullptr) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      dragging = hovered;
      const ImVec2 p = hovered->positionOn(screen);
      grab = ImVec2(mouse.x - p.x, mouse.y - p.y);
    }
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) hovered->setScale(hovered->scale() + wheel * 0.05f);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) hovered->resetPosition();
  }

  if (dragging != nullptr) {
    ImVec2 p(mouse.x - grab.x, mouse.y - grab.y);
    const ImVec2 size = dragging->lastSize();
    const float snap = 8.0f * uiScale;
    const float margin = 10.0f * uiScale;
    const float xs[] = {margin, (screen.x - size.x) * 0.5f, screen.x - size.x - margin};
    const float ys[] = {margin, (screen.y - size.y) * 0.5f, screen.y - size.y - margin};
    for (int i = 0; i < 3; ++i) {
      if (std::fabs(p.x - xs[i]) < snap) {
        p.x = xs[i];
        const float g = i == 0 ? margin : i == 1 ? screen.x * 0.5f : screen.x - margin;
        list.AddLine(ImVec2(g, 0), ImVec2(g, screen.y), IM_COL32(96, 192, 255, 200));
      }
      if (std::fabs(p.y - ys[i]) < snap) {
        p.y = ys[i];
        const float g = i == 0 ? margin : i == 1 ? screen.y * 0.5f : screen.y - margin;
        list.AddLine(ImVec2(0, g), ImVec2(screen.x, g), IM_COL32(96, 192, 255, 200));
      }
    }
    dragging->moveTo(p, screen);
  }

  for (const auto& module : manager.modules()) {
    auto* hud = dynamic_cast<HudModule*>(module.get());
    if (hud == nullptr || !hud->isEnabled() || !hud->visible()) continue;
    const ImVec2 p = hud->positionOn(screen);
    const ImVec2 q(p.x + hud->lastSize().x, p.y + hud->lastSize().y);
    const bool hot = hud == hovered || hud == dragging;
    list.AddRect(ImVec2(p.x - 2, p.y - 2), ImVec2(q.x + 2, q.y + 2),
                 hot ? IM_COL32(96, 192, 255, 255) : IM_COL32(150, 190, 230, 90), 6.0f, 0, hot ? 2.0f : 1.0f);
    if (hot) {
      char text[96];
      std::snprintf(text, sizeof(text), "%s  %.0f%%", hud->name().c_str(), hud->scale() * 100.0f);
      const float ty = p.y > 24.0f ? p.y - 22.0f : q.y + 6.0f;
      list.AddText(ImVec2(p.x, ty), IM_COL32(170, 220, 255, 255), text);
    }
  }
}

}  // namespace lex::platform::windows::hud
