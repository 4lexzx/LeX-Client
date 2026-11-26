#include <windows.h>
#include <timeapi.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

#include "core/module/ModuleManager.h"
#include "core/platform/IPlatform.h"
#include "platform/windows/game/Cosmetics.h"
#include "platform/windows/game/LiveTextures.h"
#include "platform/windows/game/ItemPhysics.h"
#include "platform/windows/game/Memory.h"
#include "platform/windows/game/PlayerList.h"
#include "platform/windows/game/Telemetry.h"
#include "platform/windows/gui/TextureStore.h"
#include "platform/windows/hud/Crosshair.h"
#include "platform/windows/hud/HudModule.h"
#include "platform/windows/input/Input.h"

namespace lex::platform::windows::hud {
namespace {

using namespace lex::core::settings;
using core::module::Category;

constexpr float kText = 18.0f;

float textWidth(const std::string& text, float size) {
  ImFont* font = fonts().bold != nullptr ? fonts().bold : ImGui::GetFont();
  return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x;
}

std::string format(const char* pattern, ...) {
  char buffer[128];
  va_list args;
  va_start(args, pattern);
  std::vsnprintf(buffer, sizeof(buffer), pattern, args);
  va_end(args);
  return buffer;
}

template <typename S>
S& setting(core::module::Module& module, const char* name) {
  return *static_cast<S*>(module.settings().find(name));
}

bool editing() { return mouse_guard::menuOpen(); }

// Caja de una sola linea; cubre la mayoria de contadores.
class TextHud : public HudModule {
 public:
  using HudModule::HudModule;
  virtual std::string text() = 0;

  ImVec2 measure() override {
    value_ = text();
    return ImVec2(std::ceil(textWidth(value_, kText)) + 20.0f, 30.0f);
  }
  void paint(ImDrawList& list, ImVec2 o, float s) override {
    const ImVec2 size = lastSize();
    box(list, o, ImVec2(o.x + size.x, o.y + size.y), s);
    label(list, ImVec2(o.x + 10 * s, o.y + (30 - kText) * 0.5f * s), kText * s, value_);
  }

 private:
  std::string value_;
};

int armorDurability(int material, int slot) {
    static constexpr int kTable[10][4] = {{0, 0, 0, 0},         {55, 80, 75, 65},     {165, 240, 225, 195}, {165, 240, 225, 195}, {363, 528, 495, 429},
                                          {77, 112, 105, 91},   {432, 432, 432, 432}, {275, 275, 275, 275}, {407, 592, 555, 481}, {121, 176, 165, 143}};
    return kTable[std::clamp(material, 0, 9)][slot];
}

// ---------------------------------------------------------------- HUD
class Fps final : public TextHud {
 public:
  Fps() : TextHud("FPS", "Fotogramas reales por segundo", "icon.module.fps", 0.008f, 0.014f, true) {}
  std::string text() override { return format("%.0f FPS", platform() ? platform()->fps() : 0.0f); }
};

class LowFps final : public TextHud {
 public:
  LowFps() : TextHud("FPS mínimos", "El 1% más lento: delata los tirones", "icon.module.fps_limit", 0.008f, 0.118f) {}
  std::string text() override {
    data_[cursor_++ % data_.size()] = ImGui::GetIO().DeltaTime;
    clock_ += ImGui::GetIO().DeltaTime;
    if (clock_ > 0.5f) {
      clock_ = 0;
      auto sorted = data_;
      const std::size_t n = std::min(cursor_, data_.size());
      std::sort(sorted.begin(), sorted.begin() + n, std::greater<float>());
      const float worst = sorted[std::min<std::size_t>(n / 100, n - 1)];
      low_ = worst > 0 ? 1.0f / worst : 0.0f;
    }
    return format("%.0f mín", low_);
  }

 private:
  std::array<float, 600> data_{};
  std::size_t cursor_ = 0;
  float clock_ = 0;
  float low_ = 0;
};

class Cps final : public TextHud {
 public:
  Cps() : TextHud("CPS", "Clics por segundo, ambos botones", "icon.module.cps", 0.008f, 0.066f, true) {
    settings().add<BoolSetting>("Clic derecho", "Incluye el botón derecho", true);
  }
  std::string text() override {
    const Input& in = Input::instance();
    return setting<BoolSetting>(*this, "Clic derecho").value() ? format("%d | %d CPS", in.leftCps(), in.rightCps()) : format("%d CPS", in.leftCps());
  }
};

class Combo final : public TextHud {
 public:
  Combo() : TextHud("Combo", "Golpes seguidos a jugadores sin recibir", "icon.module.hurtcam", 0.5f, 0.62f, true) {
    settings().add<BoolSetting>("Ocultar a cero", "Solo se ve durante un combo", true);
  }
  bool visible() const override {
    update();
    return editing() || combo_ > 0 || !static_cast<const BoolSetting*>(settings().find("Ocultar a cero"))->value();
  }
  std::string text() override { return combo_ == 1 ? "1 golpe" : format("%d golpes", combo_); }

 private:
  void update() const {
    const auto hit = game::lastHit();
    const ULONGLONG now = GetTickCount64();
    if (hit.serial != serial_) {
      serial_ = hit.serial;
      // Solo cuenta si tu acababas de atacar: asi no suma peleas ajenas.
      if (now - Input::instance().lastLeftClick() < 600) {
        ++combo_;
        last_ = now;
      }
    }
    if (combo_ > 0 && (game::playerData().hurt || now - last_ > 3000)) combo_ = 0;
  }
  mutable unsigned serial_ = 0;
  mutable int combo_ = 0;
  mutable ULONGLONG last_ = 0;
};

class Reach final : public TextHud {
 public:
  Reach() : TextHud("Alcance", "Distancia de tu último golpe a un jugador", "icon.module.zoom", 0.5f, 0.67f, true) {}
  bool visible() const override {
    const auto hit = game::lastHit();
    if (hit.serial != serial_) {
      serial_ = hit.serial;
      if (GetTickCount64() - Input::instance().lastLeftClick() < 600) {
        distance_ = hit.distance;
        time_ = GetTickCount64();
      }
    }
    return editing() || (time_ != 0 && GetTickCount64() - time_ < 2500);
  }
  std::string text() override { return format("%.1f bloques", distance_); }

