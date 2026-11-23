// LEX CLIENT — Bridge al estado (leído por módulos de Fase 4 y actualizado por overlay).
#pragma once
#include <string>
#include <vector>

namespace lex {
namespace engine {
namespace bridge {

struct OverlayState {
  bool visible = true;
  bool clickThrough = false;
  bool hideOnGameMinimize = true;
  int fpsCap = 0;          // 0 => sin cambio
  int positionX = -1;
  int positionY = -1;
  float opacity = -1.f;
  bool requiresCleanup = false;
};

std::string getOverlayStatePath();  // %APPDATA%\lex-client\overlay-state.json
std::string getModulesStatePath();  // %APPDATA%\lex-client\modules.json
std::string getCosmeticsPath();     // %APPDATA%\lex-client\cosmetics.json

struct ModuleState {
  std::string id;
  bool enabled = false;
};

struct Cosmetic {
  std::string id;
  std::string name;
  std::string type;
  std::string source;
  std::string version;
};

struct KeyState {
  int vk = 0;                        // código VK útil del teclado
  bool active = false;
};

// Stats del overlay, escrito por engine.cpp cada ~1s (consumido por módulos de Fase 4).
struct OverlayStats {
  double overlayFps = 0;
  int gameFpsTga = -1;
  int cpsAvg = 0;
  int cpsClickCount = 0;
  int keyCount = 0;
  bool menuVisible = false;
  int menuPage = 0;
  int moduleCount = 0;
  int cosmeticCount = 0;
  long long timestamp = 0;           // ms desde epoch
};

OverlayState readOverlayState(const std::string& path);
OverlayState readOverlayStateFromString(const std::string& json);
void writeOverlayState(const std::string& path, const OverlayState& st);

std::string getOverlayStatsPath();   // %APPDATA%\lex-client\overlay-stats.json

OverlayStats readOverlayStats(const std::string& path);
OverlayStats readOverlayStatsFromString(const std::string& json);
void writeOverlayStats(const std::string& path, const OverlayStats& st);

std::vector<ModuleState> readModuleStates(const std::string& path);
std::vector<ModuleState> readModuleStatesFromString(const std::string& json);
void writeModuleStates(const std::string& path, const std::vector<ModuleState>& states);

std::vector<Cosmetic> readCosmetics(const std::string& path);
std::vector<Cosmetic> readCosmeticsFromString(const std::string& json);
std::vector<KeyState> readKeyStatesFromString(const std::string& json);

}  // namespace bridge
}  // namespace engine
}  // namespace lex
