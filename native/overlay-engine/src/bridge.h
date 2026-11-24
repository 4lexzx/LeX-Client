// LEX CLIENT — Bridge al estado (leído por módulos de Fase 4 y actualizado por overlay).
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace lex {
namespace engine {
namespace bridge {

// Estilo por módulo: lo que un cliente profesional guarda por overlay.
struct ModuleStyle {
  std::string id;
  int size = 100;        // 50..200 (%)
  int opacity = 100;     // 0..100 (%)
  int background = 1;    // 1 = fondo translúcido visible por defecto
  unsigned color = 0xFFFFFF;  // blanco puro, legible en el HUD
  int preset = 0;        // preset de mira (0..23)
};

struct ArmorState {
  int helmet = -1, chest = -1, legs = -1, boots = -1, hand = -1;  // -1 = sin dato
};

// Perfil de módulos, como el panel izquierdo de los clientes de referencia.
struct Profile {
  std::string id;
  std::string name;
  std::vector<std::pair<std::string, bool>> modules;
};

struct OverlayState {
  bool visible = true;
  bool clickThrough = false;
  bool hideOnGameMinimize = true;
  int fpsCap = 0;          // 0 => sin cambio
  int positionX = -1;
  int positionY = -1;
  float opacity = -1.f;
  int menuOpacity = -1;   // 0..100, -1 = sin cambio
  int hudOpacity = -1;    // 0..100, -1 = sin cambio
  bool requiresCleanup = false;
};

std::string getOverlayStatePath();  // %APPDATA%\lex-client\overlay-state.json
std::string getModulesStatePath();  // %APPDATA%\lex-client\modules.json
std::string getCosmeticsPath();     // %APPDATA%\lex-client\cosmetics.json
std::string getCatalogPath();       // %APPDATA%\lex-client\catalog.json
std::string getHudLayoutPath();     // %APPDATA%\lex-client\hud-layout.json
std::string getGameBridgePath();    // %APPDATA%\lex-client\game-bridge.json
std::string getUiStatePath();        // %APPDATA%\lex-client\ui-state.json
std::string getModuleStylesPath();   // %APPDATA%\lex-client\module-styles.json
std::string getArmorPath();          // %APPDATA%\lex-client\armor.json
std::string getProfilesPath();       // %APPDATA%\lex-client\profiles.json
std::string getLogoPath();           // %APPDATA%\lex-client\lex-logo.bgra

// Escrito por el DLL inyectado en Minecraft (hook de IDXGISwapChain::Present).
struct GameBridge {
  bool valid = false;
  int fps = 0;
  long long frames = 0;
  std::string api;
  long long timestamp = 0;  // ms desde epoch
};

GameBridge readGameBridge(const std::string& path);
GameBridge readGameBridgeFromString(const std::string& json);

struct ModuleState {
  std::string id;
  bool enabled = false;
};

// Entrada del catálogo de módulos (lo escribe el launcher en catalog.json).
// Sin ella, el motor categoriza por prefijo del id.
struct ModuleMeta {
  std::string id;
  std::string name;
  std::string category;      // hud | visual | gameplay | utilities
  std::string dataSource;
  std::string description;
};

struct Cosmetic {
  std::string id;
  std::string name;
  std::string type;
  std::string source;
  std::string version;
  std::string preview;   // PNG del atlas para la galería
  std::string atlas;     // archivo LEXTEX1 que consume el overlay
  bool animated = false;
  int frames = 0;
  int frameMs = 0;
  bool active = false;
};

struct KeyState {
  int vk = 0;                        // código VK útil del teclado
  bool active = false;
};

// Posición guardada de un módulo HUD dentro de la ventana del juego.
struct HudPos {
  std::string id;
  int x = 12;
  int y = 40;
};

// Stats del overlay, escrito por engine.cpp cada ~1s (consumido por módulos de Fase 4).
struct OverlayStats {
  double overlayFps = 0;
  int gameFpsTga = -1;
  int cpsAvg = 0;         // compatibilidad: CPS izquierdo
  int cpsLeft = 0;
  int cpsRight = 0;
  int cpsClickCount = 0;
  int keyCount = 0;
  bool menuVisible = false;
  bool hudEditVisible = false;
  bool detailVisible = false;
  bool searchActive = false;
  int visibleCount = 0;
  int menuPage = 0;
  int menuScroll = 0;
  int cosmeticPage = 0;
  int moduleCount = 0;
  int cosmeticCount = 0;
  std::string activeCosmetic;        // id del cosmético animado activo (si hay)
  int hotkeyCount = 0;
  long long timestamp = 0;           // ms desde epoch
};

// El engine lo escribe y el DLL inyectado lo lee para liberar el puntero
// del juego mientras el menú modal F8 está abierto.
struct UiState {
  bool menu = false;
  bool hudEdit = false;
  long long timestamp = 0;
};

UiState readUiState(const std::string& path);
UiState readUiStateFromString(const std::string& json);
void writeUiState(const std::string& path, const UiState& st);

OverlayState readOverlayState(const std::string& path);
OverlayState readOverlayStateFromString(const std::string& json);
void writeOverlayState(const std::string& path, const OverlayState& st);

std::vector<ModuleStyle> readModuleStyles(const std::string& path,
                                          const std::vector<std::string>& ids);
void writeModuleStyles(const std::string& path, const std::vector<ModuleStyle>& styles);
ArmorState readArmorState(const std::string& path);
std::vector<Profile> readProfiles(const std::string& path);
void writeProfiles(const std::string& path, const std::vector<Profile>& profiles);

std::string getOverlayStatsPath();   // %APPDATA%\lex-client\overlay-stats.json

OverlayStats readOverlayStats(const std::string& path);
OverlayStats readOverlayStatsFromString(const std::string& json);
void writeOverlayStats(const std::string& path, const OverlayStats& st);

std::vector<ModuleState> readModuleStates(const std::string& path);
std::vector<ModuleState> readModuleStatesFromString(const std::string& json);
void writeModuleStates(const std::string& path, const std::vector<ModuleState>& states);

std::vector<Cosmetic> readCosmetics(const std::string& path);
std::vector<Cosmetic> readCosmeticsFromString(const std::string& json);
std::vector<ModuleMeta> readCatalog(const std::string& path);
std::vector<ModuleMeta> readCatalogFromString(const std::string& json);
std::vector<KeyState> readKeyStatesFromString(const std::string& json);

std::vector<HudPos> readHudLayout(const std::string& path);
std::vector<HudPos> readHudLayoutFromString(const std::string& json);
void writeHudLayout(const std::string& path, const std::vector<HudPos>& positions);

}  // namespace bridge
}  // namespace engine
}  // namespace lex
