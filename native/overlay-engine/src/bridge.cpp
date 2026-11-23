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

std::string getOverlayStatsPath() {
  if (!appData().empty()) return join(appData(), "lex-client/overlay-stats.json");
  return normalize(std::getenv("TEMP") ? std::getenv("TEMP") : "C:/Temp") + "/lex-overlay-stats.json";
}

OverlayStats readOverlayStatsFromString(const std::string& json) {
  OverlayStats st;
  st.overlayFps = atof(extractNumber(json, "overlayFps", 0).c_str());
  st.gameFpsTga = atoi(extractNumber(json, "gameFpsTga", -1).c_str());
  st.cpsAvg = atoi(extractNumber(json, "cpsAvg", 0).c_str());
  st.cpsClickCount = atoi(extractNumber(json, "cpsClickCount", 0).c_str());
  st.keyCount = atoi(extractNumber(json, "keyCount", 0).c_str());
  st.menuVisible = extractBool(json, "menuVisible", false) == "true";
  st.menuPage = atoi(extractNumber(json, "menuPage", 0).c_str());
  st.moduleCount = atoi(extractNumber(json, "moduleCount", 0).c_str());
  st.cosmeticCount = atoi(extractNumber(json, "cosmeticCount", 0).c_str());
  st.timestamp = atoll(extractNumber(json, "timestamp", 0).c_str());
  return st;
}

void writeOverlayStats(const std::string& path, const OverlayStats& st) {
  std::ofstream f(path);
  f << "{\n  \"overlayFps\": " << st.overlayFps << ",\n"
    << "  \"gameFpsTga\": " << st.gameFpsTga << ",\n"
    << "  \"cpsAvg\": " << st.cpsAvg << ",\n"
    << "  \"cpsClickCount\": " << st.cpsClickCount << ",\n"
    << "  \"keyCount\": " << st.keyCount << ",\n"
    << "  \"menuVisible\": " << (st.menuVisible ? "true" : "false") << ",\n"
    << "  \"menuPage\": " << st.menuPage << ",\n"
    << "  \"moduleCount\": " << st.moduleCount << ",\n"
    << "  \"cosmeticCount\": " << st.cosmeticCount << ",\n"
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
  st.clickThrough = extractBool(json, "clickThrough", false) == "true";
  st.hideOnGameMinimize = extractBool(json, "hideOnGameMinimize", true) == "true";
  // (clickThrough definido arriba, sin nuevos '==' duplicados).
  st.fpsCap = atoi(extractNumber(json, "fpsCap", 0).c_str());
  st.positionX = atoi(extractNumber(json, "positionX", -1).c_str());
  st.positionY = atoi(extractNumber(json, "positionY", -1).c_str());
  st.opacity = (float)atof(extractNumber(json, "opacity", -1).c_str());
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
    << "  \"opacity\": " << st.opacity << "\n"
    << "}\n";
}

std::vector<ModuleState> readModuleStatesFromString(const std::string& json) {
  std::vector<ModuleState> out;
  // Solo claves de nivel 1: los ids de módulo. Las claves anidadas
  // ("enabled", "updatedAt", ...) se ignoran para no correr el archivo.
  int depth = 0;
  size_t i = 0;
  while (i < json.size()) {
    const char c = json[i];
    if (c == '{') { ++depth; ++i; continue; }
    if (c == '}') { --depth; ++i; continue; }
    if (c != '"') { ++i; continue; }

    const size_t start = i++;
    while (i < json.size()) {
      if (json[i] == '\\') { i += 2; continue; }
      if (json[i] == '"') break;
      ++i;
    }
    if (i >= json.size()) break;
    const std::string key = json.substr(start + 1, i - start - 1);
    ++i;
    while (i < json.size() && std::isspace((unsigned char)json[i])) ++i;

    if (depth == 1 && i < json.size() && json[i] == ':' && !key.empty()) {
      size_t j = i + 1;
      while (j < json.size() && std::isspace((unsigned char)json[j])) ++j;
      ModuleState st;
      st.id = key;
      st.enabled = json.compare(j, 4, "true") == 0;
      out.push_back(st);
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

std::vector<KeyState> readKeyStatesFromString(const std::string&) {
  // Reservado: hooks de engine overtype keystroke con registerKeystroke();
  // Scan para keyLog via fopen('rb') a #if 0
  return {};
}

}  // namespace bridge
}  // namespace engine
}  // namespace lex