 private:
  mutable unsigned serial_ = 0;
  mutable float distance_ = 0.0f;
  mutable ULONGLONG time_ = 0;
};

// Golpes que conectan entre clics hechos, en esta pelea.
class Accuracy final : public TextHud {
 public:
  Accuracy() : TextHud("Precisión", "Porcentaje de clics que golpean", "icon.module.crosshair", 0.5f, 0.86f) {}
  std::string text() override {
    const ULONGLONG now = GetTickCount64();
    const auto& input = Input::instance();
    const game::Hit hit = game::lastHit();
    if (hit.serial != serial_) {
      if (now - last_ > 8000) {  // pelea nueva
        hits_ = 0;
        base_ = input.totalClicks() > 0 ? input.totalClicks() - 1 : 0;
      }
      serial_ = hit.serial;
      if (now - input.lastLeftClick() < 700) {
        ++hits_;
        last_ = now;
      }
    }
    if (hits_ == 0 || now - last_ > 8000) return "Precisión --";
    const unsigned long long clicks = std::max<unsigned long long>(hits_, input.totalClicks() - base_);
    return format("Precisión %d%%", static_cast<int>(hits_ * 100 / clicks));
  }

 private:
  unsigned serial_ = 0;
  unsigned long long hits_ = 0;
  unsigned long long base_ = 0;
  ULONGLONG last_ = 0;
};

// Bajas de la partida en un servidor: muertes a tu lado justo despues de
// atacar. Vuelve a cero al morir y al cambiar de partida (la pantalla de carga
// entre mapas corta el HUD mas de 1.5 s). En un mundo propio no se muestra.
class Kills final : public TextHud {
 public:
  Kills() : TextHud("Kills", "Bajas de la partida en servidores; vuelve a cero al morir o cambiar de partida", "icon.module.hurtcam", 0.992f, 0.30f) {
    settings().add<BoolSetting>("Récord", "Mejor partida de la sesión", true);
    setEnabled(true);
  }
  bool visible() const override {
    game::PlayerList::instance().touch();
    return game::PlayerList::instance().snapshot().size() > 1;
  }
  std::string text() override {
    const ULONGLONG now = GetTickCount64();
    if (last_ != 0 && now - last_ > 1500) kills_ = 0;  // otra partida
    last_ = now;
    const unsigned confirmed = game::confirmedKills();
    if (confirmed != seen_) {
      if (seen_ != ~0u) kills_ += static_cast<int>(confirmed - seen_);
      seen_ = confirmed;
    }
    const auto p = game::playerData();
    const bool dead = p.valid && p.health <= 0;
    if (dead && !dead_) kills_ = 0;
    dead_ = dead;
    best_ = std::max(best_, kills_);
    std::string out = format("Kills %d", kills_);
    if (setting<BoolSetting>(*this, "Récord").value() && best_ > 0) out += format("  Récord %d", best_);
    return out;
  }

 private:
  int kills_ = 0, best_ = 0;
  unsigned seen_ = ~0u;
  bool dead_ = false;
  ULONGLONG last_ = 0;
};

class CombatTimer final : public TextHud {
 public:
  CombatTimer() : TextHud("Tiempo de combate", "Cuánto llevas peleando sin parar", "icon.module.clock", 0.5f, 0.10f) {}
  std::string text() override {
    const ULONGLONG now = GetTickCount64();
    const bool active = now - game::lastHit().time < 8000 || game::playerData().hurt;
    if (active) last_ = now;
    if (last_ == 0 || now - last_ > 8000) {
      start_ = now;
      return "Sin combate";
    }
    const int seconds = static_cast<int>((now - start_) / 1000);
    return format("Combate %d:%02d", seconds / 60, seconds % 60);
  }

 private:
  ULONGLONG start_ = 0;
  ULONGLONG last_ = 0;
};

class Clock final : public TextHud {
 public:
  Clock() : TextHud("Reloj", "Hora de tu PC", "icon.module.clock", 1.0f, 0.02f) {}
  std::string text() override {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return format("%02d:%02d", t.wHour, t.wMinute);
  }
};

class Blocks final : public TextHud {
 public:
  Blocks() : TextHud("Bloques", "Cuántos bloques tienes en la mano", "icon.module.nametag", 0.5f, 0.72f, true) {}
  bool visible() const override {
    const auto p = game::playerData();
    return editing() || (p.valid && p.count > 0);
  }
  std::string text() override { return format("Blocks: %d", game::playerData().count); }
};

class Coordinates final : public TextHud {
 public:
  Coordinates() : TextHud("Coordenadas", "Tu posición X, Y, Z", "icon.module.coordinates", 0.5f, 0.014f) {}
  std::string text() override {
    const auto p = game::playerData();
    return format("XYZ  %d  %d  %d", p.x, p.y, p.z);
  }
};

class Compass final : public TextHud {
 public:
  Compass() : TextHud("Rumbo", "Punto cardinal hacia el que avanzas", "icon.module.coordinates", 0.5f, 0.066f) {}
  std::string text() override {
    static constexpr const char* kNames[] = {"Sur  +Z", "Oeste  -X", "Norte  -Z", "Este  +X"};
    const auto p = game::playerData();
    if (p.valid && p.facing >= 0 && p.facing < 4) last_ = p.facing;  // quieto: conserva el ultimo
    return kNames[last_];
  }

