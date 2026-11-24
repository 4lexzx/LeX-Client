#define IMGUI_DEFINE_MATH_OPERATORS
#include "platform/windows/gui/ClickGuiRenderer.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/module/Category.h"
#include "core/util/Math.h"
#include "platform/windows/gui/SettingEditor.h"
#include "platform/windows/gui/VectorIcons.h"

namespace lex::platform::windows::gui {

namespace {

ImU32 toImU32(const core::settings::Color& color) {
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(color.r, color.g, color.b, color.a));
}

struct TabDefinition {
  core::gui::ClickGuiModel::Tab tab;
  const char* label;
  const char* icon;
};

const TabDefinition kTabs[] = {
    {core::gui::ClickGuiModel::Tab::Hud, "HUD", "icon.category.hud"},
    {core::gui::ClickGuiModel::Tab::Visual, "Visual", "icon.category.visual"},
    {core::gui::ClickGuiModel::Tab::Utility, "Utilidad", "icon.category.utility"},
    {core::gui::ClickGuiModel::Tab::Cosmetics, "Cosmeticos",
     "icon.category.cosmetics"},
    {core::gui::ClickGuiModel::Tab::Settings, "Ajustes", "icon.category.settings"},
    {core::gui::ClickGuiModel::Tab::Favorites, "Favoritos", "icon.star"},
};

void iconButton(const char* id, const char* iconId, ImU32 color,
                const ImVec2& size) {
  const ImVec2 position = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* list = ImGui::GetWindowDrawList();
  if (hovered) {
    list->AddRectFilled(position, position + size,
                        ImGui::ColorConvertFloat4ToU32(
                            ImVec4(color >> 24 & 0xFF, 0, 0, 1.0f)),
                        6.0f);
  }
  drawIcon(list, iconId, ImVec2(position.x + 5.0f, position.y + 5.0f),
           ImVec2(position.x + size.x - 5.0f, position.y + size.y - 5.0f),
           hovered ? IM_COL32_WHITE : color, 1.8f);
}

void sidebar(core::gui::ClickGuiModel& model, ImU32 accent,
             const ImVec4& panelColor, const ImVec4& textColor) {
  ImGui::BeginChild("##lex-sidebar", ImVec2(198.0f, 0.0f), true,
                    ImGuiWindowFlags_NoScrollbar);
  ImGui::SetCursorPos(ImVec2(18.0f, 20.0f));
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent), "LeX");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(textColor.x, textColor.y, textColor.z, 0.55f),
                     "CLIENT");
  ImGui::SetCursorPosY(52.0f);
  ImGui::Separator();
  ImGui::SetCursorPosY(66.0f);

  for (const TabDefinition& definition : kTabs) {
    const bool active = model.tab() == definition.tab;
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size(178.0f, 42.0f);
    ImGui::PushID(definition.label);

    ImDrawList* list = ImGui::GetWindowDrawList();
    const bool hovered = ImGui::InvisibleButton("##tab", size);
    const bool tabClicked = hovered && ImGui::IsItemClicked();
    if (active) {
      list->AddRectFilled(position, position + size, accent & 0x00FFFFFF, 8.0f);
      list->AddRect(position, position + size, accent, 8.0f, 0, 1.0f);
      list->AddRectFilled(ImVec2(position.x, position.y + 8.0f),
                          ImVec2(position.x + 3.0f, position.y + size.y - 8.0f),
                          accent, 2.0f);
    } else if (hovered) {
      list->AddRectFilled(position, position + size,
                          ImGui::ColorConvertFloat4ToU32(panelColor), 8.0f);
    }
    drawIcon(list, definition.icon,
             ImVec2(position.x + 12.0f, position.y + 11.0f),
             ImVec2(position.x + 31.0f, position.y + 31.0f),
             active ? IM_COL32_WHITE : accent, 1.8f);
    ImGui::SetCursorPos(ImVec2(position.x - ImGui::GetWindowPos().x + 42.0f,
                               position.y - ImGui::GetWindowPos().y + 12.0f));
    ImGui::TextColored(active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textColor,
                       "%s", definition.label);

    if (tabClicked) {
      model.setTab(definition.tab);
      model.closeSettings();
    }
    ImGui::PopID();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
  }

  ImGui::SetCursorPosY(ImGui::GetContentRegionAvail().y + ImGui::GetCursorPosY() -
                       26.0f);
  ImGui::TextColored(ImVec4(textColor.x, textColor.y, textColor.z, 0.45f),
                     "v0.2.0  |  RShift");
  ImGui::EndChild();
}

