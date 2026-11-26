#include "platform/windows/gui/ClickGuiRenderer.h"

#include <windows.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>

#include "platform/windows/game/Cosmetics.h"
#include "platform/windows/gui/IconAtlas.h"
#include "platform/windows/gui/Preview.h"
#include "platform/windows/gui/TextureStore.h"
#include "platform/windows/game/Telemetry.h"
#include "platform/windows/hud/Crosshair.h"
#include "platform/windows/hud/HudModule.h"

namespace lex::platform::windows::gui {

using namespace lex::core::settings;
using core::gui::ClickGuiModel;
using core::module::Module;

namespace {

// En el personaje no se ponen las animaciones de muerte ni las particulas.
std::vector<const game::Cosmetics::Item*> stageItems(std::vector<const game::Cosmetics::Item*> items) {
  std::erase_if(items, [](const game::Cosmetics::Item* i) { return i->slot == "kill" || i->slot == "death" || i->slot == "hit"; });
  return items;
}


constexpr ImU32 kText = IM_COL32(238, 244, 252, 255);
constexpr ImU32 kMuted = IM_COL32(139, 156, 180, 255);
constexpr ImU32 kCard = IM_COL32(20, 29, 45, 255);
constexpr ImU32 kCardHot = IM_COL32(27, 39, 60, 255);
constexpr ImU32 kLine = IM_COL32(40, 56, 82, 255);
constexpr ImU32 kField = IM_COL32(12, 18, 30, 255);

ImU32 accentU32(const core::gui::GuiTheme& t, float alpha = 1.0f) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(t.accent.r, t.accent.g, t.accent.b, alpha));
}

ImU32 fade(ImU32 color) { return ImGui::GetColorU32(color); }

ImFont* bold() { return hud::fonts().bold ? hud::fonts().bold : ImGui::GetFont(); }
ImFont* regular() { return hud::fonts().regular ? hud::fonts().regular : ImGui::GetFont(); }

// Nombre de la tecla segun el teclado del usuario.
std::string keyName(const Keybind& key) {
  switch (key.key) {
    case 0: return "Sin asignar";
    case VK_LBUTTON: return "Clic izq.";
    case VK_RBUTTON: return "Clic der.";
    case VK_MBUTTON: return "Rueda";
    case VK_XBUTTON1: return "Ratón 4";
    case VK_XBUTTON2: return "Ratón 5";
    case VK_RSHIFT: return "Shift der.";
    case VK_LSHIFT: return "Shift izq.";
    default: break;
  }
  if ((key.key >= '0' && key.key <= '9') || (key.key >= 'A' && key.key <= 'Z')) return std::string(1, static_cast<char>(key.key));
  UINT scan = MapVirtualKeyW(static_cast<UINT>(key.key), MAPVK_VK_TO_VSC_EX);
  wchar_t wide[64] = {};
  const LONG param = static_cast<LONG>((scan & 0xFF) << 16 | ((scan & 0xE000) != 0 ? 1 << 24 : 0));
  if (GetKeyNameTextW(param, wide, 64) <= 0) return "Tecla " + std::to_string(key.key);
  char utf8[128] = {};
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr);
  return utf8;
}

void text(ImDrawList* d, ImFont* f, float size, ImVec2 p, ImU32 c, const std::string& t, float wrap = 0) {
  d->AddText(f, size, p, fade(c), t.c_str(), nullptr, wrap);
}

void icon(ImDrawList* d, const std::string& id, ImVec2 p, float size, ImU32 c) {
  const ImTextureID texture = IconAtlas::instance().texture(id);
  if (texture) d->AddImage(texture, p, ImVec2(p.x + size, p.y + size), ImVec2(0, 0), ImVec2(1, 1), fade(c));
}

// Icono propio del mod; si falta, el generico.
void moduleIcon(ImDrawList* d, const Module& module, ImVec2 p, float size, ImU32 c) {
  static std::map<std::string, std::string> keys;
  std::string& key = keys[module.name()];
  if (key.empty()) {
    key = "icon.";
    const std::string& name = module.name();
    for (std::size_t i = 0; i < name.size(); ++i) {
      const auto b = static_cast<unsigned char>(name[i]);
      if (b < 0x80) {
        key += std::isalnum(b) ? static_cast<char>(std::tolower(b)) : '-';
      } else if (b == 0xC3 && i + 1 < name.size()) {
        const auto n = static_cast<unsigned char>(name[++i]) | 0x20;
        key += n == 0xA1 ? 'a' : n == 0xA9 ? 'e' : n == 0xAD ? 'i' : n == 0xB3 ? 'o' : n == 0xBA || n == 0xBC ? 'u' : 'n';
      }
    }
  }
  const auto* entry = TextureStore::instance().find(key);
  if (entry != nullptr && entry->texture != 0) d->AddImage(entry->texture, p, ImVec2(p.x + size, p.y + size), ImVec2(0, 0), ImVec2(1, 1), fade(c));
  else icon(d, module.iconId(), p, size, c);
}

float anim(ImGuiID id, float target) {
  float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
  *v += (target - *v) * std::min(1.0f, ImGui::GetIO().DeltaTime * 16.0f);
  return *v;
}

ImU32 mix(ImU32 a, ImU32 b, float t) {
  return ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
}

void drawSwitch(ImDrawList* d, ImVec2 p, float s, float t, ImU32 accent) {
  const ImVec2 q(p.x + 36 * s, p.y + 20 * s);
  d->AddRectFilled(p, q, fade(mix(IM_COL32(48, 62, 86, 255), accent, t)), 10 * s);
  d->AddCircleFilled(ImVec2(p.x + (10 + 16 * t) * s, p.y + 10 * s), 7 * s, fade(IM_COL32_WHITE), 24);
}