 private:
  int last_ = 2;
};

class Speed final : public TextHud {
 public:
  Speed() : TextHud("Velocidad", "Bloques por segundo", "icon.module.speedometer", 0.008f, 0.17f) {}
  std::string text() override { return format("%.1f b/s", game::playerData().speed); }
};

class Health final : public TextHud {
 public:
  Health() : TextHud("Vida", "Tu vida en número", "icon.module.hurtcam", 0.4f, 0.8f) {}
  std::string text() override { return format("%d HP", game::playerData().health); }
};

class Armor final : public HudModule {
 public:
  Armor() : HudModule("Armadura", "Piezas con su textura y durabilidad", "icon.module.armor", 0.992f, 0.55f, true) {
    settings().add<BoolSetting>("Horizontal", "Coloca las piezas en fila", false);
    settings().add<EnumSetting>("Durabilidad", "Qué se muestra", 0).withOptions({"Porcentaje", "Puntos restantes", "Nada"});
    settings().add<BoolSetting>("Color según desgaste", "Verde, amarillo o rojo", true);
  }
  bool visible() const override {
    if (editing()) return true;
    const auto p = game::playerData();
    return p.armor[0] + p.armor[1] + p.armor[2] + p.armor[3] > 0;
  }
  ImVec2 measure() override {
    const auto p = game::playerData();
    pieces_.clear();
    for (int i = 0; i < 4; ++i) {
      // En el editor se enseña un ejemplo para poder colocarlo.
      const bool sample = editing() && p.armor[0] + p.armor[1] + p.armor[2] + p.armor[3] == 0;
      const int material = sample ? 4 : p.armor[i];
      if (material > 0) pieces_.push_back({i, material, sample ? 40 * i : p.damage[i]});
    }
    const int n = std::max<int>(1, static_cast<int>(pieces_.size()));
    return horizontal() ? ImVec2(cell() * n + 4.0f * (n - 1), 40) : ImVec2(cell(), 40.0f * n + 4.0f * (n - 1));
  }
  void paint(ImDrawList& list, ImVec2 o, float s) override {
    for (std::size_t i = 0; i < pieces_.size(); ++i) {
      const Piece& piece = pieces_[i];
      const ImVec2 p(o.x + (horizontal() ? i * (cell() + 4) * s : 0), o.y + (horizontal() ? 0 : i * 44.0f * s));
      box(list, p, ImVec2(p.x + cell() * s, p.y + 40 * s), s);
      const auto* icon = gui::TextureStore::instance().find("item." + name(piece));
      if (icon != nullptr) list.AddImage(icon->texture, ImVec2(p.x + 4 * s, p.y + 4 * s), ImVec2(p.x + 36 * s, p.y + 36 * s));
      const int max = durability(piece);
      const float ratio = max > 0 ? std::clamp(1.0f - static_cast<float>(piece.damage) / max, 0.0f, 1.0f) : 1.0f;
      const std::size_t mode = setting<EnumSetting>(*this, "Durabilidad").value();
      const ImU32 tone = ratio > 0.5f ? IM_COL32(94, 214, 122, 255) : ratio > 0.2f ? IM_COL32(244, 196, 72, 255) : IM_COL32(240, 84, 84, 255);
      if (mode < 2) {
        const std::string t = mode == 0 ? format("%.0f%%", std::ceil(ratio * 100)) : format("%d", std::max(0, max - piece.damage));
        const ImVec2 at(p.x + 42 * s, p.y + 8 * s);
        if (setting<BoolSetting>(*this, "Color según desgaste").value()) {
          ImFont* font = fonts().bold != nullptr ? fonts().bold : ImGui::GetFont();
          list.AddText(font, 17 * s, ImVec2(at.x + s, at.y + s), IM_COL32(0, 0, 0, 170), t.c_str());
          list.AddText(font, 17 * s, at, tone, t.c_str());
        } else {
          label(list, at, 17 * s, t);
        }
        list.AddRectFilled(ImVec2(p.x + 42 * s, p.y + 30 * s), ImVec2(p.x + 88 * s, p.y + 33 * s), IM_COL32(0, 0, 0, 150));
        list.AddRectFilled(ImVec2(p.x + 42 * s, p.y + 30 * s), ImVec2(p.x + (42 + 46 * ratio) * s, p.y + 33 * s), tone);
      }
    }
  }

 private:
  struct Piece {
    int slot;
    int material;
    int damage;
  };
  bool horizontal() { return setting<BoolSetting>(*this, "Horizontal").value(); }
  float cell() { return setting<EnumSetting>(*this, "Durabilidad").value() < 2 ? 96.0f : 40.0f; }

  // Orden de query.armor_texture_slot + 1: cuero, malla, hierro, diamante, oro, elitros, tortuga, netherita, cobre.
  static std::string name(const Piece& p) {
    static constexpr const char* kMaterial[] = {"", "leather", "chainmail", "iron", "diamond", "gold", "elytra", "turtle", "netherite", "copper"};
    static constexpr const char* kPiece[] = {"helmet", "chestplate", "leggings", "boots"};
    if (p.material == 6) return "elytra";
    return std::string(kMaterial[std::clamp(p.material, 0, 9)]) + "_" + kPiece[p.slot];
  }
  static int durability(const Piece& p) { return armorDurability(p.material, p.slot); }
  std::vector<Piece> pieces_;
};

class Keystrokes final : public HudModule {
 public:
  Keystrokes() : HudModule("Teclas", "WASD, clics y espacio en vivo", "icon.module.keystrokes", 0.008f, 0.98f, true) {
    settings().add<BoolSetting>("Botones del ratón", "Muestra LMB y RMB con CPS", true);
    settings().add<BoolSetting>("Barra espaciadora", "Muestra el salto", true);
  }
  ImVec2 measure() override {
    float h = 92;
    if (setting<BoolSetting>(*this, "Botones del ratón").value()) h += 44;
    if (setting<BoolSetting>(*this, "Barra espaciadora").value()) h += 28;
    return ImVec2(140, h);
  }
  void paint(ImDrawList& list, ImVec2 o, float s) override {
    const Input& in = Input::instance();
    key(list, o, s, 48, 0, 44, 44, "W", in.down('W'), 0);
    key(list, o, s, 0, 48, 44, 44, "A", in.down('A'), 1);
    key(list, o, s, 48, 48, 44, 44, "S", in.down('S'), 2);
    key(list, o, s, 96, 48, 44, 44, "D", in.down('D'), 3);
    float y = 96;
    if (setting<BoolSetting>(*this, "Botones del ratón").value()) {
      key(list, o, s, 0, y, 68, 40, format("LMB %d", in.leftCps()), in.down(VK_LBUTTON), 4, 14);
      key(list, o, s, 72, y, 68, 40, format("RMB %d", in.rightCps()), in.down(VK_RBUTTON), 5, 14);
      y += 44;
    }
    if (setting<BoolSetting>(*this, "Barra espaciadora").value()) key(list, o, s, 0, y, 140, 24, "ESPACIO", in.down(VK_SPACE), 6, 12);
  }