void moduleCard(core::gui::ClickGuiModel& model, core::module::Module* module,
                float width, ImU32 accent, const ImVec4& textColor,
                const ImVec4& mutedColor) {
  ImGui::PushID(module->name().c_str());
  const ImVec2 position = ImGui::GetCursorScreenPos();
  const ImVec2 size(width, 104.0f);
  ImGui::InvisibleButton("##card", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = hovered && ImGui::IsItemClicked();

  ImDrawList* list = ImGui::GetWindowDrawList();
  const ImU32 cardBackground = ImGui::ColorConvertFloat4ToU32(
      ImVec4(mutedColor.x, mutedColor.y, mutedColor.z,
             hovered ? mutedColor.w + 0.06f : mutedColor.w));
  list->AddRectFilled(position, position + size, cardBackground, 10.0f);
  list->AddRect(position, position + size,
                hovered ? (accent & 0x00FFFFFF) | 0x55000000 : accent & 0x11FFFFFF,
                10.0f, 0, 1.0f);

  // Icono
  const ImVec2 iconMin(position.x + 12.0f, position.y + 14.0f);
  const ImVec2 iconMax(iconMin.x + 44.0f, iconMin.y + 44.0f);
  list->AddRectFilled(iconMin, iconMax,
                      (accent & 0x00FFFFFF) | 0x33000000, 9.0f);
  drawIcon(list, module->iconId(), ImVec2(iconMin.x + 7.0f, iconMin.y + 7.0f),
           ImVec2(iconMax.x - 7.0f, iconMax.y - 7.0f), accent, 1.9f);

  ImGui::SetCursorPos(ImVec2(position.x - ImGui::GetWindowPos().x + 66.0f,
                             position.y - ImGui::GetWindowPos().y + 12.0f));
  ImGui::TextColored(textColor, "%s", module->name().c_str());

  ImGui::SetCursorPos(ImVec2(position.x - ImGui::GetWindowPos().x + 66.0f,
                             position.y - ImGui::GetWindowPos().y + 34.0f));
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 132.0f);
  ImGui::TextColored(ImVec4(textColor.x, textColor.y, textColor.z, 0.62f), "%s",
                     module->description().c_str());
  ImGui::PopTextWrapPos();

  // Interruptor
  const ImVec2 togglePos(position.x + width - 62.0f, position.y + 14.0f);
  const ImVec2 toggleSize(50.0f, 26.0f);
  ImGui::SetCursorPos(
      ImVec2(togglePos.x - ImGui::GetWindowPos().x,
             togglePos.y - ImGui::GetWindowPos().y));
  const bool toggled = ImGui::InvisibleButton("##toggle", toggleSize);
  const bool enabled = module->isEnabled();
  if (toggled) module->setEnabled(!enabled);
  const ImU32 toggleColor = enabled ? accent : (accent & 0x00FFFFFF) | 0x33FFFFFF;
  list->AddRectFilled(togglePos, togglePos + toggleSize, toggleColor, 13.0f);
  const float knobX = togglePos.x + (enabled ? toggleSize.x - 13.0f : 13.0f);
  list->AddCircleFilled(ImVec2(knobX, togglePos.y + toggleSize.y * 0.5f), 9.0f,
                        enabled ? IM_COL32_WHITE : IM_COL32(180, 185, 195, 255),
                        14);

  // Engranaje y favorito
  ImGui::SetCursorPos(ImVec2(position.x - ImGui::GetWindowPos().x + width - 34.0f,
                             position.y - ImGui::GetWindowPos().y + 62.0f));
  iconButton("##gear", "icon.gear", accent, ImVec2(26.0f, 26.0f));
  if (ImGui::IsItemClicked()) model.selectModule(module->name());

  ImGui::SetCursorPos(ImVec2(position.x - ImGui::GetWindowPos().x + 12.0f,
                             position.y - ImGui::GetWindowPos().y + 72.0f));
  iconButton("##favorite", "icon.star",
             model.isFavorite(module->name())
                 ? accent
                 : IM_COL32(130, 138, 150, 255),
             ImVec2(22.0f, 22.0f));
  if (ImGui::IsItemClicked()) model.toggleFavorite(module->name());

  if (hovered) model.setTooltip(module->description());
  if (clicked) model.selectModule(module->name());
  ImGui::PopID();
}