bool switchRow(const char* id, const std::string& label, bool value, float s, ImU32 accent) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x;
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(w, 32 * s));
  ImDrawList* d = ImGui::GetWindowDrawList();
  text(d, regular(), 16 * s, ImVec2(p.x, p.y + 6 * s), kText, label);
  drawSwitch(d, ImVec2(p.x + w - 36 * s, p.y + 6 * s), s, anim(ImGui::GetItemID(), value ? 1.0f : 0.0f), accent);
  return clicked;
}

bool button(const char* label, ImVec2 size, float s, ImU32 fill, ImU32 ink) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(label, size);
  const bool hot = ImGui::IsItemHovered();
  ImDrawList* d = ImGui::GetWindowDrawList();
  d->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), fade(hot ? mix(fill, IM_COL32_WHITE, 0.12f) : fill), 8 * s);
  const char* end = ImGui::FindRenderedTextEnd(label);
  const ImVec2 t = bold()->CalcTextSizeA(16 * s, FLT_MAX, 0, label, end);
  d->AddText(bold(), 16 * s, ImVec2(p.x + (size.x - t.x) * 0.5f, p.y + (size.y - t.y) * 0.5f), fade(ink), label, end);
  return clicked;
}

void image(ImDrawList* d, const std::string& key, ImVec2 p, ImVec2 size, ImU32 tint = IM_COL32_WHITE) {
  const TextureStore::Entry* entry = TextureStore::instance().find(key);
  if (entry == nullptr) return;
  // Encaja conservando la proporcion.
  const float k = std::min(size.x / entry->width, size.y / entry->height);
  const ImVec2 q(entry->width * k, entry->height * k);
  const ImVec2 o(p.x + (size.x - q.x) * 0.5f, p.y + (size.y - q.y) * 0.5f);
  d->AddImage(entry->texture, o, ImVec2(o.x + q.x, o.y + q.y), ImVec2(0, 0), ImVec2(1, 1), fade(tint));
}

void centered(ImDrawList* d, ImFont* f, float size, ImVec2 p, float width, ImU32 c, const std::string& t) {
  float k = size;
  ImVec2 m = f->CalcTextSizeA(k, FLT_MAX, 0, t.c_str());
  if (m.x > width) {
    k *= width / m.x;
    m = f->CalcTextSizeA(k, FLT_MAX, 0, t.c_str());
  }
  d->AddText(f, k, ImVec2(p.x + (width - m.x) * 0.5f, p.y + (size - k) * 0.5f), fade(c), t.c_str());
}

void star(ImDrawList* d, ImVec2 c, float r, ImU32 color, bool filled) {
  ImVec2 p[10];
  for (int i = 0; i < 10; ++i) {
    const float a = -1.5707963f + i * 0.62831853f;
    const float k = i % 2 == 0 ? r : r * 0.45f;
    p[i] = ImVec2(c.x + std::cos(a) * k, c.y + std::sin(a) * k);
  }
  if (filled) d->AddConcavePolyFilled(p, 10, fade(color));
  else d->AddPolyline(p, 10, fade(color), ImDrawFlags_Closed, 1.6f);
}

constexpr ImU32 kOn = IM_COL32(38, 166, 91, 255);
constexpr ImU32 kOff = IM_COL32(196, 58, 70, 255);

// Barra de estado: verde activado, rojo desactivado.
bool statusBar(const char* id, ImVec2 p, ImVec2 size, float s, bool on, const char* yes, const char* no, ImU32 off = kOff) {
  ImGui::SetCursorScreenPos(p);
  const bool clicked = ImGui::InvisibleButton(id, size);
  const float t = anim(ImGui::GetItemID(), on ? 1.0f : 0.0f);
  const ImU32 fill = mix(off, kOn, t);
  ImDrawList* d = ImGui::GetWindowDrawList();
  d->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), fade(ImGui::IsItemHovered() ? mix(fill, IM_COL32_WHITE, 0.14f) : fill), 10 * s, ImDrawFlags_RoundCornersBottom);
  centered(d, bold(), 15 * s, ImVec2(p.x, p.y + (size.y - 15 * s) * 0.5f), size.x, IM_COL32_WHITE, on ? yes : no);
  return clicked;
}

}  // namespace

ClickGuiRenderer& ClickGuiRenderer::instance() {
  static ClickGuiRenderer renderer;
  return renderer;
}

void ClickGuiRenderer::captureKey(int key) {
  if (capturing_ == nullptr) return;
  captured_ = Keybind{};
  captured_.key = key == VK_ESCAPE ? 0 : key;
  hasCapture_ = true;
}

