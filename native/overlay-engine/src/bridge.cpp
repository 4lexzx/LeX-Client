// LEX CLIENT — Bridge al estado (JSON simple sin dependencias).
#include "bridge.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <algorithm>

namespace lex {
namespace engine {
namespace bridge {

static std::string appData() {
  const char* p = std::getenv("APPDATA");
  return p ? p : "";
}

static std::string normalize(const std::string& s) {
  std::string r = s;
  std::replace(r.begin(), r.end(), '\\', '/');
  return r;
}

static bool readFileTo(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

static std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  size_t e = s.find_last_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  return s.substr(b, e - b + 1);
}

static std::string extractBool(const std::string& json, const std::string& key, bool fallback) {
  auto it = json.find("\"" + key + "\"");
  if (it == std::string::npos) return fallback ? "true" : "false";
  auto colon = json.find(':', it);
  if (colon == std::string::npos) return fallback ? "true" : "false";
  size_t i = colon + 1;
  while (i < json.size() && std::isspace((unsigned char)json[i])) i++;
  if (json.compare(i, 4, "true") == 0) return "true";
  if (json.compare(i, 5, "false") == 0) return "false";
  return fallback ? "true" : "false";
}

static std::string extractNumber(const std::string& json, const std::string& key, double def) {
  auto it = json.find("\"" + key + "\"");
  if (it == std::string::npos) return std::to_string((int)def);
  auto colon = json.find(':', it);
  if (colon == std::string::npos) return std::to_string((int)def);
  auto open = json.find_first_of("-0123456789.", colon);
  auto close = json.find_first_not_of("0123456789.-", open == std::string::npos ? 0 : open);
  return trim(json.substr(open, close == std::string::npos ? std::string::npos : close - open));
}

static std::string extractString(const std::string& json, const std::string& key) {
  auto it = json.find("\"" + key + "\"");
  if (it == std::string::npos) return "";
  auto colon = json.find(':', it);
  if (colon == std::string::npos) return "";
  auto quote = json.find('"', colon);
  if (quote == std::string::npos) return "";
  auto close = json.find('"', quote + 1);
  if (close == std::string::npos) return "";
  std::string v = json.substr(quote + 1, close - quote - 1);
  return v;
}

static std::string join(const std::string& base, const std::string& leaf) {
  return normalize(base) + "/" + leaf;
}

std::string getOverlayStatePath() {
  if (!appData().empty()) return join(appData(), "lex-client/overlay-state.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-overlay-state.json";
}

std::string getModulesStatePath() {
  if (!appData().empty()) return join(appData(), "lex-client/modules.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-modules.json";
}

std::string getCosmeticsPath() {
  if (!appData().empty()) return join(appData(), "lex-client/cosmetics.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-cosmetics.json";
}

std::string getCatalogPath() {
  if (!appData().empty()) return join(appData(), "lex-client/catalog.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-catalog.json";
}

std::string getHudLayoutPath() {
  if (!appData().empty()) return join(appData(), "lex-client/hud-layout.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-hud-layout.json";
}

std::string getOverlayStatsPath() {
  if (!appData().empty()) return join(appData(), "lex-client/overlay-stats.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-overlay-stats.json";
}

std::string getGameBridgePath() {
  if (!appData().empty()) return join(appData(), "lex-client/game-bridge.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-game-bridge.json";
}

std::string getUiStatePath() {
  if (!appData().empty()) return join(appData(), "lex-client/ui-state.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-ui-state.json";
}

std::string getModuleStylesPath() {
  if (!appData().empty()) return join(appData(), "lex-client/module-styles.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-module-styles.json";
}

std::string getArmorPath() {
  if (!appData().empty()) return join(appData(), "lex-client/armor.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-armor.json";
}

std::string getProfilesPath() {
  if (!appData().empty()) return join(appData(), "lex-client/profiles.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-profiles.json";
}

std::string getLogoPath() {
  if (!appData().empty()) return join(appData(), "lex-client/lex-logo.bgra");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-logo.bgra";
}

UiState readUiStateFromString(const std::string& json) {
  UiState st;
  st.menu = extractBool(json, "menu", false) == "true";
  st.hudEdit = extractBool(json, "hudEdit", false) == "true";
  st.timestamp = atoll(extractNumber(json, "timestamp", 0).c_str());
  return st;
}

UiState readUiState(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return UiState();
  return readUiStateFromString(data);
}

void writeUiState(const std::string& path, const UiState& st) {
  std::ofstream f(path);
  f << "{\n  \"menu\": " << (st.menu ? "true" : "false") << ",\n"
    << "  \"hudEdit\": " << (st.hudEdit ? "true" : "false") << ",\n"
    << "  \"timestamp\": " << st.timestamp << "\n}\n";
}

GameBridge readGameBridgeFromString(const std::string& json) {
  GameBridge gb;
  gb.fps = atoi(extractNumber(json, "fps", 0).c_str());
  gb.frames = atoll(extractNumber(json, "frames", 0).c_str());
  gb.api = extractString(json, "api");
  gb.timestamp = atoll(extractNumber(json, "timestamp", 0).c_str());
  gb.valid = !json.empty() && gb.timestamp > 0;
  return gb;
}

GameBridge readGameBridge(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return GameBridge();
  return readGameBridgeFromString(data);
}

OverlayStats readOverlayStatsFromString(const std::string& json) {
  OverlayStats st;
  st.overlayFps = atof(extractNumber(json, "overlayFps", 0).c_str());
  st.gameFpsTga = atoi(extractNumber(json, "gameFpsTga", -1).c_str());
  st.cpsAvg = atoi(extractNumber(json, "cpsAvg", 0).c_str());
  st.cpsLeft = atoi(extractNumber(json, "cpsLeft", st.cpsAvg).c_str());
  st.cpsRight = atoi(extractNumber(json, "cpsRight", 0).c_str());
  st.cpsClickCount = atoi(extractNumber(json, "cpsClickCount", 0).c_str());
  st.keyCount = atoi(extractNumber(json, "keyCount", 0).c_str());
  st.menuVisible = extractBool(json, "menuVisible", false) == "true";
  st.hudEditVisible = extractBool(json, "hudEditVisible", false) == "true";
  st.detailVisible = extractBool(json, "detailVisible", false) == "true";
  st.searchActive = extractBool(json, "searchActive", false) == "true";
  st.visibleCount = atoi(extractNumber(json, "visibleCount", 0).c_str());
  st.menuPage = atoi(extractNumber(json, "menuPage", 0).c_str());
  st.menuScroll = atoi(extractNumber(json, "menuScroll", 0).c_str());
  st.cosmeticPage = atoi(extractNumber(json, "cosmeticPage", 0).c_str());
  st.moduleCount = atoi(extractNumber(json, "moduleCount", 0).c_str());
  st.cosmeticCount = atoi(extractNumber(json, "cosmeticCount", 0).c_str());
  st.activeCosmetic = extractString(json, "activeCosmetic");
  st.hotkeyCount = atoi(extractNumber(json, "hotkeyCount", 0).c_str());
  st.timestamp = atoll(extractNumber(json, "timestamp", 0).c_str());
  return st;
}

void writeOverlayStats(const std::string& path, const OverlayStats& st) {
  std::ofstream f(path);
  f << "{\n  \"overlayFps\": " << st.overlayFps << ",\n"
    << "  \"gameFpsTga\": " << st.gameFpsTga << ",\n"
    << "  \"cpsAvg\": " << st.cpsAvg << ",\n"
    << "  \"cpsLeft\": " << st.cpsLeft << ",\n"
    << "  \"cpsRight\": " << st.cpsRight << ",\n"
    << "  \"cpsClickCount\": " << st.cpsClickCount << ",\n"
    << "  \"keyCount\": " << st.keyCount << ",\n"
    << "  \"menuVisible\": " << (st.menuVisible ? "true" : "false") << ",\n"
    << "  \"hudEditVisible\": " << (st.hudEditVisible ? "true" : "false") << ",\n"
    << "  \"detailVisible\": " << (st.detailVisible ? "true" : "false") << ",\n"
    << "  \"searchActive\": " << (st.searchActive ? "true" : "false") << ",\n"
    << "  \"visibleCount\": " << st.visibleCount << ",\n"
    << "  \"menuPage\": " << st.menuPage << ",\n"
    << "  \"menuScroll\": " << st.menuScroll << ",\n"
    << "  \"cosmeticPage\": " << st.cosmeticPage << ",\n"
    << "  \"moduleCount\": " << st.moduleCount << ",\n"
    << "  \"cosmeticCount\": " << st.cosmeticCount << ",\n"
    << "  \"activeCosmetic\": \"" << st.activeCosmetic << "\",\n"
    << "  \"hotkeyCount\": " << st.hotkeyCount << ",\n"
    << "  \"timestamp\": " << st.timestamp << "\n"
    << "}\n";
}

OverlayStats readOverlayStats(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return OverlayStats();
  return readOverlayStatsFromString(data);
}

OverlayState readOverlayStateFromString(const std::string& json) {
  OverlayState st;
  st.visible = extractBool(json, "visible", true) == "true";
  st.clickThrough = extractBool(json, "clickThrough", false) == "true";
  st.hideOnGameMinimize = extractBool(json, "hideOnGameMinimize", true) == "true";
  st.fpsCap = atoi(extractNumber(json, "fpsCap", 0).c_str());
  st.positionX = atoi(extractNumber(json, "positionX", -1).c_str());
  st.positionY = atoi(extractNumber(json, "positionY", -1).c_str());
  st.opacity = (float)atof(extractNumber(json, "opacity", -1).c_str());
  st.menuOpacity = atoi(extractNumber(json, "menuOpacity", -1).c_str());
  st.hudOpacity = atoi(extractNumber(json, "hudOpacity", -1).c_str());
  std::string s2 = extractString(json, "source");
  (void)s2;
  return st;
}

OverlayState readOverlayState(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return OverlayState();
  return readOverlayStateFromString(data);
}

void writeOverlayState(const std::string& path, const OverlayState& st) {
  std::ofstream f(path);
  f << "{\n  \"visible\": " << (st.visible ? "true" : "false") << ",\n"
    << "  \"clickThrough\": " << (st.clickThrough ? "true" : "false") << ",\n"
    << "  \"hideOnGameMinimize\": " << (st.hideOnGameMinimize ? "true" : "false") << ",\n"
    << "  \"fpsCap\": " << st.fpsCap << ",\n"
    << "  \"positionX\": " << st.positionX << ",\n"
    << "  \"positionY\": " << st.positionY << ",\n"
    << "  \"opacity\": " << st.opacity << ",\n"
    << "  \"menuOpacity\": " << st.menuOpacity << ",\n"
    << "  \"hudOpacity\": " << st.hudOpacity << "\n"
    << "}\n";
}

std::vector<ModuleState> readModuleStatesFromString(const std::string& json) {
  std::vector<ModuleState> out;
  size_t i = 0;

  auto skipWs = [&]() {
    while (i < json.size() && std::isspace((unsigned char)json[i])) ++i;
  };
  auto parseString = [&](size_t& p) {
    ++p;  // comilla de apertura
    std::string s;
    while (p < json.size()) {
      const char c = json[p++];
      if (c == '\\') {
        if (p < json.size()) s += json[p++];
        continue;
      }
      if (c == '"') break;
      s += c;
    }
    return s;
  };

  skipWs();
  if (i >= json.size() || json[i] != '{') return out;
  ++i;

  while (i < json.size()) {
    skipWs();
    if (i >= json.size()) break;
    if (json[i] == '}') break;
    if (json[i] == ',') { ++i; continue; }
    if (json[i] != '"') { ++i; continue; }

    const std::string key = parseString(i);
    skipWs();
    if (i >= json.size() || json[i] != ':') continue;
    ++i;
    skipWs();
    if (key.empty()) continue;

    if (i < json.size() && json[i] == '{') {
      // Formato real del launcher: { "id": { "enabled": true, ... } }.
      // Leemos solo "enabled" dentro del objeto de este módulo.
      int depth = 1;
      bool enabled = false;
      ++i;
      while (i < json.size() && depth > 0) {
        const char c = json[i];
        if (c == '"') {
          const std::string innerKey = parseString(i);
          skipWs();
          if (innerKey == "enabled" && i < json.size() && json[i] == ':') {
            ++i;
            skipWs();
            enabled = json.compare(i, 4, "true") == 0;
          }
          continue;
        }
        if (c == '{') ++depth;
        else if (c == '}') --depth;
        ++i;
      }
      if (i < json.size()) ++i;  // cierre del objeto del módulo
      out.push_back({key, enabled});
    } else {
      // Compatibilidad con formatos planos: { "id": true }.
      const bool enabled = json.compare(i, 4, "true") == 0;
      while (i < json.size() && json[i] != ',' && json[i] != '}') ++i;
      out.push_back({key, enabled});
    }
  }
  return out;
}

std::vector<ModuleState> readModuleStates(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return {};
  return readModuleStatesFromString(data);
}

void writeModuleStates(const std::string& path, const std::vector<ModuleState>& states) {
  std::ofstream f(path);
  f << "{\n";
  for (size_t i = 0; i < states.size(); ++i) {
    f << "  \"" << states[i].id << "\": { \"enabled\": " << (states[i].enabled ? "true" : "false") << " }";
    if (i + 1 < states.size()) f << ",";
    f << "\n";
  }
  f << "}\n";
}

std::vector<Cosmetic> readCosmeticsFromString(const std::string& json) {
  std::vector<Cosmetic> out;
  size_t pos = 0;
  while ((pos = json.find('{', pos)) != std::string::npos) {
    size_t end = json.find('}', pos);
    if (end == std::string::npos) break;
    const std::string obj = json.substr(pos, end - pos + 1);
    Cosmetic c;
    c.id = extractString(obj, "id");
    c.name = extractString(obj, "name");
    c.type = extractString(obj, "type");
    c.source = extractString(obj, "source");
    c.version = extractString(obj, "version");
    c.preview = extractString(obj, "preview");
    c.atlas = extractString(obj, "atlas");
    c.animated = extractBool(obj, "animated", false) == "true" || !c.atlas.empty();
    c.frames = atoi(extractNumber(obj, "frames", 0).c_str());
    c.frameMs = atoi(extractNumber(obj, "frameMs", 0).c_str());
    c.active = extractBool(obj, "active", false) == "true";
    if (!c.id.empty()) out.push_back(c);
    pos = end + 1;
  }
  return out;
}

std::vector<Cosmetic> readCosmetics(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return {};
  return readCosmeticsFromString(data);
}

// Divide un array de objetos planos en sus objetos individuales,
// respetando comillas y anidamiento (no se rompe con arrays dentro).
static std::vector<std::string> splitObjects(const std::string& json) {
  std::vector<std::string> out;
  int depth = 0;
  size_t start = std::string::npos;
  bool inStr = false;
  for (size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (inStr) {
      if (c == '\\') { ++i; continue; }
      if (c == '"') inStr = false;
      continue;
    }
    if (c == '"') { inStr = true; continue; }
    if (c == '{') {
      if (depth == 0) start = i;
      ++depth;
    } else if (c == '}') {
      if (depth > 0) --depth;
      if (depth == 0 && start != std::string::npos) {
        out.push_back(json.substr(start, i - start + 1));
        start = std::string::npos;
      }
    }
  }
  return out;
}

std::vector<ModuleMeta> readCatalogFromString(const std::string& json) {
  std::vector<ModuleMeta> out;
  for (const std::string& obj : splitObjects(json)) {
    ModuleMeta m;
    m.id = extractString(obj, "id");
    m.name = extractString(obj, "name");
    m.category = extractString(obj, "category");
    m.dataSource = extractString(obj, "dataSource");
    m.description = extractString(obj, "description");
    if (!m.id.empty()) out.push_back(m);
  }
  return out;
}

std::vector<ModuleMeta> readCatalog(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return {};
  return readCatalogFromString(data);
}

std::vector<KeyState> readKeyStatesFromString(const std::string&) {
  // Reservado: hooks de engine overtype keystroke con registerKeystroke();
  // Scan para keyLog via fopen('rb') a #if 0
  return {};
}

std::vector<HudPos> readHudLayoutFromString(const std::string& json) {
  std::vector<HudPos> out;
  size_t pos = 0;
  while ((pos = json.find('{', pos)) != std::string::npos) {
    // Busca el cierre de ESTE objeto, respetando comillas.
    int depth = 0;
    bool inStr = false;
    size_t end = std::string::npos;
    for (size_t i = pos; i < json.size(); ++i) {
      const char c = json[i];
      if (inStr) {
        if (c == '\\') { ++i; continue; }
        if (c == '"') inStr = false;
        continue;
      }
      if (c == '"') { inStr = true; continue; }
      if (c == '{') ++depth;
      else if (c == '}') {
        --depth;
        if (depth == 0) { end = i; break; }
      }
    }
    if (end == std::string::npos) break;
    const size_t start = pos;
    const std::string obj = json.substr(start, end - start + 1);
    // Si es el contenedor ({ "items": [ ... ] }), seguimos escaneando dentro
    // en vez de saltarlo: los objetos planos son los que nos interesan.
    if (obj.find('{', 1) != std::string::npos) { pos = start + 1; continue; }
    pos = end + 1;
    HudPos p;
    p.id = extractString(obj, "id");
    if (p.id.empty()) continue;
    p.x = atoi(extractNumber(obj, "x", 12).c_str());
    p.y = atoi(extractNumber(obj, "y", 40).c_str());
    out.push_back(p);
  }
  return out;
}

std::vector<HudPos> readHudLayout(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return {};
  return readHudLayoutFromString(data);
}

void writeHudLayout(const std::string& path, const std::vector<HudPos>& positions) {
  std::ofstream f(path);
  f << "{\n  \"items\": [\n";
  for (size_t i = 0; i < positions.size(); ++i) {
    const HudPos& p = positions[i];
    f << "    { \"id\": \"" << p.id << "\", \"x\": " << p.x << ", \"y\": " << p.y << " }";
    if (i + 1 < positions.size()) f << ",";
    f << "\n";
  }
  f << "  ]\n}\n";
}

std::vector<ModuleStyle> readModuleStyles(const std::string& path,
                                          const std::vector<std::string>& ids) {
  std::string data;
  const bool haveFile = readFileTo(path, data);
  std::vector<ModuleStyle> out;
  out.reserve(ids.size());
  for (const std::string& id : ids) {
    ModuleStyle st;
    st.id = id;
    if (haveFile) {
      const size_t at = data.find("\"" + id + "\"");
      if (at != std::string::npos) {
        const size_t close = data.find('}', at);
        const std::string obj = data.substr(at, close == std::string::npos
                                                  ? std::string::npos : close - at + 1);
        st.size = std::max(50, std::min(200, atoi(extractNumber(obj, "size", 100).c_str())));
        st.opacity = std::max(0, std::min(100, atoi(extractNumber(obj, "opacity", 100).c_str())));
        st.background = atoi(extractNumber(obj, "background", 1).c_str()) != 0 ? 1 : 0;
        st.color = (unsigned)strtoul(extractNumber(obj, "color", 0xFFFFFF).c_str(), nullptr, 10);
        st.preset = std::max(0, std::min(23, atoi(extractNumber(obj, "preset", 0).c_str())));
      }
    }
    out.push_back(st);
  }
  return out;
}

void writeModuleStyles(const std::string& path, const std::vector<ModuleStyle>& styles) {
  std::ofstream f(path);
  f << "{\n";
  for (size_t i = 0; i < styles.size(); ++i) {
    const ModuleStyle& st = styles[i];
    f << "  \"" << st.id << "\": { \"size\": " << st.size
      << ", \"opacity\": " << st.opacity
      << ", \"background\": " << st.background
      << ", \"color\": " << st.color
      << ", \"preset\": " << st.preset << " }";
    if (i + 1 < styles.size()) f << ",";
    f << "\n";
  }
  f << "}\n";
}

ArmorState readArmorState(const std::string& path) {
  ArmorState a;
  std::string data;
  if (!readFileTo(path, data)) return a;
  a.helmet = atoi(extractNumber(data, "helmet", -1).c_str());
  a.chest = atoi(extractNumber(data, "chest", -1).c_str());
  a.legs = atoi(extractNumber(data, "legs", -1).c_str());
  a.boots = atoi(extractNumber(data, "boots", -1).c_str());
  a.hand = atoi(extractNumber(data, "hand", -1).c_str());
  return a;
}

std::vector<Profile> readProfiles(const std::string& path) {
  std::string data;
  if (!readFileTo(path, data)) return {};
  std::vector<Profile> out;
  for (const std::string& obj : splitObjects(data)) {
    Profile p;
    p.id = extractString(obj, "id");
    p.name = extractString(obj, "name");
    if (p.id.empty() || p.name.empty()) continue;
    const size_t at = obj.find("\"modules\"");
    if (at != std::string::npos) {
      const size_t lb = obj.find('[', at);
      const size_t rb = lb == std::string::npos ? std::string::npos : obj.find(']', lb);
      if (lb != std::string::npos && rb != std::string::npos) {
        const std::string arr = obj.substr(lb, rb - lb + 1);
        for (const std::string& mo : splitObjects(arr)) {
          const std::string id = extractString(mo, "id");
          if (id.empty()) continue;
          p.modules.push_back({id, extractBool(mo, "enabled", false) == "true"});
        }
      }
    }
    out.push_back(p);
  }
  return out;
}

void writeProfiles(const std::string& path, const std::vector<Profile>& profiles) {
  std::ofstream f(path);
  f << "{\n  \"items\": [\n";
  for (size_t i = 0; i < profiles.size(); ++i) {
    const Profile& p = profiles[i];
    f << "    { \"id\": \"" << p.id << "\", \"name\": \"" << p.name
      << "\", \"modules\": [";
    for (size_t j = 0; j < p.modules.size(); ++j) {
      f << "{ \"id\": \"" << p.modules[j].first << "\", \"enabled\": "
        << (p.modules[j].second ? "true" : "false") << " }";
      if (j + 1 < p.modules.size()) f << ",";
    }
    f << "] }";
    if (i + 1 < profiles.size()) f << ",";
    f << "\n";
  }
  f << "  ]\n}\n";
}

}  // namespace bridge
}  // namespace engine
}  // namespace lex