 private:
  void key(ImDrawList& list, ImVec2 o, float s, float x, float y, float w, float h, const std::string& text, bool down, int index, float size = 18) {
    float& a = glow_[index];
    a += ((down ? 1.0f : 0.0f) - a) * std::min(1.0f, ImGui::GetIO().DeltaTime * 20.0f);
    const ImVec2 p(o.x + x * s, o.y + y * s);
    const ImVec2 q(p.x + w * s, p.y + h * s);
    box(list, p, q, s);
    if (a > 0.01f) {
      ImVec4 c = ImGui::ColorConvertU32ToFloat4(accentColor());
      c.w *= a * 0.85f;
      list.AddRectFilled(p, q, ImGui::ColorConvertFloat4ToU32(c), rounding() * s);
    }
    label(list, ImVec2(p.x + (w - textWidth(text, size)) * 0.5f * s, p.y + (h - size) * 0.5f * s), size * s, text, index * 0.06f);
  }
  std::array<float, 7> glow_{};
};

// ---------------------------------------------------------------- opciones del juego
// Cambia una opcion mientras el modulo esta activo y la devuelve al apagarlo.
class OptionModule : public ScreenModule {
 public:
  OptionModule(std::string name, std::string description, Category category, std::string icon, std::string tag, float on)
      : ScreenModule(std::move(name), std::move(description), category, std::move(icon)), tag_(std::move(tag)), on_(on) {}
  virtual float target() { return on_; }
  void onDisable() override {
    if (hasOriginal_) game::Options::instance().set(tag_, original_);
    hasOriginal_ = false;
  }
  void paint(ImDrawList&, ImVec2, float) override {
    auto& options = game::Options::instance();
    if (!options.ready()) return;
    if (!hasOriginal_) hasOriginal_ = options.get(tag_, original_);
    if (hasOriginal_) options.set(tag_, target());
  }
  // El original sobrevive a un cierre: si no, el juego guardaria el cambio para siempre.
  nlohmann::json saveExtra() const override {
    return hasOriginal_ ? nlohmann::json{{"original", original_}} : nlohmann::json::object();
  }
  void loadExtra(const nlohmann::json& v) override {
    if (v.is_object() && v.contains("original") && v["original"].is_number() && !hasOriginal_) {
      original_ = v["original"].get<float>();
      hasOriginal_ = true;
    }
  }

 private:
  std::string tag_;
  float on_;
  float original_ = 0.0f;
  bool hasOriginal_ = false;
};

class Fullbright final : public OptionModule {
 public:
  Fullbright() : OptionModule("Brillo total", "Ve en la oscuridad subiendo el brillo", Category::Visual, "icon.module.fullbright", "gfx_gamma", 1.0f) {
    settings().add<FloatSetting>("Intensidad", "1 es el máximo del juego", 6.0f).withRange(1.0f, 15.0f);
  }
  float target() override { return setting<FloatSetting>(*this, "Intensidad").value(); }
};

class FovChanger final : public OptionModule {
 public:
  FovChanger() : OptionModule("Campo de visión", "FOV más allá del límite del juego", Category::Visual, "icon.module.zoom", "gfx_field_of_view", 90.0f) {
    settings().add<FloatSetting>("FOV", "Grados", 90.0f).withRange(30.0f, 140.0f);
  }
  float target() override { return setting<FloatSetting>(*this, "FOV").value(); }
};

class NoHurtCam final : public OptionModule {
 public:
  NoHurtCam() : OptionModule("Sin sacudida", "La cámara no tiembla al recibir daño", Category::Visual, "icon.module.hurtcam", "gfx_damagebobbing", 0.0f) {}
};

class NoBobbing final : public OptionModule {
 public:
  NoBobbing() : OptionModule("Sin balanceo", "La cámara no se mueve al caminar", Category::Visual, "icon.module.speedometer", "gfx_viewbobbing", 0.0f) {}
};

class BlockOutline final : public OptionModule {
 public:
  BlockOutline() : OptionModule("Borde de bloque", "Contorno en el bloque que miras", Category::Visual, "icon.module.nametag", "gfx_classic_box_selection", 1.0f) {}
};

class AutoSprint final : public OptionModule {
 public:
  AutoSprint() : OptionModule("Sprint automático", "Corre siempre que avanzas", Category::Utility, "icon.module.toggle_sprint", "ctrl_sprintonmovement", 1.0f) {}
};

class EnchantGlint final : public OptionModule {
 public:
  EnchantGlint() : OptionModule("Brillo de encantamiento", "Menos destello en espadas y armaduras encantadas", Category::Visual, "icon.star", "glint_strength", 0.0f) {
    settings().add<FloatSetting>("Intensidad", "0 lo quita", 0.0f).withRange(0.0f, 1.0f);
  }
  float target() override { return setting<FloatSetting>(*this, "Intensidad").value(); }
};

class HideHand final : public OptionModule {
 public:
  HideHand() : OptionModule("Sin mano", "Oculta la mano y el objeto para ver más", Category::Visual, "icon.module.nametag", "gfx_hidehand", 1.0f) {}
};

class GuiOpacity final : public OptionModule {
 public:
  GuiOpacity() : OptionModule("Interfaz transparente", "Opacidad de la barra y el HUD del juego", Category::Visual, "icon.module.screenshot", "gfx_interface_opacity", 0.6f) {
    settings().add<FloatSetting>("Opacidad", "1 es normal", 0.6f).withRange(0.1f, 1.0f);
  }
  float target() override { return setting<FloatSetting>(*this, "Opacidad").value(); }
};

class ClearVision final : public OptionModule {
 public:
  ClearVision() : OptionModule("Visión clara", "Sin distorsión por náusea o portales", Category::Visual, "icon.module.fullbright", "screen_distortion", 0.0f) {}
};

class CapePhysics final : public ScreenModule {
 public:
  CapePhysics() : ScreenModule("Física de capa", "La capa LeX ondea al correr, saltar y girar", Category::Visual, "icon.category.cosmetics") { setEnabled(true); }
  void onDisable() override { game::Cosmetics::instance().setCapePhysics(false); }
  void paint(ImDrawList&, ImVec2, float) override { game::Cosmetics::instance().setCapePhysics(true); }
};

// Tiñe a otros jugadores con tu color al recibir daño.
class HitColor final : public ScreenModule {
 public:
  HitColor() : ScreenModule("Color de golpe", "Color de los jugadores al recibir daño", Category::Visual, "icon.module.hurtcam") {
    settings().add<ColorSetting>("Color", "Color e intensidad", Color{0.25f, 0.65f, 1.0f, 0.7f});
    settings().add<BoolSetting>("RGB", "Recorre el arcoíris", false);
    setEnabled(true);
  }
  void onDisable() override {
    last_ = 0;
    publish(0);
  }
  void paint(ImDrawList&, ImVec2, float) override {
    Color c = setting<ColorSetting>(*this, "Color").value();
    if (setting<BoolSetting>(*this, "RGB").value()) {
      ImGui::ColorConvertHSVtoRGB(std::floor(std::fmod(static_cast<float>(ImGui::GetTime()) * 0.35f, 1.0f) * 24.0f) / 24.0f, 0.9f, 1.0f, c.r, c.g, c.b);
    }
    const ImU32 now = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
    if (now == last_) return;
    last_ = now;
    publish(now);
  }