void ClickGuiRenderer::draw(ClickGuiModel& model, core::IPlatform&, float s) {
  if (!model.isOpen()) {
    hudEditing_ = false;
    capturing_ = nullptr;
    hasCapture_ = false;
  }
  drawHint(model, s);
  if (!model.isVisible()) return;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const ImU32 accent = accentU32(model.theme());

  if (hudEditing_) {
    ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##hudbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    if (button("Guardar HUD", ImVec2(150 * s, 40 * s), s, accent, IM_COL32(6, 16, 30, 255))) hudEditing_ = false;
    ImGui::End();
    ImGui::PopStyleVar();
    const char* help = "Arrastra para mover. Rueda: tamaño. Clic derecho: restablecer. X o Supr: quitar.";
    const ImVec2 t = regular()->CalcTextSizeA(16 * s, FLT_MAX, 0, help);
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const ImVec2 p((screen.x - t.x) * 0.5f, screen.y * 0.5f + 38 * s);
    fg->AddRectFilled(ImVec2(p.x - 12 * s, p.y - 7 * s), ImVec2(p.x + t.x + 12 * s, p.y + t.y + 7 * s), kField, 8 * s);
    fg->AddText(regular(), 16 * s, p, kText, help);
    return;
  }

  const float open = model.openAmount();
  if (model.theme().dimBackground) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(3, 6, 12, static_cast<int>(150 * open)));
  }

  // Panel amplio: barra lateral con secciones y contenido con aire.
  const ImVec2 size(std::min(1180 * s, screen.x - 40), std::min(720 * s, screen.y - 40));
  ImGui::SetNextWindowPos(ImVec2((screen.x - size.x) * 0.5f, (screen.y - size.y) * 0.5f + (1 - open) * 18 * s));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.01f, open));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6 * s);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
  ImGui::Begin("##lex", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PushFont(regular());
  ImDrawList* d = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetWindowPos();
  const ImVec2 e(o.x + size.x, o.y + size.y);
  const int alpha = static_cast<int>(255 * std::clamp(model.theme().opacity, 0.5f, 1.0f));
  const float side = 216 * s;
  d->AddRectFilled(ImVec2(o.x + side, o.y), e, fade(IM_COL32(13, 19, 31, alpha)), 14 * s, ImDrawFlags_RoundCornersRight);
  d->AddRectFilled(o, ImVec2(o.x + side, e.y), fade(IM_COL32(8, 12, 21, alpha)), 14 * s, ImDrawFlags_RoundCornersLeft);
  d->AddLine(ImVec2(o.x + side, o.y + 1), ImVec2(o.x + side, e.y - 1), fade(kLine));
  d->AddRect(o, e, fade(kLine), 14 * s);
  image(d, "logo", ImVec2(o.x + 20 * s, o.y + 18 * s), ImVec2(52 * s, 52 * s));
  text(d, bold(), 22 * s, ImVec2(o.x + 82 * s, o.y + 24 * s), kText, "LeX Client");
  text(d, regular(), 13 * s, ImVec2(o.x + 82 * s, o.y + 48 * s), kMuted, "Minecraft Bedrock");

  struct Nav {
    ClickGuiModel::Tab tab;
    const char* label;
    const char* icon;
    const char* title;
  };
  static const Nav navs[] = {{ClickGuiModel::Tab::All, "Todos los mods", "icon.search", "Todos los mods"},
                             {ClickGuiModel::Tab::Favorites, "Favoritos", "icon.star", "Tus favoritos"},
                             {ClickGuiModel::Tab::Hud, "HUD", "icon.category.hud", "Elementos del HUD"},
                             {ClickGuiModel::Tab::Visual, "Visual", "icon.category.visual", "Mods visuales"},
                             {ClickGuiModel::Tab::Utility, "Utilidad", "icon.category.utility", "Utilidad y rendimiento"},
                             {ClickGuiModel::Tab::Cosmetics, "Cosméticos", "icon.category.cosmetics", "Cosméticos"},
                             {ClickGuiModel::Tab::Settings, "Ajustes", "icon.category.settings", "Ajustes del cliente"}};
  float y = 96 * s;
  const Nav* current = &navs[0];
  for (const Nav& nav : navs) {
    const bool active = model.tab() == nav.tab;
    if (active) current = &nav;
    ImGui::SetCursorPos(ImVec2(14 * s, y));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 q(p.x + side - 28 * s, p.y + 42 * s);
    if (ImGui::InvisibleButton(nav.label, ImVec2(side - 28 * s, 42 * s))) {
      model.setTab(nav.tab);
      model.closeSettings();
    }
    const float t = anim(ImGui::GetItemID(), active ? 1.0f : ImGui::IsItemHovered() ? 0.4f : 0.0f);
    d->AddRectFilled(p, q, fade(accentU32(model.theme(), 0.16f * t)), 9 * s);
    icon(d, nav.icon, ImVec2(p.x + 12 * s, p.y + 11 * s), 20 * s, active ? accent : kMuted);
    text(d, bold(), 16 * s, ImVec2(p.x + 44 * s, p.y + 12 * s), active ? kText : kMuted, nav.label);
    // Separa los mods de las demas secciones.
    y += nav.tab == ClickGuiModel::Tab::Favorites || nav.tab == ClickGuiModel::Tab::Utility ? 62 * s : 48 * s;
  }
  ImGui::SetCursorPos(ImVec2(14 * s, size.y - 62 * s));
  if (button("Editar HUD", ImVec2(side - 28 * s, 46 * s), s, accent, IM_COL32(6, 16, 30, 255))) {
    hudEditing_ = true;
    model.closeSettings();
  }

  const bool prefs = model.tab() == ClickGuiModel::Tab::Settings;
  const bool cosmetics = model.tab() == ClickGuiModel::Tab::Cosmetics;
  Module* selected = prefs || cosmetics ? nullptr : model.selectedModule();
  const float left = side + 30 * s;
  const float width = size.x - left - 30 * s;
  const float top = 92 * s;
  if (selected != nullptr) {
    // Opciones a pagina completa: volver, nombre, descripcion y estado.
    ImGui::SetCursorPos(ImVec2(left, 26 * s));
    if (button("Volver", ImVec2(88 * s, 40 * s), s, kCard, kText)) model.closeSettings();
    moduleIcon(d, *selected, ImVec2(o.x + left + 108 * s, o.y + 26 * s), 38 * s, selected->isEnabled() ? IM_COL32_WHITE : kMuted);
    text(d, bold(), 22 * s, ImVec2(o.x + left + 160 * s, o.y + 24 * s), kText, selected->name());
    text(d, regular(), 15 * s, ImVec2(o.x + left + 160 * s, o.y + 50 * s), kMuted, selected->description());
    ImGui::SetCursorPos(ImVec2(size.x - 30 * s - 140 * s, 26 * s));
    const bool on = selected->isEnabled();
    if (button(on ? "Activado##estado" : "Desactivado##estado", ImVec2(140 * s, 40 * s), s, on ? kOn : kOff, IM_COL32_WHITE)) selected->setEnabled(!on);
  } else {
    text(d, bold(), 26 * s, ImVec2(o.x + left, o.y + 32 * s), kText, current->title);
    if (!prefs && !cosmetics) {
      const float w = 260 * s;
      ImGui::SetCursorPos(ImVec2(size.x - w - 30 * s, 26 * s));
      ImGui::SetNextItemWidth(w);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14 * s, 11 * s));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9 * s);
      ImGui::PushStyleColor(ImGuiCol_FrameBg, kCard);
      if (ImGui::InputTextWithHint("##buscar", "Buscar un mod", search_, sizeof(search_))) model.setSearch(search_);
      ImGui::PopStyleColor();
      ImGui::PopStyleVar(2);
    }
    if (cosmetics) {
      ImGui::SetCursorPos(ImVec2(size.x - 30 * s - 140 * s, 26 * s));
      if (button("Quitar todo", ImVec2(140 * s, 40 * s), s, kCard, kText)) game::Cosmetics::instance().clear();
    }
  }
  ImGui::SetCursorPos(ImVec2(left, top));
  ImGui::BeginChild(selected != nullptr ? "##opciones" : cosmetics ? "##cosmeticos" : "##contenido", ImVec2(width + 16 * s, size.y - top - 24 * s));
  if (selected != nullptr) drawSettings(model, *selected, s);
  else if (prefs) drawPreferences(model, s);
  else if (cosmetics) drawCosmetics(s);
  else drawModules(model, s);
  ImGui::EndChild();

  ImGui::PopFont();
  ImGui::End();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(4);
}