void settingsPanel(core::gui::ClickGuiModel& model, ImU32 accent,
                   const ImVec4& textColor, const ImVec4& mutedColor) {
  core::module::Module* module = model.selectedModule();
  if (module == nullptr) return;

  ImGui::SetCursorPos(ImVec2(20.0f, 16.0f));
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent), "%s",
                     module->name().c_str());
  ImGui::TextColored(ImVec4(textColor.x, textColor.y, textColor.z, 0.55f), "%s",
                     module->description().c_str());
  ImGui::Separator();

  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 112.0f);
  ImGui::SetCursorPosY(16.0f);
  if (ImGui::Button("Restablecer", ImVec2(96.0f, 28.0f))) {
    model.resetModule(module->name());
  }
  ImGui::SameLine();
  if (ImGui::Button("X", ImVec2(28.0f, 28.0f))) model.closeSettings();

  ImGui::SetCursorPos(ImVec2(20.0f, 96.0f));
  ImGui::BeginChild("##settings-scroll", ImVec2(0.0f, 0.0f), false);
  SettingEditor& editor = SettingEditor::instance();
  for (const std::unique_ptr<core::settings::SettingBase>& setting :
       module->settings().settings()) {
    ImGui::PushID(setting->name().c_str());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    editor.draw(*setting);
    ImGui::SameLine(ImGui::GetWindowWidth() - 34.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.65f);
    if (ImGui::Button("R", ImVec2(24.0f, 22.0f))) setting->reset();
    ImGui::PopStyleVar();
    ImGui::PopID();
  }

  if (module->settings().settings().empty()) {
    ImGui::TextColored(mutedColor, "Este modulo no tiene ajustes.");
  }
  ImGui::EndChild();
}

void themePanel(core::gui::ClickGuiModel& model, ImU32 accent,
                const ImVec4& textColor) {
  core::gui::GuiTheme& theme = model.theme();
  ImGui::SetCursorPos(ImVec2(20.0f, 16.0f));
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent),
                     "Ajustes de la interfaz");
  ImGui::Separator();
  ImGui::SetCursorPosY(64.0f);

  core::settings::Color accentCopy = theme.accent;
  float rgba[4] = {accentCopy.r, accentCopy.g, accentCopy.b, accentCopy.a};
  if (ImGui::ColorEdit4("Color de acento", rgba, ImGuiColorEditFlags_AlphaBar)) {
    theme.accent = core::settings::Color{rgba[0], rgba[1], rgba[2], rgba[3]};
  }
  ImGui::Checkbox("Modo oscuro", &theme.darkMode);
  ImGui::SliderFloat("Opacidad", &theme.opacity, 0.35f, 1.0f, "%.2f");
  ImGui::Checkbox("Desenfocado de fondo (simulado)", &theme.blur);
  ImGui::Checkbox("Animaciones", &theme.animations);
  ImGui::SliderFloat("Velocidad de animacion", &theme.animationSpeed, 2.0f,
                     30.0f, "%.1f");

  const bool armed = SettingEditor::instance().armedKeybind() != nullptr;
  const std::string keyLabel =
      "Tecla del menu: " + (armed ? std::string("Presiona una tecla...")
                                  : theme.menuKey.label());
  if (ImGui::Button(keyLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
    // Se arma un setting temporal mediante un puntero estatico local.
    static core::settings::KeybindSetting menuBinding("MenuKey", "",
                                                       core::settings::Keybind{});
    menuBinding.setValue(theme.menuKey);
    SettingEditor::instance().armKeybind(
        SettingEditor::instance().armedKeybind() ? nullptr : &menuBinding);
  }
  core::settings::KeybindSetting* armedBinding =
      SettingEditor::instance().armedKeybind();
  if (armedBinding != nullptr) {
    theme.menuKey = armedBinding->value();
  }
  (void)textColor;
}

}  // namespace

ClickGuiRenderer& ClickGuiRenderer::instance() {
  static ClickGuiRenderer renderer;
  return renderer;
}

