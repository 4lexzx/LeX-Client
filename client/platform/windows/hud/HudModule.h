#pragma once

#include <imgui.h>

#include <string>

#include "core/module/Module.h"

namespace lex::platform::windows::hud {

// Elemento arrastrable del HUD. La posicion se guarda normalizada (0..1) sobre
// el espacio libre, asi sobrevive a cambios de resolucion.
class HudModule : public core::module::Module {
 public:
  HudModule(std::string name, std::string description, std::string icon,
            float x, float y, bool onByDefault = false);

  virtual bool visible() const { return true; }
  // Tamano en pixeles sin escalar.
  virtual ImVec2 measure() = 0;
  virtual void paint(ImDrawList& list, ImVec2 origin, float scale) = 0;

  void render(ImDrawList& list, ImVec2 screen, float uiScale);

  float scale() const;
  void setScale(float value);
  ImVec2 positionOn(ImVec2 screen) const;
  void moveTo(ImVec2 topLeft, ImVec2 screen);
  ImVec2 lastSize() const noexcept { return size_; }

  nlohmann::json saveExtra() const override;
  void loadExtra(const nlohmann::json& value) override;
  void resetPosition();

 protected:
  ImU32 rainbow(float offset = 0.0f) const;
  ImU32 textColor(float offset = 0.0f) const;
  ImU32 fillColor() const;
  ImU32 accentColor() const;
  float rounding() const;
  bool shadow() const;
  void label(ImDrawList& list, ImVec2 at, float size, const std::string& text,
             float chromaOffset = 0.0f) const;
  void box(ImDrawList& list, ImVec2 min, ImVec2 max, float scale) const;

 private:
  float defaultX_;
  float defaultY_;
  float x_;
  float y_;
  ImVec2 size_{0.0f, 0.0f};
};

// Efecto a pantalla completa o centrado, sin posicion editable.
class ScreenModule : public core::module::Module {
 public:
  using Module::Module;
  virtual void paint(ImDrawList& list, ImVec2 screen, float uiScale) = 0;
};

// Zona tapada por la lista de jugadores: el HUD que caiga dentro se oculta.
ImVec4& coveredArea();
void registerAllModules();
// Falso si el juego no tiene esa opcion (se oculta en el menu).
// Aviso en vivo para las opciones del modulo (vacio si no hay).
std::string moduleStatus(const core::module::Module& module);
bool settingAvailable(const core::module::Module& module, const std::string& name);
void drawHud(bool editing, float uiScale);

struct Fonts {
  ImFont* regular = nullptr;
  ImFont* bold = nullptr;
};
Fonts& fonts();

}  // namespace lex::platform::windows::hud