void ClickGuiRenderer::drawModules(ClickGuiModel& model, float s) {
  const auto modules = model.visibleModules();
  const float gap = 16 * s;
  const float avail = ImGui::GetContentRegionAvail().x - 16 * s;
  const int columns = std::max(2, static_cast<int>((avail + gap) / (206 * s)));
  const float w = std::floor((avail - gap * (columns - 1)) / columns);
  const float h = 162 * s;
  const float bar = 40 * s;
  ImDrawList* d = ImGui::GetWindowDrawList();
  if (modules.empty()) {
    text(d, regular(), 16 * s, ImGui::GetCursorScreenPos(), kMuted,
         model.tab() == ClickGuiModel::Tab::Favorites && model.search().empty() ? "Pulsa la estrella de un mod para guardarlo aquí."
                                                                                : "Ningún mod coincide con la búsqueda.");
  }
  const auto card = [&](Module& m, ImVec2 at) {
    ImGui::PushID(m.name().c_str());
    ImGui::SetCursorPos(at);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 q(p.x + w, p.y + h);
    const bool on = m.isEnabled();
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::InvisibleButton("card", ImVec2(w, h - bar))) m.setEnabled(!on);
    const bool hot = ImGui::IsItemHovered();
    if (hot) ImGui::SetTooltip("%s", m.description().c_str());
    d->AddRectFilled(p, q, fade(hot ? kCardHot : kCard), 12 * s);
    moduleIcon(d, m, ImVec2(p.x + (w - 48 * s) * 0.5f, p.y + 22 * s), 48 * s, on ? IM_COL32_WHITE : IM_COL32(132, 146, 170, 255));
    centered(d, bold(), 17 * s, ImVec2(p.x + 12 * s, p.y + 84 * s), w - 24 * s, on ? kText : IM_COL32(190, 202, 220, 255), m.name());
    const bool favorite = model.isFavorite(m.name());
    ImGui::SetCursorScreenPos(ImVec2(q.x - 40 * s, p.y + 4 * s));
    if (ImGui::InvisibleButton("star", ImVec2(36 * s, 36 * s))) model.toggleFavorite(m.name());
    star(d, ImVec2(q.x - 22 * s, p.y + 22 * s), 8 * s, favorite ? IM_COL32(255, 206, 84, 255) : ImGui::IsItemHovered() ? kText : IM_COL32(72, 88, 116, 255), favorite);

    // Pie: estado a la izquierda, opciones a la derecha.
    const float gear = 46 * s;
    ImGui::SetCursorScreenPos(ImVec2(p.x, q.y - bar));
    if (ImGui::InvisibleButton("state", ImVec2(w - gear, bar))) m.setEnabled(!on);
    const float t = anim(ImGui::GetItemID(), on ? 1.0f : 0.0f);
    const ImU32 fill = mix(kOff, kOn, t);
    d->AddRectFilled(ImVec2(p.x, q.y - bar), ImVec2(q.x - gear, q.y), fade(ImGui::IsItemHovered() ? mix(fill, IM_COL32_WHITE, 0.14f) : fill), 12 * s, ImDrawFlags_RoundCornersBottomLeft);
    centered(d, bold(), 15 * s, ImVec2(p.x, q.y - bar + 13 * s), w - gear, IM_COL32_WHITE, on ? "Activado" : "Desactivado");
    ImGui::SetCursorScreenPos(ImVec2(q.x - gear, q.y - bar));
    if (ImGui::InvisibleButton("options", ImVec2(gear, bar))) model.selectModule(m.name());
    const bool gearHot = ImGui::IsItemHovered();
    if (gearHot) ImGui::SetTooltip("Opciones");
    d->AddRectFilled(ImVec2(q.x - gear, q.y - bar), q, fade(gearHot ? kLine : IM_COL32(32, 45, 68, 255)), 12 * s, ImDrawFlags_RoundCornersBottomRight);
    icon(d, "icon.gear", ImVec2(q.x - gear + 13 * s, q.y - bar + 10 * s), 20 * s, gearHot ? kText : kMuted);
    ImGui::PopID();
  };
  const struct {
    core::module::Category category;
    const char* label;
  } groups[] = {{core::module::Category::Hud, "HUD"}, {core::module::Category::Visual, "Visual"}, {core::module::Category::Utility, "Utilidad"}};
  const bool headed = model.tab() == ClickGuiModel::Tab::All;
  float y = ImGui::GetCursorPosY();
  const float x0 = ImGui::GetCursorPosX();
  const auto block = [&](const std::vector<Module*>& list) {
    for (std::size_t i = 0; i < list.size(); ++i) card(*list[i], ImVec2(x0 + (i % columns) * (w + gap), y + (i / columns) * (h + gap)));
    y += static_cast<float>((list.size() + columns - 1) / columns) * (h + gap);
  };
  if (!headed) {
    block(modules);
  } else {
    for (const auto& group : groups) {
      std::vector<Module*> part;
      for (Module* m : modules) {
        if (m->category() == group.category) part.push_back(m);
      }
      if (part.empty()) continue;
      ImGui::SetCursorPos(ImVec2(x0, y));
      const ImVec2 p = ImGui::GetCursorScreenPos();
      text(d, bold(), 16 * s, p, kMuted, group.label);
      y += 30 * s;
      block(part);
      y += 10 * s;
    }
  }
  ImGui::SetCursorPos(ImVec2(x0, y));
  ImGui::Dummy(ImVec2(1, 1));
}