 private:
  static void publish(ImU32 color) {
    std::vector<std::uint8_t> fill(71 * 71 * 4);
    for (std::size_t i = 0; i < fill.size(); i += 4) std::memcpy(&fill[i], &color, 4);
    game::LiveTextures::instance().set(game::Live::HitColor, 71, 71, std::move(fill));
  }
  ImU32 last_ = 0;
};

class ItemPhysicsModule final : public ScreenModule {
 public:
  ItemPhysicsModule() : ScreenModule("Física de objetos", "Los objetos tirados giran al caer y quedan en el suelo", Category::Visual, "icon.module.test") {
    setEnabled(true);
    settings().add<FloatSetting>("Giro al caer", "Cuántas vueltas dan", 1.0f).withRange(0.0f, 3.0f);
    settings().add<FloatSetting>("Altura", "Sube o baja el objeto", 0.0f).withRange(-0.5f, 0.5f);
  }
  void onDisable() override { game::ItemPhysics::configure(false, 0, 0); }
  void paint(ImDrawList&, ImVec2, float) override {
    game::ItemPhysics::configure(true, setting<FloatSetting>(*this, "Giro al caer").value(), setting<FloatSetting>(*this, "Altura").value());
  }
};

// Marca blanca en la mira cuando un golpe tuyo conecta.
class HitMarker final : public ScreenModule {
 public:
  HitMarker() : ScreenModule("Marcador de golpe", "Aspa en la mira cuando tu golpe conecta", Category::Visual, "icon.module.crosshair") {
    settings().add<ColorSetting>("Color", "Color del aspa", Color{1.0f, 1.0f, 1.0f, 1.0f});
    settings().add<FloatSetting>("Tamaño", "Largo de las líneas", 1.0f).withRange(0.5f, 2.5f);
  }
  void paint(ImDrawList& list, ImVec2 d, float ui) override {
    const game::Hit hit = game::lastHit();
    const ULONGLONG age = GetTickCount64() - hit.time;
    const ULONGLONG click = GetTickCount64() - Input::instance().lastLeftClick();
    if (hit.time == 0 || age > 260 || click > 700 || !Input::instance().gameplay()) return;
    Color c = setting<ColorSetting>(*this, "Color").value();
    c.a *= 1.0f - static_cast<float>(age) / 260.0f;
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
    const float k = ui * setting<FloatSetting>(*this, "Tamaño").value();
    const ImVec2 m(d.x * 0.5f, d.y * 0.5f);
    for (int i = 0; i < 4; ++i) {
      const float x = i % 2 == 0 ? 1.0f : -1.0f, y = i < 2 ? 1.0f : -1.0f;
      list.AddLine(ImVec2(m.x + x * 6 * k, m.y + y * 6 * k), ImVec2(m.x + x * 12 * k, m.y + y * 12 * k), color, 2.0f * k);
    }
  }
};

class ArmorWarning final : public ScreenModule {
 public:
  ArmorWarning() : ScreenModule("Aviso de armadura", "Avisa antes de que se rompa una pieza", Category::Utility, "icon.module.armor") {
    settings().add<IntSetting>("Avisar por debajo de", "Porcentaje de durabilidad", 15).withRange(5, 50);
  }
  void paint(ImDrawList& list, ImVec2 d, float ui) override {
    const auto p = game::playerData();
    if (!p.valid) return;
    static constexpr const char* kPiece[4] = {"Casco", "Peto", "Pantalón", "Botas"};
    int worst = -1, percent = 101;
    for (int i = 0; i < 4; ++i) {
      const int total = armorDurability(p.armor[i], i);
      if (total <= 0) continue;
      const int left = std::clamp((total - p.damage[i]) * 100 / total, 0, 100);
      if (left < percent) {
        percent = left;
        worst = i;
      }
    }
    if (worst < 0 || percent > setting<IntSetting>(*this, "Avisar por debajo de").value()) return;
    const std::string text = format("%s al %d%%", kPiece[worst], percent);
    const float size = 20 * ui;
    const float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.0f);
    const ImVec2 at((d.x - textWidth(text, size)) * 0.5f, d.y * 0.66f);
    ImFont* font = fonts().bold != nullptr ? fonts().bold : ImGui::GetFont();
    list.AddText(font, size, ImVec2(at.x + 2 * ui, at.y + 2 * ui), IM_COL32(0, 0, 0, 180), text.c_str());
    list.AddText(font, size, at, IM_COL32(255, 86, 72, static_cast<int>(255 * pulse)), text.c_str());
  }
};

// ---------------------------------------------------------------- visual
class PlayerHealth final : public ScreenModule {
 public:
  PlayerHealth() : ScreenModule("Vida de jugadores", "Vida sobre el nombre de otros jugadores", Category::Visual, "icon.module.nametag") {
    settings().add<BoolSetting>("Estimar si no la envían", "Calcula la vida por los golpes cuando el servidor no la publica (se marca con ~)", true);
    setEnabled(true);  // despues del ajuste: al activarse lo lee
  }
  void onEnable() override { publish(true); }
  void onDisable() override { publish(false); }
  void paint(ImDrawList&, ImVec2, float) override {
    const bool estimate = setting<BoolSetting>(*this, "Estimar si no la envían").value();
    if (!published_ || estimate != estimate_) publish(true);
  }