void ClickGuiRenderer::draw(core::gui::ClickGuiModel& model,
                            core::IPlatform& platform) {
  (void)platform;
  if (!model.isVisible()) return;

  const ImGuiIO& io = ImGui::GetIO();
  const core::gui::GuiTheme& theme = model.theme();
  const float open = core::util::clamp(model.openAmount(), 0.0f, 1.0f);
  const ImU32 accent = toImU32(theme.accent);

  if (theme.blur && open > 0.01f) {
    ImGui::GetForegroundDrawList()->AddRectFilled(
        ImVec2(0.0f, 0.0f), io.DisplaySize,
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.32f * open)));
  }

  ImVec4 background = theme.darkMode ? ImVec4(0.045f, 0.052f, 0.070f, 1.0f)
                                     : ImVec4(0.92f, 0.93f, 0.96f, 1.0f);
  const ImVec4 textColor = theme.darkMode
                               ? ImVec4(0.94f, 0.95f, 0.98f, 1.0f)
                               : ImVec4(0.10f, 0.12f, 0.16f, 1.0f);
  const ImVec4 panelColor = theme.darkMode
                                ? ImVec4(0.075f, 0.085f, 0.110f, 1.0f)
                                : ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
  const ImVec4 mutedColor = theme.darkMode
                                ? ImVec4(0.085f, 0.095f, 0.125f, 0.85f)
                                : ImVec4(0.80f, 0.82f, 0.87f, 0.85f);
  background.w = theme.opacity * open;

  const ImVec2 windowSize(980.0f * (0.94f + 0.06f * open),
                          620.0f * (0.94f + 0.06f * open));
  const ImVec2 windowCenter(io.DisplaySize.x * 0.5f,
                            io.DisplaySize.y * 0.5f - (1.0f - open) * 24.0f);
  ImGui::SetNextWindowPos(windowCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, open);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, panelColor);
  ImGui::PushStyleColor(ImGuiCol_Button, accent & 0x00FFFFFF);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
  ImGui::PushStyleColor(ImGuiCol_Text, textColor);

  ImGui::Begin("LeX ClickGUI", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoScrollbar);
  sidebar(model, accent, panelColor, textColor);
  ImGui::SameLine();

  ImGui::BeginChild("##lex-content", ImVec2(0.0f, 0.0f), false,
                    ImGuiWindowFlags_NoScrollbar);
  if (model.tab() == core::gui::ClickGuiModel::Tab::Settings &&
      !model.settingsOpen()) {
    ImGui::SetCursorPos(ImVec2(20.0f, 16.0f));
    themePanel(model, accent, textColor);
  } else if (model.settingsOpen()) {
    settingsPanel(model, accent, textColor, mutedColor);
  } else {
    static char searchBuffer[128] = "";
    ImGui::SetCursorPos(ImVec2(18.0f, 16.0f));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 36.0f);
    if (ImGui::InputTextWithHint("##lex-search", "Buscar modulos...",
                                 searchBuffer, sizeof(searchBuffer))) {
      model.setSearch(searchBuffer);
    }
    ImGui::SetCursorPos(ImVec2(18.0f, 58.0f));

    const std::vector<core::module::Module*>& modules = model.visibleModules();
    const float innerWidth = ImGui::GetContentRegionAvail().x - 36.0f;
    const float cardWidth = (innerWidth - 14.0f) * 0.5f;
    int index = 0;
    for (core::module::Module* module : modules) {
      const float cardX = 18.0f + static_cast<float>(index % 2) * (cardWidth + 14.0f);
      const float cardY = 58.0f + static_cast<float>(index / 2) * 112.0f;
      ImGui::SetCursorPos(ImVec2(cardX, cardY));
      moduleCard(model, module, cardWidth, accent, textColor, mutedColor);
      ++index;
    }
    if (modules.empty()) {
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);
      ImGui::TextColored(mutedColor, "No hay modulos en esta categoria.");
    }
  }
  ImGui::EndChild();
  ImGui::End();

  if (model.tooltipTime() > 0.0f && !model.tooltip().empty()) {
    ImGui::SetTooltip("%s", model.tooltip().c_str());
  }

  ImGui::PopStyleColor(6);
  ImGui::PopStyleVar(6);
}

}  // namespace lex::platform::windows::gui