// Miniatura de una estela: sus particulas subiendo en bucle.
void drawTrail(ImDrawList* d, const game::Cosmetics::Item& item, ImVec2 p, ImVec2 size, float s) {
  Preview::instance().request(item.id);
  const auto* strip = TextureStore::instance().find("strip." + item.id);
  if (strip == nullptr || strip->texture == 0) return;
  d->PushClipRect(p, ImVec2(p.x + size.x, p.y + size.y), true);
  for (int i = 0; i < 14; ++i) {
    const float t = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.8f + i * 0.618f, 1.0f);
    const float x = p.x + size.x * (0.14f + 0.72f * std::fmod(std::fabs(std::sin(i * 12.9898f) * 437.58f), 1.0f)) + std::sin(t * 5 + i) * 6 * s;
    const float y = p.y + size.y * (0.94f - 0.80f * t);
    const float k = (22 + 10 * std::fmod(i * 0.53f, 1.0f)) * s;
    const int frame = std::min(7, static_cast<int>(t * 8));
    d->AddImage(strip->texture, ImVec2(x - k * 0.5f, y - k * 0.5f), ImVec2(x + k * 0.5f, y + k * 0.55f), ImVec2(frame / 8.0f, 0), ImVec2((frame + 1) / 8.0f, 1), fade(IM_COL32_WHITE));
  }
  d->PopClipRect();
}

void ClickGuiRenderer::drawCosmetics(float s) {
  auto& cosmetics = game::Cosmetics::instance();
  ImDrawList* d = ImGui::GetWindowDrawList();
  if (cosmetics.items().empty()) {
    text(d, regular(), 16 * s, ImGui::GetCursorScreenPos(), kMuted, "Abre Minecraft con el botón Jugar de LeX para cargar los cosméticos.");
    return;
  }
  static const struct {
    const char* slot;
    const char* label;
  } slots[] = {{"cape", "Capas"}, {"wings", "Alas"}, {"head", "Cabeza"}, {"pet", "Mascotas"}, {"aura", "Auras"}, {"trail", "Partículas"}, {"hit", "Golpes"}, {"kill", "Kills"}, {"death", "Muertes"}};
  const ImVec2 origin = ImGui::GetCursorPos();
  const float full = ImGui::GetContentRegionAvail().x - 16 * s;
  const float tall = ImGui::GetContentRegionAvail().y;

  // Izquierda: tu personaje con todo lo equipado. Se gira arrastrando.
  const float stage = 272 * s;
  const ImVec2 sp = ImGui::GetCursorScreenPos();
  d->AddRectFilled(sp, ImVec2(sp.x + stage, sp.y + tall - 4 * s), fade(kCard), 12 * s);
  ImGui::InvisibleButton("stage", ImVec2(stage, tall - 4 * s));
  if (ImGui::IsItemActive()) cosmeticYaw_ += ImGui::GetIO().MouseDelta.x * 0.6f;
  else cosmeticYaw_ += ImGui::GetIO().DeltaTime * 14.0f;
  Preview::instance().draw(*d, ImVec2(sp.x + 6 * s, sp.y + 10 * s), ImVec2(stage - 12 * s, tall - 56 * s), stageItems(cosmetics.worn()), cosmeticYaw_, false, true);
  centered(d, regular(), 14 * s, ImVec2(sp.x, sp.y + tall - 34 * s), stage, kMuted, "Arrastra para girar");

  // Derecha: categorias y tarjetas.
  const float x0 = origin.x + stage + 22 * s;
  float x = x0;
  for (int i = 0; i < static_cast<int>(std::size(slots)); ++i) {
    const float w = bold()->CalcTextSizeA(14 * s, FLT_MAX, 0, slots[i].label).x + 14 * s;
    ImGui::SetCursorPos(ImVec2(x, origin.y));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton(slots[i].label, ImVec2(w, 36 * s))) cosmeticSlot_ = i;
    const bool active = cosmeticSlot_ == i;
    d->AddRectFilled(p, ImVec2(p.x + w, p.y + 36 * s), fade(active ? kLine : ImGui::IsItemHovered() ? kCardHot : kCard), 9 * s);
    centered(d, bold(), 14 * s, ImVec2(p.x, p.y + 11 * s), w, active ? kText : kMuted, slots[i].label);
    x += w + 5 * s;
  }
  float gridTop = 52 * s;
  if (std::string_view(slots[cosmeticSlot_].slot) == "kill" || std::string_view(slots[cosmeticSlot_].slot) == "death") {
    // Bedrock no deja cambiar la coreografia en vivo: va en el pack al jugar.
    ImGui::SetCursorPos(ImVec2(x0, origin.y + 46 * s));
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted("Se aplica la próxima vez que abras Minecraft desde LeX.");
    ImGui::PopStyleColor();
    gridTop = 74 * s;
  }
  ImGui::SetCursorPos(ImVec2(x0, origin.y + gridTop));
  ImGui::BeginChild("##grid", ImVec2(origin.x + full + 16 * s - x0, tall - gridTop));
  ImDrawList* g = ImGui::GetWindowDrawList();
  const float gap = 14 * s;
  const float avail = ImGui::GetContentRegionAvail().x - 14 * s;
  const int columns = std::max(2, static_cast<int>((avail + gap) / (164 * s)));
  const float w = std::floor((avail - gap * (columns - 1)) / columns);
  const float h = 226 * s;
  const float bar = 36 * s;
  const ImVec2 start = ImGui::GetCursorPos();
  int i = 0;
  for (const auto& item : cosmetics.items()) {
    if (item.slot != slots[cosmeticSlot_].slot) continue;
    ImGui::PushID(item.id.c_str());
    ImGui::SetCursorPos(ImVec2(start.x + (i % columns) * (w + gap), start.y + (i / columns) * (h + gap)));
    ++i;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 q(p.x + w, p.y + h);
    const bool worn = cosmetics.equipped(item);
    bool clicked = ImGui::InvisibleButton("card", ImVec2(w, h - bar));
    g->AddRectFilled(p, q, fade(ImGui::IsItemHovered() ? kCardHot : kCard), 12 * s);
    if (ImGui::IsRectVisible(p, q)) {
      const ImVec2 box(p.x + 4 * s, p.y + 6 * s);
      const ImVec2 area(w - 8 * s, h - bar - 42 * s);
      if (item.trail >= 0) {
        drawTrail(g, item, box, area, s);
      } else {
        const bool back = item.slot == "cape" || item.slot == "wings";
        const float yaw = (back ? 180.0f : item.slot == "pet" ? -24.0f : 0.0f) + std::sin(static_cast<float>(ImGui::GetTime()) * 0.9f + i) * 30.0f;
        Preview::instance().draw(*g, box, area, {&item}, yaw, item.slot == "head", (item.slot == "wings" && item.id.find("mini") == std::string::npos) || item.slot == "aura" || item.slot == "pet");
      }
    }
    centered(g, bold(), 15 * s, ImVec2(p.x + 8 * s, q.y - bar - 27 * s), w - 16 * s, kText, item.name);
    clicked |= statusBar("state", ImVec2(p.x, q.y - bar), ImVec2(w, bar), s, worn, "Equipado", "Equipar", IM_COL32(44, 60, 88, 255));
    if (clicked) cosmetics.toggle(item);
    ImGui::PopID();
  }
  const int rows = (i + columns - 1) / columns;
  ImGui::SetCursorPos(ImVec2(start.x, start.y + rows * (h + gap)));
  ImGui::Dummy(ImVec2(1, 1));
  ImGui::EndChild();
}