 private:
  // Corazon y numero en pixel art, con contorno y sin fondo.
  static bool pixel(int hp, bool estimated, int x, int y, std::uint8_t (&rgb)[3]) {
    static constexpr const char* kHeart[8] = {".##...##.", "####.####", "#########", "#########", ".#######.", "..#####..", "...###...", "....#...."};
    static constexpr const char* kDigits[11][7] = {
        {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}, {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
        {".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####"}, {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."},
        {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}, {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."},
        {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}, {"#####", "....#", "...#.", "...#.", "..#..", "..#..", "..#.."},
        {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}, {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."},
        {".....", ".....", ".#...", "#.#.#", "...#.", ".....", "....."}};  // el ultimo es ~
    const std::string text = (estimated ? ":" : "") + std::to_string(hp);
    const int total = 9 + 3 + static_cast<int>(text.size()) * 6 - 1;
    const int left = (kCellsX - total) / 2;
    const auto solid = [&](int cx, int cy, int& kind) {
      const int hx = cx - left, hy = cy - 2;
      if (hx >= 0 && hx < 9 && hy >= 0 && hy < 8 && kHeart[hy][hx] == '#') {
        kind = hx <= 3 && hx >= 1 && hy == 1 ? 1 : 0;
        return true;
      }
      const int dx = cx - left - 12, dy = cy - 2;
      if (dx >= 0 && dy >= 0 && dy < 7 && dx / 6 < static_cast<int>(text.size()) && dx % 6 < 5 && kDigits[text[dx / 6] - '0'][dy][dx % 6] == '#') {
        kind = 2;
        return true;
      }
      return false;
    };
    int kind = 0;
    if (solid(x, y, kind)) {
      static constexpr std::uint8_t kColor[3][3] = {{232, 42, 52}, {255, 150, 150}, {255, 255, 255}};
      std::copy_n(kColor[kind], 3, rgb);
      return true;
    }
    for (int ox = -1; ox <= 1; ++ox) {
      for (int oy = -1; oy <= 1; ++oy) {
        if (solid(x + ox, y + oy, kind)) {
          rgb[0] = rgb[1] = rgb[2] = 14;
          return true;
        }
      }
    }
    return false;
  }
  static constexpr int kCellsX = 42;
  static constexpr int kCellsY = 12;

  void publish(bool on) {
    for (int hp = 0; hp < game::kHealthSteps; ++hp) {
     for (const bool estimated : {false, true}) {
      const int width = (estimated ? 181 : 131) + hp;
      std::vector<std::uint8_t> plate(static_cast<std::size_t>(width) * 36 * 4, 0);
      for (int y = 0; on && hp > 0 && (!estimated || setting<BoolSetting>(*this, "Estimar si no la envían").value()) && y < 36; ++y) {
        for (int x = 0; x < width; ++x) {
          std::uint8_t rgb[3];
          if (!pixel(hp, estimated, x * kCellsX / width, y * kCellsY / 36, rgb)) continue;
          std::uint8_t* out = &plate[(static_cast<std::size_t>(y) * width + x) * 4];
          std::copy_n(rgb, 3, out);
          out[3] = 255;
        }
      }
      game::LiveTextures::instance().set(static_cast<game::Live>(static_cast<int>(estimated ? game::Live::Estimate : game::Live::Health) + hp), width, 36, std::move(plate));
     }
    }
    published_ = on;
    estimate_ = setting<BoolSetting>(*this, "Estimar si no la envían").value();
  }
  bool published_ = false;
  bool estimate_ = true;
};

class LowHealth final : public ScreenModule {
 public:
  LowHealth() : ScreenModule("Aviso de vida baja", "Borde rojo cuando te queda poca vida", Category::Visual, "icon.module.hurtcam") {
    settings().add<IntSetting>("Avisar por debajo de", "Puntos de vida", 6).withRange(1, 19);
  }
  void paint(ImDrawList& list, ImVec2 d, float) override {
    const auto p = game::playerData();
    if (!p.valid || p.health <= 0 || p.health > setting<IntSetting>(*this, "Avisar por debajo de").value()) return;
    const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
    const ImU32 c = IM_COL32(220, 30, 40, static_cast<int>(130 * pulse));
    const ImU32 n = IM_COL32(220, 30, 40, 0);
    const float r = 0.16f;
    list.AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(d.x, d.y * r), c, c, n, n);
    list.AddRectFilledMultiColor(ImVec2(0, d.y * (1 - r)), d, n, n, c, c);
    list.AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(d.x * r * 0.6f, d.y), c, n, n, c);
    list.AddRectFilledMultiColor(ImVec2(d.x * (1 - r * 0.6f), 0), d, n, c, c, n);
  }
};

// Lista de jugadores al mantener TAB, como en Java.
class TabList final : public ScreenModule {
 public:
  TabList() : ScreenModule("Lista de jugadores", "Mantén TAB para ver quién está en línea", Category::Utility, "icon.module.nametag") {
    setEnabled(true);
    Keybind key;
    key.key = VK_TAB;
    settings().add<KeybindSetting>("Tecla", "Mantener pulsada", key);
    settings().add<BoolSetting>("Caras", "Cara de la skin de cada jugador", true);
    settings().add<BoolSetting>("Logo LeX", "Marca a quienes usan LeX Client", true);
    settings().add<FloatSetting>("Tamaño", "Escala de la lista", 1.0f).withRange(0.6f, 2.0f);
    settings().add<ColorSetting>("Color de fondo", "Relleno del panel", Color{0.0f, 0.0f, 0.0f, 0.55f});
  }
  void paint(ImDrawList& list, ImVec2 d, float ui) override {
    if (!game::inWorld()) return;
    auto& source = game::PlayerList::instance();
    source.touch();
    const auto& input = Input::instance();
    if (editing() || !input.gameplay() || input.down(VK_MENU) || !input.down(setting<KeybindSetting>(*this, "Tecla").value().key)) return;

    const auto players = source.snapshot();
    const float s = ui * setting<FloatSetting>(*this, "Tamaño").value();
    const bool faces = setting<BoolSetting>(*this, "Caras").value();
    const bool logos = setting<BoolSetting>(*this, "Logo LeX").value();
    const auto* logo = gui::TextureStore::instance().find("logo.small");
    const float row = 18 * s, gap = 1 * s, pad = 5 * s, text = 13.5f * s, icon = 14 * s, mark = 17 * s;
    ImFont* font = fonts().bold != nullptr ? fonts().bold : ImGui::GetFont();

    const std::size_t count = players.size();
    const std::size_t fit = std::max<std::size_t>(1, static_cast<std::size_t>(d.y * 0.5f / (row + gap)));
    const std::size_t rows = std::min(fit, std::max(std::min<std::size_t>(count, 20), (count + 4) / 5));
    const std::size_t columns = count == 0 ? 1 : std::min<std::size_t>(7, (count + rows - 1) / rows);
    float widest = 70 * s;
    for (const auto& p : players) widest = std::max(widest, textWidth(p.name, text));
    const float cell = widest + 8 * s + (faces ? icon + 4 * s : 0) + (logos ? mark + 3 * s : 0);
    const std::string title = count == 0 ? "Buscando jugadores..." : format(count == 1 ? "%d jugador en línea" : "%d jugadores en línea", static_cast<int>(count));
    const float width = std::max(columns * cell + (columns - 1) * gap * 2, textWidth(title, text) + 12 * s) + pad * 2;
    const float header = 19 * s;
    const float height = header + (count == 0 ? 0 : rows * (row + gap) + pad);
    const ImVec2 o(std::floor((d.x - width) * 0.5f), std::floor(d.y * 0.02f));
    coveredArea() = ImVec4(o.x, o.y, o.x + width, o.y + height);
    const Color bg = setting<ColorSetting>(*this, "Color de fondo").value();
    list.AddRectFilled(o, ImVec2(o.x + width, o.y + height), ImGui::ColorConvertFloat4ToU32(ImVec4(bg.r, bg.g, bg.b, bg.a)), 4 * s);
    const auto write = [&](ImVec2 at, ImU32 color, const std::string& value) {
      list.AddText(font, text, ImVec2(at.x + s, at.y + s), IM_COL32(0, 0, 0, 170), value.c_str());
      list.AddText(font, text, at, color, value.c_str());
    };
    write(ImVec2(o.x + (width - textWidth(title, text)) * 0.5f, o.y + 3 * s), IM_COL32(230, 236, 245, 255), title);
    for (std::size_t i = 0; i < std::min(count, rows * columns); ++i) {
      const auto& p = players[i];
      const ImVec2 c(o.x + pad + (i / rows) * (cell + gap * 2), o.y + header + (i % rows) * (row + gap));
      list.AddRectFilled(c, ImVec2(c.x + cell, c.y + row), IM_COL32(255, 255, 255, 28), 2 * s);
      float x = c.x + 4 * s;
      const float iy = std::floor(c.y + (row - icon) * 0.5f);
      if (faces) {
        const float px = icon / 8;
        for (int k = 0; k < 64; ++k) {
          const ImU32 color = p.hasFace ? p.face[static_cast<std::size_t>(k)] : IM_COL32(95, 105, 120, 255);
          list.AddRectFilled(ImVec2(std::floor(x + (k % 8) * px), std::floor(iy + (k / 8) * px)), ImVec2(std::floor(x + (k % 8 + 1) * px), std::floor(iy + (k / 8 + 1) * px)), color);
        }
        x += icon + 4 * s;
      }
      if (logos) {
        if (p.lex && logo != nullptr && logo->texture != 0) list.AddImage(logo->texture, ImVec2(x, c.y + (row - mark) * 0.5f), ImVec2(x + mark, c.y + (row + mark) * 0.5f));
        x += mark + 3 * s;
      }
      write(ImVec2(x, c.y + (row - text) * 0.5f), p.self ? IM_COL32(255, 224, 130, 255) : IM_COL32_WHITE, p.name);
    }
  }
};

// ---------------------------------------------------------------- utilidad
// Zoom real: estrecha el campo de vision del juego, sin ampliar la imagen.
class Zoom final : public ScreenModule {
 public:
  Zoom() : ScreenModule("Zoom", "Acerca la vista con calidad, como en Java", Category::Utility, "icon.module.zoom") {
    setEnabled(true);
    Keybind key;
    key.key = 'R';
    settings().add<KeybindSetting>("Tecla de zoom", "Mantener pulsada", key);
    settings().add<FloatSetting>("Aumento", "Cuánto se acerca", 4.0f).withRange(1.5f, 12.0f);
    settings().add<BoolSetting>("Suave", "Transición animada", true);
  }
  void onDisable() override { release(); }
  void paint(ImDrawList&, ImVec2, float) override {
    auto& options = game::Options::instance();
    if (!options.ready()) return;
    const bool held = !editing() && Input::instance().gameplay() && Input::instance().down(setting<KeybindSetting>(*this, "Tecla de zoom").value().key);
    if (held && !active_) {
      if (!options.get("gfx_field_of_view", base_) || base_ < 20.0f) return;
      active_ = true;
      amount_ = 1.0f;
    }
    if (!active_) return;
    const float target = held ? setting<FloatSetting>(*this, "Aumento").value() : 1.0f;
    const float speed = setting<BoolSetting>(*this, "Suave").value() ? std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f) : 1.0f;
    amount_ += (target - amount_) * speed;
    if (!held && amount_ < 1.02f) {
      release();
      return;
    }
    constexpr float kRad = 3.14159265f / 180.0f;
    options.set("gfx_field_of_view", 2.0f * std::atan(std::tan(base_ * 0.5f * kRad) / amount_) / kRad);
  }

 private:
  void release() {
    if (active_) game::Options::instance().set("gfx_field_of_view", base_);
    active_ = false;
  }
  bool active_ = false;
  float base_ = 0.0f;
  float amount_ = 1.0f;
};

// Optimizador: cada ajuste se puede activar por separado y se deshace solo.
class Optimizer final : public ScreenModule {
 public:
  struct Tweak {
    const char* name;
    const char* help;
    const char* tag;
    float value;
    bool byDefault;
  };
  static constexpr Tweak kTweaks[] = {
      {"Sin sincronía vertical", "Quita el tope de FPS", "gfx_vsync", 0, true},
      {"Sin nubes", "No dibuja nubes", "gfx_toggleclouds", 0, true},
      {"Cielo simple", "Sin efectos en el cielo", "gfx_fancyskies", 0, true},
      {"Luz simple", "Sin sombreado suave", "gfx_smoothlighting", 0, true},
      {"Hojas opacas", "Los árboles cuestan menos", "gfx_transparentleaves", 0, true},
      {"Sin burbujas", "Sin partículas de burbuja", "gfx_bubble_particles", 0, true},
      {"Sin animaciones de menús", "Menús instantáneos", "screen_animations", 0, true},
      {"Sin muñeco", "Oculta el personaje de la esquina", "gfx_hidepaperdoll", 1, true},
      {"Sin niebla volumétrica", "Niebla simple", "volumetric_fog_quality", 0, true},
      {"Sin sombras", "Sombras desactivadas", "shadow_quality", 0, true},
      {"Sin vigilar archivos", "El juego deja de revisar sus carpetas", "dev_file_watcher", 0, true},
      {"Sin destello del End", "Quita el destello de pantalla", "hide_endflash", 1, true},
      {"Sin tramado", "No calcula transparencias cercanas", "enable_dithering_mobs", 0, true},
      {"Luz de bloques simple", "Luces de antorchas simples", "block_light_quality", 0, true},
      {"Sin reflejos", "Reflejos al mínimo", "reflections_quality", 0, true},
      {"Sin suavizado", "Antialiasing al mínimo", "gfx_msaa", 1, false},
      {"Sin gráficos elegantes", "Modo gráfico simple", "graphics_mode", 0, false},
  };
  Optimizer() : ScreenModule("Optimizador", "Más FPS. Algunos cambios se ven al reentrar al mundo", Category::Utility, "icon.module.fps_limit") {
    for (const Tweak& t : kTweaks) settings().add<BoolSetting>(t.name, t.help, t.byDefault);
    settings().add<BoolSetting>("Limitar distancia", "Reduce los chunks visibles", true);
    settings().add<IntSetting>("Distancia máxima", "Chunks", 6).withRange(4, 24);
    settings().add<BoolSetting>("Prioridad al juego", "Más CPU para Minecraft", true);
  }
  void onDisable() override {
    for (const auto& [tag, value] : originals_) game::Options::instance().set(tag, value);
    originals_.clear();
    priority(false);
  }
  void paint(ImDrawList&, ImVec2, float) override {
    priority(setting<BoolSetting>(*this, "Prioridad al juego").value());
    clock_ += ImGui::GetIO().DeltaTime;
    auto& options = game::Options::instance();
    if (clock_ < 0.25f || !options.ready()) return;
    clock_ = 0;
    for (const Tweak& t : kTweaks) apply(t.tag, t.value, setting<BoolSetting>(*this, t.name).value());
    // La distancia solo se baja, nunca se sube.
    const float limit = static_cast<float>(setting<IntSetting>(*this, "Distancia máxima").value());
    float now = 0;
    if (options.get("gfx_viewdistance", now)) {
      const auto it = originals_.find("gfx_viewdistance");
      const float original = it != originals_.end() ? it->second : now;
      apply("gfx_viewdistance", std::min(limit, original), setting<BoolSetting>(*this, "Limitar distancia").value() && original > limit);
    }
  }
  nlohmann::json saveExtra() const override { return nlohmann::json{{"originals", originals_}}; }
  void loadExtra(const nlohmann::json& v) override {
    if (!v.is_object() || !v.contains("originals") || !v["originals"].is_object() || !originals_.empty()) return;
    for (const auto& [tag, value] : v["originals"].items()) {
      if (value.is_number()) originals_[tag] = value.get<float>();
    }
  }
  static bool available(const std::string& name) {
    for (const Tweak& t : kTweaks) {
      if (name == t.name) return !game::Options::instance().ready() || game::Options::instance().has(t.tag);
    }
    return true;
  }