void ClickGuiRenderer::drawSettings(ClickGuiModel& model, Module& module, float s) {
  ImDrawList* d = ImGui::GetWindowDrawList();
  const ImU32 accent = accentU32(model.theme());
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7 * s);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * s, 8 * s));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 10 * s));
  ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6 * s);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kCard);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kCardHot);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kCardHot);
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
  ImGui::PushStyleColor(ImGuiCol_Button, kCard);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kCardHot);

  // Dos columnas separadas: se lee mejor que una tira estrecha.
  const float gutter = 44 * s;
  const float width = std::floor((ImGui::GetContentRegionAvail().x - 20 * s - gutter) * 0.5f);
  const ImVec2 first = ImGui::GetCursorPos();
  const std::string status = hud::moduleStatus(module);
  ImGui::BeginChild("##izquierda", ImVec2(width, 0), ImGuiChildFlags_AutoResizeY);
  d = ImGui::GetWindowDrawList();
  if (!status.empty()) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 m = regular()->CalcTextSizeA(15 * s, FLT_MAX, width - 28 * s, status.c_str());
    d->AddRectFilled(at, ImVec2(at.x + width, at.y + m.y + 24 * s), fade(kCard), 9 * s);
    text(d, regular(), 15 * s, ImVec2(at.x + 14 * s, at.y + 12 * s), kText, status, width - 28 * s);
    ImGui::Dummy(ImVec2(1, m.y + 24 * s));
  }
  const auto caption = [&](const std::string& t) {
    text(d, regular(), 15 * s, ImGui::GetCursorScreenPos(), kMuted, t);
    ImGui::Dummy(ImVec2(1, 13 * s));
  };
  const auto keyButton = [&](const Keybind& current, const auto& apply, const void* token) {
    if (capturing_ == token && hasCapture_) {
      apply(captured_);
      capturing_ = nullptr;
      hasCapture_ = false;
    }
    const std::string label = capturing_ == token ? "Pulsa una tecla" : keyName(current);
    ImGui::PushID(token);
    if (ImGui::Button((label + "###key").c_str(), ImVec2(width, 0))) {
      capturing_ = token;
      hasCapture_ = false;
    }
    ImGui::PopID();
  };

  caption("Tecla para activar (Esc: ninguna)");
  keyButton(module.keybind(), [&](const Keybind& k) { module.setKeybind(k); }, &module);

  if (auto* crosshair = dynamic_cast<hud::Crosshair*>(&module)) drawCrosshairEditor(*crosshair, s, width);
  std::size_t total = 0;
  for (const auto& owned : module.settings().settings()) total += hud::settingAvailable(module, owned->name()) ? 1 : 0;
  const bool crossed = dynamic_cast<hud::Crosshair*>(&module) != nullptr;
  const std::size_t split = crossed ? 0 : total > 4 ? (total + 1) / 2 : total;
  std::size_t index = 0;
  const auto nextColumn = [&]() {
    ImGui::EndChild();
    ImGui::SetCursorPos(ImVec2(first.x + width + gutter, first.y));
    ImGui::BeginChild("##derecha", ImVec2(width, 0), ImGuiChildFlags_AutoResizeY);
    d = ImGui::GetWindowDrawList();
  };
  for (const auto& owned : module.settings().settings()) {
    SettingBase* base = owned.get();
    if (!hud::settingAvailable(module, base->name())) continue;
    if (index++ == split) nextColumn();
    ImGui::PushID(base);
    switch (base->type()) {
      case SettingType::Bool: {
        auto& v = static_cast<BoolSetting&>(*base);
        if (switchRow("b", v.name(), v.value(), s, accent)) v.setValue(!v.value());
        break;
      }
      case SettingType::Int: {
        auto& v = static_cast<IntSetting&>(*base);
        caption(v.name());
        int n = v.value();
        ImGui::SetNextItemWidth(width);
        if (ImGui::SliderInt("##i", &n, v.minimum(), v.maximum())) v.setValue(n);
        break;
      }
      case SettingType::Float: {
        auto& v = static_cast<FloatSetting&>(*base);
        caption(v.name());
        float n = v.value();
        ImGui::SetNextItemWidth(width);
        if (ImGui::SliderFloat("##f", &n, v.minimum(), v.maximum(), "%.2f")) v.setValue(n);
        break;
      }
      case SettingType::Color: {
        auto& v = static_cast<ColorSetting&>(*base);
        Color c = v.value();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        text(d, regular(), 16 * s, ImVec2(p.x, p.y + 4 * s), kText, v.name());
        ImGui::SetCursorScreenPos(ImVec2(p.x + ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight(), p.y));
        if (ImGui::ColorEdit4("##c", &c.r, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel)) {
          v.setValue(c);
        }
        break;
      }
      case SettingType::Enum: {
        auto& v = static_cast<EnumSetting&>(*base);
        caption(v.name());
        ImGui::SetNextItemWidth(width);
        if (!v.options().empty() && ImGui::BeginCombo("##e", v.options()[v.value()].c_str())) {
          for (std::size_t i = 0; i < v.options().size(); ++i) {
            if (ImGui::Selectable(v.options()[i].c_str(), i == v.value())) v.setValue(i);
          }
          ImGui::EndCombo();
        }
        break;
      }
      case SettingType::Keybind: {
        auto& v = static_cast<KeybindSetting&>(*base);
        caption(v.name());
        keyButton(v.value(), [&](const Keybind& k) { v.setValue(k); }, base);
        break;
      }
    }
    ImGui::PopID();
  }
  if (index <= split) nextColumn();
  ImGui::Dummy(ImVec2(1, 8 * s));
  if (button("Restablecer", ImVec2(width, 40 * s), s, kLine, kText)) {
    model.resetModule(module.name());
    if (auto* h = dynamic_cast<hud::HudModule*>(&module)) h->resetPosition();
  }
  ImGui::EndChild();
  ImGui::PopStyleColor(7);
  ImGui::PopStyleVar(4);
}

// Editor pixel a pixel: pinta o borra arrastrando.
void ClickGuiRenderer::drawCrosshairEditor(hud::Crosshair& crosshair, float s, float width) {
  ImDrawList* d = ImGui::GetWindowDrawList();
  constexpr int n = hud::Crosshair::kGrid;
  const float cell = std::floor(width * 0.66f / n);
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("grid", ImVec2(cell * n, cell * n));
  static int brush = -1;
  if (ImGui::IsItemActive()) {
    const ImVec2 m = ImGui::GetIO().MousePos;
    const int x = static_cast<int>((m.x - o.x) / cell);
    const int y = static_cast<int>((m.y - o.y) / cell);
    if (m.x >= o.x && m.y >= o.y && x < n && y < n) {
      auto& value = crosshair.grid()[y * n + x];
      if (brush < 0) brush = value != 0 ? 0 : 1;
      value = static_cast<std::uint8_t>(brush);
      crosshair.touch();
    }
  } else {
    brush = -1;
  }
  d->AddRectFilled(o, ImVec2(o.x + cell * n, o.y + cell * n), fade(IM_COL32(70, 104, 84, 255)), 6 * s);
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const ImVec2 p(o.x + x * cell, o.y + y * cell);
      if (crosshair.grid()[y * n + x] != 0) d->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), fade(crosshair.color((x + y) * 0.015f)));
      d->AddRect(p, ImVec2(p.x + cell, p.y + cell), fade(x == 7 || y == 7 ? IM_COL32(255, 255, 255, 46) : IM_COL32(0, 0, 0, 46)));
    }
  }
  ImGui::Dummy(ImVec2(1, 2 * s));
  // Plantillas: se eligen viendolas.
  text(d, bold(), 15 * s, ImGui::GetCursorScreenPos(), kText, "Plantillas");
  ImGui::Dummy(ImVec2(1, 14 * s));
  constexpr int perRow = 6;
  const float tile = std::floor((width - 6 * s * (perRow - 1)) / perRow);
  const ImVec2 start = ImGui::GetCursorScreenPos();
  for (int i = 0; i < hud::Crosshair::kPresets; ++i) {
    const ImVec2 p(start.x + (i % perRow) * (tile + 6 * s), start.y + (i / perRow) * (tile + 6 * s));
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID(1000 + i);
    if (ImGui::InvisibleButton("preset", ImVec2(tile, tile))) crosshair.preset(i);
    const bool hot = ImGui::IsItemHovered();
    ImGui::PopID();
    const auto grid = hud::Crosshair::presetGrid(i);
    const bool chosen = grid == crosshair.grid();
    d->AddRectFilled(p, ImVec2(p.x + tile, p.y + tile), fade(hot ? IM_COL32(96, 136, 112, 255) : IM_COL32(70, 104, 84, 255)), 6 * s);
    const float c = std::max(1.0f, std::floor(tile / n));
    const ImVec2 g(std::floor(p.x + (tile - c * n) * 0.5f), std::floor(p.y + (tile - c * n) * 0.5f));
    for (int k = 0; k < n * n; ++k) {
      if (grid[k] != 0) d->AddRectFilled(ImVec2(g.x + (k % n) * c, g.y + (k / n) * c), ImVec2(g.x + (k % n + 1) * c, g.y + (k / n + 1) * c), fade(IM_COL32_WHITE));
    }
    if (chosen) d->AddRect(p, ImVec2(p.x + tile, p.y + tile), fade(kOn), 6 * s, 0, 2.5f * s);
  }
  ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + ((hud::Crosshair::kPresets + perRow - 1) / perRow) * (tile + 6 * s)));
  if (button("Borrar todo", ImVec2(width, 28 * s), s * 0.82f, kLine, kText)) {
    crosshair.grid().fill(0);
    crosshair.touch();
  }
  ImGui::Dummy(ImVec2(1, 4 * s));
}