 private:
  void apply(const std::string& tag, float value, bool on) {
    auto& options = game::Options::instance();
    const auto it = originals_.find(tag);
    if (on) {
      float now = 0;
      if (!options.get(tag, now)) return;
      if (it == originals_.end() && now != value) originals_[tag] = now;
      options.set(tag, value);
    } else if (it != originals_.end()) {
      options.set(tag, it->second);
      originals_.erase(it);
    }
  }
  void priority(bool on) {
    if (on == boosted_) return;
    boosted_ = on;
    SetPriorityClass(GetCurrentProcess(), on ? ABOVE_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
    if (on) timeBeginPeriod(1);
    else timeEndPeriod(1);
    // Evita que Windows reduzca la velocidad del proceso (EcoQoS).
    struct {
      ULONG version, controlMask, stateMask;
    } throttling{1, on ? 1u : 0u, 0};
    using SetInfo = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);
    const auto set = reinterpret_cast<SetInfo>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32"), "SetProcessInformation")));
    if (set != nullptr) set(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &throttling, sizeof(throttling));
  }
  std::map<std::string, float> originals_;
  float clock_ = 0;
  bool boosted_ = false;
};

template <typename T>
void add() {
  core::module::ModuleManager::instance().registerModule(std::make_unique<T>());
}

}  // namespace

std::string moduleStatus(const core::module::Module& module) {
  if (module.name() != "Vida de jugadores") return {};
  const auto peer = game::peerHealth();
  if (!peer.seen) return "Acércate a otro jugador para comprobar si este servidor envía su vida.";
  if (peer.highest <= 0) return "Este servidor no envía la vida de los demás. Con la estimación activa verás un valor aproximado (~).";
  if (peer.lowest == peer.highest) return "El servidor envía siempre " + std::to_string(peer.highest) + ". Si no baja al golpear, no la actualiza y se usa la estimación (~).";
  return "Este servidor envía la vida de los demás: lo que ves es real y en vivo.";
}

bool settingAvailable(const core::module::Module& module, const std::string& name) {
  return module.name() != "Optimizador" || Optimizer::available(name);
}

void registerAllModules() {
  static bool done = false;
  if (done) return;
  done = true;
  add<Fps>();
  add<Cps>();
  add<Keystrokes>();
  add<Armor>();
  add<Combo>();
  add<Reach>();
  add<Blocks>();
  add<Health>();
  add<Coordinates>();
  add<Compass>();
  add<Speed>();
  add<LowFps>();
  add<Crosshair>();
  add<PlayerHealth>();
  add<BlockOutline>();
  add<Fullbright>();
  add<FovChanger>();
  add<NoHurtCam>();
  add<NoBobbing>();
  add<LowHealth>();
  add<TabList>();
  add<CapePhysics>();
  add<ItemPhysicsModule>();
  add<HitColor>();
  add<HitMarker>();
  add<ArmorWarning>();
  add<EnchantGlint>();
  add<HideHand>();
  add<GuiOpacity>();
  add<ClearVision>();
  add<Kills>();
  add<Accuracy>();
  add<CombatTimer>();
  add<Clock>();
  add<Zoom>();
  add<AutoSprint>();
  add<Optimizer>();
}

}  // namespace lex::platform::windows::hud