// Recuerda con que tecla se abre: en el menu de inicio y al entrar a un mundo.
void ClickGuiRenderer::drawHint(ClickGuiModel& model, float s) {
  if (model.isOpen()) {
    hintTime_ = 99.0f;
    return;
  }
  const bool world = game::inWorld();
  if (!world) hintTime_ = 0.0f;
  else hintTime_ += ImGui::GetIO().DeltaTime;
  const float alpha = !world ? 1.0f : std::clamp(9.0f - hintTime_, 0.0f, 1.0f);
  if (alpha <= 0.0f) return;
  const std::string key = keyName(model.theme().menuKey);
  const char* tail = world ? "abre LeX" : "dentro de un mundo abre LeX";
  ImDrawList* d = ImGui::GetBackgroundDrawList();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const float size = 17 * s;
  const float kw = std::max(26 * s, bold()->CalcTextSizeA(size, FLT_MAX, 0, key.c_str()).x + 16 * s);
  const float tw = regular()->CalcTextSizeA(size, FLT_MAX, 0, tail).x;
  const float w = 46 * s + kw + 10 * s + tw + 18 * s;
  const ImVec2 p((screen.x - w) * 0.5f, 18 * s);
  const auto a = [&](ImU32 c) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
  };
  d->AddRectFilled(p, ImVec2(p.x + w, p.y + 42 * s), a(IM_COL32(8, 13, 23, 225)), 11 * s);
  if (const auto* logo = TextureStore::instance().find("logo")) d->AddImage(logo->texture, ImVec2(p.x + 8 * s, p.y + 5 * s), ImVec2(p.x + 40 * s, p.y + 37 * s), ImVec2(0, 0), ImVec2(1, 1), a(IM_COL32_WHITE));
  const ImVec2 k(p.x + 46 * s, p.y + 8 * s);
  d->AddRectFilled(k, ImVec2(k.x + kw, k.y + 26 * s), a(accentU32(model.theme())), 6 * s);
  const float lw = bold()->CalcTextSizeA(size, FLT_MAX, 0, key.c_str()).x;
  d->AddText(bold(), size, ImVec2(k.x + (kw - lw) * 0.5f, k.y + 5 * s), a(IM_COL32(6, 16, 30, 255)), key.c_str());
  d->AddText(regular(), size, ImVec2(k.x + kw + 10 * s, k.y + 3 * s), a(kText), tail);
}

void ClickGuiRenderer::drawPreferences(ClickGuiModel& model, float s) {
  auto& t = model.theme();
  const ImU32 accent = accentU32(t);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kCard);
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6 * s);
  ImGui::BeginChild("##prefs", ImVec2(400 * s, 0));
  ImDrawList* d = ImGui::GetWindowDrawList();
  const auto caption = [&](const char* c) {
    ImGui::Dummy(ImVec2(1, 4 * s));
    text(d, regular(), 16 * s, ImGui::GetCursorScreenPos(), kText, c);
    ImGui::Dummy(ImVec2(1, 20 * s));
  };
  if (switchRow("dim", "Oscurecer el juego", t.dimBackground, s, accent)) t.dimBackground = !t.dimBackground;
  if (switchRow("anim", "Animaciones", t.animations, s, accent)) t.animations = !t.animations;
  caption("Opacidad del panel");
  ImGui::SetNextItemWidth(-1);
  ImGui::SliderFloat("##op", &t.opacity, 0.5f, 1.0f, "%.2f");
  caption("Color de acento");
  const Color swatches[] = {{0.38f, 0.75f, 1, 1}, {0.45f, 0.95f, 0.75f, 1}, {1, 0.72f, 0.35f, 1},
                            {1, 0.48f, 0.52f, 1}, {0.75f, 0.62f, 1, 1},    {0.92f, 0.95f, 1, 1}};
  for (const Color& c : swatches) {
    ImGui::PushID(static_cast<int>(c.toArgb()));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("sw", ImVec2(36 * s, 36 * s))) t.accent = c;
    const ImVec2 m(p.x + 18 * s, p.y + 18 * s);
    d->AddCircleFilled(m, 12 * s, fade(ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1))), 32);
    if (t.accent.toArgb() == c.toArgb()) d->AddCircle(m, 16 * s, fade(kText), 32, 2 * s);
    ImGui::PopID();
    ImGui::SameLine();
  }
  ImGui::NewLine();
  caption("Tecla del menú");
  if (capturing_ == &t && hasCapture_) {
    if (captured_.key != 0) t.menuKey = captured_;
    capturing_ = nullptr;
    hasCapture_ = false;
  }
  const std::string key = capturing_ == &t ? "Pulsa una tecla" : keyName(t.menuKey);
  if (button((key + "###menukey").c_str(), ImVec2(400 * s, 38 * s), s, kCard, kText)) {
    capturing_ = &t;
    hasCapture_ = false;
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
}

}  // namespace lex::platform::windows::gui
