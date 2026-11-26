// Panel y HUD de LeX sobre el juego (OpenGL ES 3 + ImGui), entrada tactil y
// de teclado, y lectura de los datos que el pack de LeX publica (los mismos
// tamanos de textura que lee la DLL en PC).
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_activity.h>
#include <jni.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>

#include "lex.h"

namespace lex {
namespace {

// ------------------------------------------------------------------ estado
std::string g_files;          // carpeta interna de LeX (ajustes, equipamiento)
std::mutex g_lock;            // entrada (hilo de eventos) frente a dibujo
bool g_menu = false;
bool g_chat = false;          // chat del juego abierto: la X es una letra
float g_density = 2.0f;
int g_width = 0, g_height = 0;
double g_lastFrame = 0;
float g_fps = 0;
std::map<int, bool> g_keys;   // teclas fisicas pulsadas (para Teclas)

struct Pointer {
  bool down = false;
  float x = 0, y = 0;
  bool consumed = false;      // el toque empezo sobre LeX: no llega al juego
};
Pointer g_pointer;
std::vector<std::pair<int, bool>> g_pendingKeys;
float g_wheel = 0;            // rueda del raton pendiente
bool g_touch = true;          // ultimo puntero: dedo (true) o raton
bool g_showButton = true;     // boton LeX flotante
float g_hudScale = 1.0f;
std::vector<unsigned> g_pendingChars;

struct Module {
  const char* name;
  const char* description;
  bool on;
  bool available;
  const char* icon;  // archivo de icono (el mismo que en PC)
};
std::vector<Module> g_modules = {
    {"FPS", "Fotogramas por segundo", true, true, "fps"},
    {"Reloj", "La hora del dispositivo", false, true, "reloj"},
    {"Teclas", "WASD, espacio y ratón (teclado conectado)", false, true, "teclas"},
    {"Coordenadas", "X Y Z del jugador", true, true, "coordenadas"},
    {"Vida", "Tu vida con corazón", true, true, "vida"},
    {"Armadura", "Durabilidad de tu armadura", false, true, "armadura"},
    {"Velocidad", "Bloques por segundo", false, true, "velocidad"},
    {"Rumbo", "Hacia dónde miras", false, true, "rumbo"},
    {"Zoom", "Acercar la vista", false, false, "zoom"},
    {"Brillo total", "Ver en la oscuridad", false, false, "brillo-total"},
    {"Física de objetos", "Objetos con física", false, false, "fisica-de-objetos"},
    {"Lista de jugadores", "Quién está en la partida", false, false, "lista-de-jugadores"},
};

double now() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ------------------------------------------------------------------ ajustes
void saveSettings() {
  nlohmann::json j;
  for (const auto& m : g_modules) j[m.name] = m.on;
  j["_boton"] = g_showButton;
  j["_hud"] = g_hudScale;
  std::ofstream(g_files + "/android_modules.json") << j.dump(2);
}

void loadSettings() {
  std::ifstream in(g_files + "/android_modules.json");
  if (!in) return;
  try {
    nlohmann::json j;
    in >> j;
    for (auto& m : g_modules) {
      if (j.contains(m.name) && j[m.name].is_boolean() && m.available) m.on = j[m.name].get<bool>();
    }
    if (j.contains("_boton") && j["_boton"].is_boolean()) g_showButton = j["_boton"].get<bool>();
    if (j.contains("_hud") && j["_hud"].is_number()) g_hudScale = std::clamp(j["_hud"].get<float>(), 0.7f, 1.6f);
  } catch (...) {
  }
}

std::map<std::string, std::string> loadout() {
  std::map<std::string, std::string> out;
  std::ifstream in(g_files + "/loadout.json");
  if (!in) return out;
  try {
    nlohmann::json j;
    in >> j;
    for (auto& [k, v] : j.items()) out[k] = v.is_string() ? v.get<std::string>() : "";
  } catch (...) {
  }
  return out;
}

// ------------------------------------------------------------------ datos del juego
// El pack dibuja un cubo diminuto con una textura cuyo tamano codifica un valor
// (ancho = 37 + canal, alto = 211 + valor). Basta ver que textura se enlaza.
constexpr int kBaseW = 37, kBaseH = 211, kChannels = 39, kValues = 10;
std::atomic<int> g_values[kChannels];
std::atomic<double> g_seen{0};
std::mutex g_texLock;
std::map<GLuint, std::pair<int, int>> g_textures;  // id -> tamano

void noteBound(GLuint id) {
  std::pair<int, int> size;
  {
    std::lock_guard<std::mutex> lock(g_texLock);
    auto it = g_textures.find(id);
    if (it == g_textures.end()) return;
    size = it->second;
  }
  const int w = size.first, h = size.second;
  if (w >= kBaseW && w < kBaseW + kChannels && h >= kBaseH && h < kBaseH + kValues) {
    g_values[w - kBaseW].store(h - kBaseH, std::memory_order_relaxed);
    g_seen.store(now(), std::memory_order_relaxed);
  }
}

void noteSize(GLsizei w, GLsizei h) {
  GLint id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
  if (id == 0) return;
  std::lock_guard<std::mutex> lock(g_texLock);
  g_textures[static_cast<GLuint>(id)] = {w, h};
}

int value(int c) { return g_values[c].load(std::memory_order_relaxed); }
int number(int first, int digits) {
  int out = 0;
  for (int i = 0, scale = 1; i < digits; ++i, scale *= 10) out += value(first + i) * scale;
  return out;
}
bool inWorld() { return now() - g_seen.load() < 1.5; }

// ------------------------------------------------------------------ enganches GL/EGL
using SwapFn = EGLBoolean (*)(EGLDisplay, EGLSurface);
using BindFn = void (*)(GLenum, GLuint);
using TexImageFn = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexStorageFn = void (*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
using GetEventFn = int32_t (*)(AInputQueue*, AInputEvent**);
using DlsymFn = void* (*)(void*, const char*);
using ProcFn = void (*(*)(const char*))();

SwapFn g_swap = nullptr;
BindFn g_bind = nullptr;
TexImageFn g_texImage = nullptr;
TexStorageFn g_texStorage = nullptr;
GetEventFn g_getEvent = nullptr;
DlsymFn g_dlsym = nullptr;
ProcFn g_procAddress = nullptr;

void bindHook(GLenum target, GLuint id) {
  if (target == GL_TEXTURE_2D) noteBound(id);
  if (g_bind) g_bind(target, id);
}

void texImageHook(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* data) {
  if (g_texImage) g_texImage(target, level, internal, w, h, border, format, type, data);
  if (target == GL_TEXTURE_2D && level == 0) noteSize(w, h);
}

void texStorageHook(GLenum target, GLsizei levels, GLenum internal, GLsizei w, GLsizei h) {
  if (g_texStorage) g_texStorage(target, levels, internal, w, h);
  if (target == GL_TEXTURE_2D) noteSize(w, h);
}

void drawFrame();
bool g_ready = false;

EGLBoolean swapHook(EGLDisplay display, EGLSurface surface) {
  EGLint w = 0, h = 0;
  eglQuerySurface(display, surface, EGL_WIDTH, &w);
  eglQuerySurface(display, surface, EGL_HEIGHT, &h);
  g_width = w;
  g_height = h;
  if (w > 0 && h > 0) drawFrame();
  return g_swap ? g_swap(display, surface) : EGL_FALSE;
}

// Si el motor busca las funciones a mano (dlsym / eglGetProcAddress), se le
// entregan las nuestras y nos quedamos con las originales.
void* swapIn(const char* name, void* real) {
  if (real == nullptr || name == nullptr) return real;
  if (!strcmp(name, "eglSwapBuffers")) { if (!g_swap) g_swap = reinterpret_cast<SwapFn>(real); return reinterpret_cast<void*>(&swapHook); }
  if (!strcmp(name, "glBindTexture")) { if (!g_bind) g_bind = reinterpret_cast<BindFn>(real); return reinterpret_cast<void*>(&bindHook); }
  if (!strcmp(name, "glTexImage2D")) { if (!g_texImage) g_texImage = reinterpret_cast<TexImageFn>(real); return reinterpret_cast<void*>(&texImageHook); }
  if (!strcmp(name, "glTexStorage2D")) { if (!g_texStorage) g_texStorage = reinterpret_cast<TexStorageFn>(real); return reinterpret_cast<void*>(&texStorageHook); }
  return real;
}

void* dlsymHook(void* handle, const char* name) { return swapIn(name, g_dlsym ? g_dlsym(handle, name) : nullptr); }
void (*procHook(const char* name))() { return reinterpret_cast<void (*)()>(swapIn(name, g_procAddress ? reinterpret_cast<void*>(g_procAddress(name)) : nullptr)); }

// ------------------------------------------------------------------ entrada
ImVec2 buttonCenter() { return ImVec2(44 * g_density, 110 * g_density); }
float buttonRadius() { return 24 * g_density; }

bool overButton(float x, float y) {
  const ImVec2 c = buttonCenter();
  return (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= buttonRadius() * buttonRadius() * 1.4f;
}

void loadCatalog();
std::map<std::string, std::string> loadout();
std::map<std::string, std::string> g_worn;  // ranura -> id (lo que llevas puesto)

// Al abrir: lo que llevas puesto y el catalogo, leidos de nuevo.
void openMenu() {
  g_menu = true;
  loadCatalog();
  g_worn = loadout();
}

// Entrada comun (cola nativa o ventana Java): true = es de LeX, no del juego.
bool pointerEvent(int action, float x, float y, bool mouse, float scroll) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (action == AMOTION_EVENT_ACTION_SCROLL) {
    if (!g_menu) return false;
    g_wheel += scroll;
    g_pointer.x = x;
    g_pointer.y = y;
    return true;
  }
  if (action == AMOTION_EVENT_ACTION_DOWN) {
    g_touch = !mouse;
    g_pointer.consumed = g_menu || (g_showButton && overButton(x, y));
    g_pointer.down = true;
  }
  if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
    // Toque corto sobre el boton LeX: abre el panel.
    if (!g_menu && g_pointer.consumed && g_showButton && overButton(x, y)) openMenu();
    g_pointer.down = false;
  }
  g_pointer.x = x;
  g_pointer.y = y;
  if (mouse && action == AMOTION_EVENT_ACTION_BUTTON_PRESS) g_keys[1000] = true;
  if (mouse && action == AMOTION_EVENT_ACTION_BUTTON_RELEASE) g_keys[1000] = false;
  const bool ours = g_pointer.consumed || g_menu;
  if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) g_pointer.consumed = false;
  return ours;
}

bool keyEvent(int code, bool down, int meta, int repeat) {
  std::lock_guard<std::mutex> lock(g_lock);
  g_keys[code] = down;
  if (down) {
    // Igual que en PC: T, Enter o / abren el chat; Enter o Esc lo cierran.
    if (!g_menu) {
      if (g_chat && (code == AKEYCODE_ENTER || code == AKEYCODE_ESCAPE || code == AKEYCODE_BACK)) g_chat = false;
      else if (!g_chat && (code == AKEYCODE_T || code == AKEYCODE_ENTER || code == AKEYCODE_SLASH)) g_chat = true;
    }
    // X abre y cierra el panel (fuera del chat).
    if (code == AKEYCODE_X && !g_chat && repeat == 0) {
      if (g_menu) g_menu = false;
      else openMenu();
      return true;
    }
    if (g_menu && (code == AKEYCODE_ESCAPE || code == AKEYCODE_BACK)) {
      g_menu = false;
      return true;
    }
  }
  if (code == AKEYCODE_X && !g_chat) return true;  // la X tampoco llega al juego al soltarla
  if (g_menu) {
    g_pendingKeys.emplace_back(code, down);
    if (down) {
      if (code >= AKEYCODE_A && code <= AKEYCODE_Z) g_pendingChars.push_back((meta & AMETA_SHIFT_ON ? 'A' : 'a') + (code - AKEYCODE_A));
      else if (code >= AKEYCODE_0 && code <= AKEYCODE_9) g_pendingChars.push_back('0' + (code - AKEYCODE_0));
      else if (code == AKEYCODE_SPACE) g_pendingChars.push_back(' ');
    }
    return true;
  }
  return false;
}

bool handleEvent(AInputEvent* event) {
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const bool mouse = (AInputEvent_getSource(event) & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE;
    return pointerEvent(action, AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0), mouse,
                        action == AMOTION_EVENT_ACTION_SCROLL ? AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0) : 0);
  }
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
    return keyEvent(AKeyEvent_getKeyCode(event), AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN, AKeyEvent_getMetaState(event),
                    AKeyEvent_getRepeatCount(event));
  }
  return false;
}

jboolean jniPointer(JNIEnv*, jclass, jint action, jfloat x, jfloat y, jboolean mouse, jfloat scroll) {
  return pointerEvent(action, x, y, mouse, scroll) ? JNI_TRUE : JNI_FALSE;
}

jboolean jniKey(JNIEnv*, jclass, jint code, jboolean down, jint meta, jint repeat) {
  return keyEvent(code, down, meta, repeat) ? JNI_TRUE : JNI_FALSE;
}

int32_t getEventHook(AInputQueue* queue, AInputEvent** out) {
  for (;;) {
    const int32_t r = g_getEvent ? g_getEvent(queue, out) : -1;
    if (r < 0 || out == nullptr || *out == nullptr) return r;
    if (!handleEvent(*out)) return r;
    AInputQueue_finishEvent(queue, *out, 1);  // consumido por LeX
  }
}

// ------------------------------------------------------------------ imagenes del panel
// El launcher deja en files/panel/ los iconos, el logo y una miniatura de cada
// cosmetico como RGBA crudo (ancho, alto y pixeles), mas el catalogo.
struct Image {
  GLuint tex = 0;
  int w = 0, h = 0;
  bool tried = false;
};
std::map<std::string, Image> g_images;

const Image* image(const std::string& name) {
  Image& img = g_images[name];
  if (img.tried) return img.tex ? &img : nullptr;
  img.tried = true;
  std::ifstream in(g_files + "/panel/" + name + ".rgba", std::ios::binary);
  int32_t size[2] = {0, 0};
  if (!in.read(reinterpret_cast<char*>(size), sizeof(size)) || size[0] <= 0 || size[1] <= 0 || size[0] > 2048 || size[1] > 2048) return nullptr;
  std::vector<unsigned char> pixels(static_cast<size_t>(size[0]) * size[1] * 4);
  if (!in.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()))) return nullptr;
  GLint texture = 0, unpack = 0, align = 4, row = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glGenTextures(1, &img.tex);
  glBindTexture(GL_TEXTURE_2D, img.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size[0], size[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack));
  glPixelStorei(GL_UNPACK_ALIGNMENT, align);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
  img.w = size[0];
  img.h = size[1];
  return &img;
}

struct CatalogItem {
  std::string id, slot, name;
};
std::vector<CatalogItem> g_catalog;


void loadCatalog() {
  g_catalog.clear();
  std::ifstream in(g_files + "/panel/catalog.json");
  if (!in) return;
  try {
    nlohmann::json j;
    in >> j;
    for (const auto& item : j["items"]) g_catalog.push_back({item.value("id", ""), item.value("slot", ""), item.value("name", "")});
  } catch (...) {
  }
}

void saveLoadout() {
  nlohmann::json j = nlohmann::json::object();
  for (const auto& [slot, id] : g_worn) {
    if (!id.empty()) j[slot] = id;
  }
  std::ofstream(g_files + "/loadout.json") << j.dump(2);
}

// ------------------------------------------------------------------ dibujo
constexpr ImU32 kText = IM_COL32(238, 244, 252, 255);
constexpr ImU32 kMuted = IM_COL32(139, 156, 180, 255);
constexpr ImU32 kCard = IM_COL32(20, 29, 45, 255);
constexpr ImU32 kCardHot = IM_COL32(27, 39, 60, 255);
constexpr ImU32 kLine = IM_COL32(40, 56, 82, 255);
constexpr ImU32 kAccent = IM_COL32(96, 192, 255, 255);
constexpr ImU32 kInk = IM_COL32(6, 16, 30, 255);
constexpr ImU32 kOn = IM_COL32(38, 166, 91, 255);
constexpr ImU32 kOff = IM_COL32(196, 58, 70, 255);
int g_tab = 0;
int g_slot = 0;

const char* const kSlots[][2] = {{"cape", "Capas"}, {"wings", "Alas"}, {"head", "Cabeza"}, {"pet", "Mascotas"}, {"aura", "Auras"},
                                 {"trail", "Partículas"}, {"hit", "Golpes"}, {"kill", "Kills"}, {"death", "Muertes"}};

float anim(ImGuiID id, float target) {
  float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
  *v += (target - *v) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
  return *v;
}

ImU32 mix(ImU32 a, ImU32 b, float t) {
  const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
  return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

void text(ImDrawList* d, float size, ImVec2 p, ImU32 c, const std::string& t) { d->AddText(ImGui::GetFont(), size, p, c, t.c_str()); }

// Texto centrado que se encoge si no cabe.
void centered(ImDrawList* d, float size, ImVec2 p, float width, ImU32 c, const std::string& t) {
  ImFont* f = ImGui::GetFont();
  float k = size;
  ImVec2 m = f->CalcTextSizeA(k, FLT_MAX, 0, t.c_str());
  if (m.x > width) {
    k *= width / m.x;
    m = f->CalcTextSizeA(k, FLT_MAX, 0, t.c_str());
  }
  d->AddText(f, k, ImVec2(p.x + (width - m.x) * 0.5f, p.y + (size - k) * 0.5f), c, t.c_str());
}

void picture(ImDrawList* d, const Image* img, ImVec2 p, ImVec2 size, ImU32 tint = IM_COL32_WHITE) {
  if (img == nullptr) return;
  const float k = std::min(size.x / img->w, size.y / img->h);
  const ImVec2 q(img->w * k, img->h * k);
  const ImVec2 o(p.x + (size.x - q.x) * 0.5f, p.y + (size.y - q.y) * 0.5f);
  d->AddImage(static_cast<ImTextureID>(img->tex), o, ImVec2(o.x + q.x, o.y + q.y), ImVec2(0, 0), ImVec2(1, 1), tint);
}

// Un toque cuenta como pulsacion solo si el dedo casi no se movio (si se movio,
// era para desplazar la lista).
bool tapped(float s) { return ImGui::GetIO().MouseDragMaxDistanceSqr[0] < (12 * s) * (12 * s); }

bool button(const char* label, ImVec2 size, float s, ImU32 fill, ImU32 ink) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(label, size) && tapped(s);
  ImDrawList* d = ImGui::GetWindowDrawList();
  d->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::IsItemActive() ? mix(fill, IM_COL32_WHITE, 0.15f) : fill, 9 * s);
  const char* end = strstr(label, "##");
  if (end == nullptr) end = label + strlen(label);
  const float fs = 15 * s;
  const ImVec2 t = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, label, end);
  d->AddText(ImGui::GetFont(), fs, ImVec2(p.x + (size.x - t.x) * 0.5f, p.y + (size.y - t.y) * 0.5f), ink, label, end);
  return clicked;
}

// Arrastrar con el dedo (o el raton) mueve la lista.
void dragScroll(float s) {
  ImGuiIO& io = ImGui::GetIO();
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseDragging(0, 8 * s)) {
    ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
  }
}

// Lo que muestra cada modulo ahora mismo (en el HUD y en su tarjeta).
std::string sample(const char* name) {
  char buf[64];
  if (!strcmp(name, "FPS")) return std::to_string(static_cast<int>(g_fps + 0.5f)) + " FPS";
  if (!strcmp(name, "Reloj")) {
    time_t t = time(nullptr);
    strftime(buf, sizeof(buf), "%H:%M", localtime(&t));
    return buf;
  }
  if (!strcmp(name, "Teclas")) {
    const auto key = [&](int code) { auto it = g_keys.find(code); return it != g_keys.end() && it->second; };
    std::string keys;
    for (auto [code, label] : {std::pair<int, const char*>{AKEYCODE_W, "W"}, {AKEYCODE_A, "A"}, {AKEYCODE_S, "S"}, {AKEYCODE_D, "D"}}) {
      keys += key(code) ? std::string("[") + label + "]" : std::string(" ") + label + " ";
    }
    return keys + (key(AKEYCODE_SPACE) ? " [ESP]" : "  ESP");
  }
  const bool live = inWorld();
  if (!strcmp(name, "Coordenadas")) {
    if (!live) return "XYZ  \xE2\x80\x94";
    const int x = number(19, 5) * (value(18) ? -1 : 1), y = number(24, 3) - 100, z = number(28, 5) * (value(27) ? -1 : 1);
    return "XYZ " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z);
  }
  if (!strcmp(name, "Vida")) return live ? "\xE2\x99\xA5 " + std::to_string(number(16, 2)) : "\xE2\x99\xA5 \xE2\x80\x94";
  if (!strcmp(name, "Armadura")) {
    if (!live) return "Armadura \xE2\x80\x94";
    int total = 0;
    for (int i = 0; i < 4; ++i) if (value(i)) total += number(4 + i * 3, 3);
    return "Armadura " + std::to_string(total);
  }
  if (!strcmp(name, "Velocidad")) {
    if (!live) return "\xE2\x80\x94 b/s";
    snprintf(buf, sizeof(buf), "%.1f b/s", number(34, 3) / 10.0f);
    return buf;
  }
  if (!strcmp(name, "Rumbo")) {
    static const char* dirs[5] = {"Sur", "Oeste", "Norte", "Este", "\xE2\x80\x94"};
    return std::string("Rumbo ") + (live ? dirs[std::min(4, std::max(0, value(33)))] : "\xE2\x80\x94");
  }
  return "";
}

// HUD: cajas en la esquina superior derecha, como en PC.
void hudBox(ImDrawList* d, float& y, const std::string& t, float s) {
  const float fs = 15 * s;
  const float pad = 7 * s;
  const ImVec2 m = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, t.c_str());
  const ImVec2 p(g_width - m.x - pad * 2 - 12 * s, y);
  const ImVec2 q(p.x + m.x + pad * 2, p.y + m.y + pad * 1.4f);
  d->AddRectFilled(p, q, IM_COL32(9, 14, 24, 185), 7 * s);
  d->AddRectFilled(p, ImVec2(p.x + 3 * s, q.y), kAccent, 7 * s, ImDrawFlags_RoundCornersLeft);
  d->AddText(ImGui::GetFont(), fs, ImVec2(p.x + pad, p.y + pad * 0.7f), kText, t.c_str());
  y = q.y + 5 * s;
}

void drawHud(float s) {
  ImDrawList* d = ImGui::GetForegroundDrawList();
  const float k = s * g_hudScale * 0.8f;
  float y = 10 * k;
  for (const auto& m : g_modules) {
    if (!m.on || !m.available) continue;
    const bool needsWorld = strcmp(m.name, "FPS") && strcmp(m.name, "Reloj") && strcmp(m.name, "Teclas");
    if (needsWorld && !inWorld()) continue;
    hudBox(d, y, sample(m.name), k);
  }
  // Boton LeX flotante (sin teclado no hay tecla X).
  if (!g_menu && g_showButton) {
    const ImVec2 c = buttonCenter();
    d->AddCircleFilled(c, buttonRadius(), IM_COL32(9, 14, 24, 215));
    d->AddCircle(c, buttonRadius(), kAccent, 0, 2.5f * g_density);
    if (const Image* logo = image("logo")) {
      const float r = buttonRadius() * 0.78f;
      picture(d, logo, ImVec2(c.x - r, c.y - r), ImVec2(r * 2, r * 2));
    } else {
      const ImVec2 t = ImGui::CalcTextSize("LeX");
      d->AddText(ImVec2(c.x - t.x / 2, c.y - t.y / 2), kText, "LeX");
    }
  }
}

void drawMods(float s) {
  ImDrawList* d = ImGui::GetWindowDrawList();
  const float gap = 12 * s;
  const float avail = ImGui::GetContentRegionAvail().x - 8 * s;
  const int columns = std::max(2, static_cast<int>((avail + gap) / (150 * s)));
  const float w = std::floor((avail - gap * (columns - 1)) / columns);
  const float h = 132 * s;
  const float bar = 32 * s;
  const float x0 = ImGui::GetCursorPosX();
  float y = ImGui::GetCursorPosY();
  for (int group = 0; group < 2; ++group) {
    std::vector<Module*> list;
    for (auto& m : g_modules) if (m.available == (group == 0)) list.push_back(&m);
    if (list.empty()) continue;
    ImGui::SetCursorPos(ImVec2(x0, y));
    text(d, 15 * s, ImGui::GetCursorScreenPos(), kMuted, group == 0 ? "En pantalla" : "Próximamente en Android");
    y += 28 * s;
    for (size_t i = 0; i < list.size(); ++i) {
      Module& m = *list[i];
      ImGui::PushID(m.name);
      ImGui::SetCursorPos(ImVec2(x0 + (i % columns) * (w + gap), y + (i / columns) * (h + gap)));
      const ImVec2 p = ImGui::GetCursorScreenPos();
      const ImVec2 q(p.x + w, p.y + h);
      if (ImGui::InvisibleButton("card", ImVec2(w, h)) && tapped(s) && m.available) {
        m.on = !m.on;
        saveSettings();
      }
      const float t = anim(ImGui::GetItemID(), m.on ? 1.0f : 0.0f);
      d->AddRectFilled(p, q, ImGui::IsItemActive() ? kCardHot : kCard, 12 * s);
      picture(d, image("icon-" + std::string(m.icon)), ImVec2(p.x + (w - 36 * s) * 0.5f, p.y + 12 * s), ImVec2(36 * s, 36 * s),
              m.on ? IM_COL32_WHITE : IM_COL32(150, 164, 188, m.available ? 255 : 110));
      centered(d, 15 * s, ImVec2(p.x + 8 * s, p.y + 54 * s), w - 16 * s, m.available ? kText : kMuted, m.name);
      // Vista en vivo de lo que muestra.
      const std::string live = m.available ? sample(m.name) : m.description;
      centered(d, 12 * s, ImVec2(p.x + 8 * s, p.y + 76 * s), w - 16 * s, m.available ? (m.on ? kAccent : kMuted) : IM_COL32(139, 156, 180, 140), live);
      if (m.available) {
        d->AddRectFilled(ImVec2(p.x, q.y - bar), q, mix(kOff, kOn, t), 12 * s, ImDrawFlags_RoundCornersBottom);
        centered(d, 13 * s, ImVec2(p.x, q.y - bar + (bar - 13 * s) * 0.5f), w, IM_COL32_WHITE, m.on ? "Activado" : "Desactivado");
      } else {
        d->AddRectFilled(ImVec2(p.x, q.y - bar), q, IM_COL32(32, 45, 68, 255), 12 * s, ImDrawFlags_RoundCornersBottom);
        centered(d, 13 * s, ImVec2(p.x, q.y - bar + (bar - 13 * s) * 0.5f), w, IM_COL32(255, 255, 255, 110), "Pronto");
      }
      ImGui::PopID();
    }
    y += static_cast<float>((list.size() + columns - 1) / columns) * (h + gap) + 8 * s;
  }
  ImGui::SetCursorPos(ImVec2(x0, y));
  ImGui::Dummy(ImVec2(1, 1));
}

void drawCosmetics(float s) {
  ImDrawList* d = ImGui::GetWindowDrawList();
  if (g_catalog.empty()) {
    text(d, 15 * s, ImGui::GetCursorScreenPos(), kMuted, "Abre el juego con el botón Jugar de LeX para cargar los cosméticos.");
    return;
  }
  // Pestanas de ranura, desplazables en horizontal.
  const ImVec2 origin = ImGui::GetCursorPos();
  ImGui::BeginChild("##ranuras", ImVec2(ImGui::GetContentRegionAvail().x, 40 * s), 0, ImGuiWindowFlags_NoScrollbar);
  ImDrawList* t = ImGui::GetWindowDrawList();
  float x = 0;
  for (int i = 0; i < static_cast<int>(std::size(kSlots)); ++i) {
    const float w = ImGui::GetFont()->CalcTextSizeA(14 * s, FLT_MAX, 0, kSlots[i][1]).x + 22 * s;
    ImGui::SetCursorPos(ImVec2(x, 0));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton(kSlots[i][1], ImVec2(w, 34 * s)) && tapped(s)) g_slot = i;
    const bool active = g_slot == i;
    t->AddRectFilled(p, ImVec2(p.x + w, p.y + 34 * s), active ? kLine : kCard, 9 * s);
    centered(t, 14 * s, ImVec2(p.x, p.y + 10 * s), w, active ? kText : kMuted, kSlots[i][1]);
    x += w + 6 * s;
  }
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseDragging(0, 8 * s)) ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseDelta.x);
  ImGui::EndChild();
  const std::string slot = kSlots[g_slot][0];
  ImGui::SetCursorPos(ImVec2(origin.x, origin.y + 46 * s));
  text(d, 13 * s, ImGui::GetCursorScreenPos(), kMuted, "Lo que eliges aquí se pone la próxima vez que pulses Jugar en LeX.");
  ImGui::SetCursorPos(ImVec2(origin.x, origin.y + 72 * s));

  const float gap = 12 * s;
  const float avail = ImGui::GetContentRegionAvail().x - 8 * s;
  const int columns = std::max(2, static_cast<int>((avail + gap) / (140 * s)));
  const float w = std::floor((avail - gap * (columns - 1)) / columns);
  const float h = 170 * s;
  const float bar = 32 * s;
  const ImVec2 start = ImGui::GetCursorPos();
  int i = 0;
  for (const auto& item : g_catalog) {
    if (item.slot != slot) continue;
    ImGui::PushID(item.id.c_str());
    ImGui::SetCursorPos(ImVec2(start.x + (i % columns) * (w + gap), start.y + (i / columns) * (h + gap)));
    ++i;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 q(p.x + w, p.y + h);
    const bool worn = g_worn[slot] == item.id;
    if (ImGui::InvisibleButton("card", ImVec2(w, h)) && tapped(s)) {
      g_worn[slot] = worn ? "" : item.id;
      saveLoadout();
    }
    const float k = anim(ImGui::GetItemID(), worn ? 1.0f : 0.0f);
    d->AddRectFilled(p, q, ImGui::IsItemActive() ? kCardHot : kCard, 12 * s);
    if (k > 0.01f) d->AddRect(p, q, mix(kCard, kOn, k), 12 * s, 0, 2 * s);
    if (ImGui::IsRectVisible(p, q)) {
      if (const Image* thumb = image("thumb-" + item.id)) picture(d, thumb, ImVec2(p.x + 6 * s, p.y + 6 * s), ImVec2(w - 12 * s, h - bar - 34 * s));
      else centered(d, 30 * s, ImVec2(p.x, p.y + 40 * s), w, kLine, "?");
    }
    centered(d, 14 * s, ImVec2(p.x + 8 * s, q.y - bar - 24 * s), w - 16 * s, kText, item.name);
    d->AddRectFilled(ImVec2(p.x, q.y - bar), q, mix(IM_COL32(44, 60, 88, 255), kOn, k), 12 * s, ImDrawFlags_RoundCornersBottom);
    centered(d, 13 * s, ImVec2(p.x, q.y - bar + (bar - 13 * s) * 0.5f), w, IM_COL32_WHITE, worn ? "Equipado" : "Equipar");
    ImGui::PopID();
  }
  ImGui::SetCursorPos(ImVec2(start.x, start.y + static_cast<float>((i + columns - 1) / columns) * (h + gap)));
  ImGui::Dummy(ImVec2(1, 1));
}

void row(ImDrawList* d, float s, const char* key, const std::string& value) {
  const ImVec2 p = ImGui::GetCursorScreenPos();
  text(d, 14 * s, p, kMuted, key);
  text(d, 15 * s, ImVec2(p.x + 190 * s, p.y), kText, value);
  ImGui::Dummy(ImVec2(1, 26 * s));
}

void drawSettings(float s) {
  ImDrawList* d = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x - 8 * s;
  // Interruptor del boton flotante.
  if (ImGui::InvisibleButton("boton", ImVec2(w, 40 * s)) && tapped(s)) {
    g_showButton = !g_showButton;
    saveSettings();
  }
  const float t = anim(ImGui::GetItemID(), g_showButton ? 1.0f : 0.0f);
  d->AddRectFilled(p, ImVec2(p.x + w, p.y + 40 * s), kCard, 10 * s);
  text(d, 15 * s, ImVec2(p.x + 14 * s, p.y + 11 * s), kText, "Botón LeX en pantalla");
  const ImVec2 sw(p.x + w - 58 * s, p.y + 9 * s);
  d->AddRectFilled(sw, ImVec2(sw.x + 42 * s, sw.y + 22 * s), mix(IM_COL32(48, 62, 86, 255), kAccent, t), 11 * s);
  d->AddCircleFilled(ImVec2(sw.x + (11 + 20 * t) * s, sw.y + 11 * s), 8 * s, IM_COL32_WHITE, 24);
  ImGui::Dummy(ImVec2(1, 8 * s));

  // Tamano del HUD.
  const ImVec2 r = ImGui::GetCursorScreenPos();
  d->AddRectFilled(r, ImVec2(r.x + w, r.y + 58 * s), kCard, 10 * s);
  char label[40];
  snprintf(label, sizeof(label), "Tamaño del HUD  %d%%", static_cast<int>(g_hudScale * 100 + 0.5f));
  text(d, 15 * s, ImVec2(r.x + 14 * s, r.y + 6 * s), kText, label);
  ImGui::SetCursorScreenPos(ImVec2(r.x + 14 * s, r.y + 30 * s));
  ImGui::PushItemWidth(w - 28 * s);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kLine);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kLine);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kLine);
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kAccent);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3 * s));
  ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 18 * s);
  if (ImGui::SliderFloat("##hud", &g_hudScale, 0.7f, 1.6f, "")) saveSettings();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
  ImGui::PopItemWidth();
  ImGui::SetCursorScreenPos(ImVec2(r.x, r.y + 70 * s));

  row(d, s, "Abrir el panel", "Botón LeX o tecla X con teclado");
  row(d, s, "Cerrar el panel", "X, Esc, Atrás o el botón Cerrar");
  row(d, s, "Datos del juego", inWorld() ? "Recibiendo" : "Sin datos (entra a un mundo)");
  row(d, s, "Pantalla", std::to_string(g_width) + " x " + std::to_string(g_height));
  row(d, s, "Versión", "LeX Client para Android 0.3");
}

void drawMenu(float s) {
  const ImVec2 screen(static_cast<float>(g_width), static_cast<float>(g_height));
  const ImVec2 size(std::min(1100 * s, screen.x - 24 * s), std::min(640 * s, screen.y - 20 * s));
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(3, 6, 12, 150));
  ImGui::SetNextWindowPos(ImVec2((screen.x - size.x) * 0.5f, (screen.y - size.y) * 0.5f));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5 * s);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
  ImGui::Begin("##lex", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImDrawList* d = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetWindowPos();
  const ImVec2 e(o.x + size.x, o.y + size.y);
  const float side = 176 * s;
  d->AddRectFilled(ImVec2(o.x + side, o.y), e, IM_COL32(13, 19, 31, 245), 14 * s, ImDrawFlags_RoundCornersRight);
  d->AddRectFilled(o, ImVec2(o.x + side, e.y), IM_COL32(8, 12, 21, 250), 14 * s, ImDrawFlags_RoundCornersLeft);
  d->AddLine(ImVec2(o.x + side, o.y + 1), ImVec2(o.x + side, e.y - 1), kLine);
  d->AddRect(o, e, kLine, 14 * s);
  picture(d, image("logo"), ImVec2(o.x + 14 * s, o.y + 14 * s), ImVec2(42 * s, 42 * s));
  text(d, 19 * s, ImVec2(o.x + 62 * s, o.y + 16 * s), kText, "LeX Client");
  text(d, 12 * s, ImVec2(o.x + 62 * s, o.y + 38 * s), kMuted, "Bedrock \xC2\xB7 Android");

  static const char* const tabs[][2] = {{"Mods", "fps"}, {"Cosméticos", "fisica-de-capa"}, {"Ajustes", "optimizador"}};
  float y = 78 * s;
  for (int i = 0; i < 3; ++i) {
    ImGui::SetCursorPos(ImVec2(10 * s, y));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 q(p.x + side - 20 * s, p.y + 40 * s);
    if (ImGui::InvisibleButton(tabs[i][0], ImVec2(side - 20 * s, 40 * s)) && tapped(s)) g_tab = i;
    const bool active = g_tab == i;
    const float t = anim(ImGui::GetItemID(), active ? 1.0f : 0.0f);
    d->AddRectFilled(p, q, IM_COL32(96, 192, 255, static_cast<int>(40 * t)), 9 * s);
    if (active) d->AddRectFilled(p, ImVec2(p.x + 3 * s, q.y), kAccent, 2 * s);
    picture(d, image("icon-" + std::string(tabs[i][1])), ImVec2(p.x + 12 * s, p.y + 10 * s), ImVec2(20 * s, 20 * s), active ? IM_COL32_WHITE : kMuted);
    text(d, 15 * s, ImVec2(p.x + 42 * s, p.y + 11 * s), active ? kText : kMuted, tabs[i][0]);
    y += 46 * s;
  }
  ImGui::SetCursorPos(ImVec2(10 * s, size.y - 54 * s));
  if (button("Cerrar", ImVec2(side - 20 * s, 42 * s), s, kAccent, kInk)) g_menu = false;

  const float left = side + 22 * s;
  text(d, 22 * s, ImVec2(o.x + left, o.y + 18 * s), kText, tabs[g_tab][0]);
  if (g_tab == 0) {
    int active = 0;
    for (const auto& m : g_modules) active += m.on && m.available;
    const std::string count = std::to_string(active) + " activos";
    const float cw = ImGui::GetFont()->CalcTextSizeA(14 * s, FLT_MAX, 0, count.c_str()).x;
    text(d, 14 * s, ImVec2(e.x - cw - 24 * s, o.y + 24 * s), kMuted, count);
  }
  ImGui::SetCursorPos(ImVec2(left, 60 * s));
  ImGui::BeginChild(g_tab == 0 ? "##mods" : g_tab == 1 ? "##cosmeticos" : "##ajustes", ImVec2(size.x - left - 14 * s, size.y - 72 * s));
  if (g_tab == 0) drawMods(s);
  else if (g_tab == 1) drawCosmetics(s);
  else drawSettings(s);
  dragScroll(s);
  ImGui::EndChild();
  ImGui::End();
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(3);
}

// Escala del panel: unos 420 puntos de alto de pantalla, en cualquier movil.
float scale() { return std::clamp(g_height / 420.0f, 1.0f, 4.0f); }

void drawFrame() {
  const double t = now();
  if (g_lastFrame > 0) {
    const double dt = t - g_lastFrame;
    if (dt > 0) g_fps = g_fps * 0.9f + static_cast<float>(1.0 / dt) * 0.1f;
  }
  g_lastFrame = t;
  if (!g_ready) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // Latin, raya y corazon (el HUD de vida).
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2014, 0x2014, 0x2665, 0x2665, 0};
    const float px = std::round(22 * scale());
    ImFontConfig cfg;
    cfg.SizePixels = px;
    if (access("/system/fonts/Roboto-Regular.ttf", R_OK) == 0) io.Fonts->AddFontFromFileTTF("/system/fonts/Roboto-Regular.ttf", px, nullptr, ranges);
    else io.Fonts->AddFontDefault(&cfg);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale());
    style.FrameRounding = 8 * scale();
    style.GrabRounding = 9 * scale();
    ImGui_ImplOpenGL3_Init("#version 300 es");
    g_ready = true;
    log("Panel listo (%dx%d)", g_width, g_height);
  }
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(g_width), static_cast<float>(g_height));
  io.DeltaTime = 1.0f / std::max(10.0f, g_fps > 1 ? g_fps : 60.0f);
  {
    std::lock_guard<std::mutex> lock(g_lock);
    io.AddMouseSourceEvent(g_touch ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(g_pointer.x, g_pointer.y);
    if (g_wheel != 0) io.AddMouseWheelEvent(0, g_wheel);
    g_wheel = 0;
    io.AddMouseButtonEvent(0, g_menu && g_pointer.down);
    for (unsigned c : g_pendingChars) io.AddInputCharacter(c);
    g_pendingChars.clear();
    g_pendingKeys.clear();
  }
  GLint fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, g_width, g_height);
  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();
  drawHud(scale());
  if (g_menu) drawMenu(scale());
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo));
}

}  // namespace

// La entrada de GameActivity llega por Java (LexInput): sus metodos nativos
// son estos. FindClass usa el cargador de la actividad, que es el de LeX.
void registerInput(JNIEnv* env) {
  if (env == nullptr) return;
  jclass input = env->FindClass("com/lexclient/android/LexInput");
  if (input == nullptr) {
    env->ExceptionClear();
    log("ERROR: no se encontro LexInput (entrada de LeX desactivada)");
    return;
  }
  static const JNINativeMethod methods[] = {
      {"pointer", "(IFFZF)Z", reinterpret_cast<void*>(&jniPointer)},
      {"key", "(IZII)Z", reinterpret_cast<void*>(&jniKey)},
  };
  if (env->RegisterNatives(input, methods, 2) != 0) {
    env->ExceptionClear();
    log("ERROR: no se pudo registrar la entrada de LeX");
  } else {
    log("Entrada de LeX registrada");
  }
  env->DeleteLocalRef(input);
}

void installOverlay(const char* gameLibrary, ANativeActivity* activity) {
  if (activity->internalDataPath) g_files = activity->internalDataPath;
  if (AConfiguration* config = AConfiguration_new()) {
    AConfiguration_fromAssetManager(config, activity->assetManager);
    const int dpi = AConfiguration_getDensity(config);
    if (dpi > 0) g_density = std::max(1.5f, dpi / 160.0f);
    AConfiguration_delete(config);
  }
  loadSettings();
  const char* lib = "libminecraftpe.so";
  (void)gameLibrary;
  // Llamadas directas del juego...
  if (void* p = hookImport(lib, "eglSwapBuffers", reinterpret_cast<void*>(&swapHook))) g_swap = reinterpret_cast<SwapFn>(p);
  if (void* p = hookImport(lib, "glBindTexture", reinterpret_cast<void*>(&bindHook))) g_bind = reinterpret_cast<BindFn>(p);
  if (void* p = hookImport(lib, "glTexImage2D", reinterpret_cast<void*>(&texImageHook))) g_texImage = reinterpret_cast<TexImageFn>(p);
  if (void* p = hookImport(lib, "glTexStorage2D", reinterpret_cast<void*>(&texStorageHook))) g_texStorage = reinterpret_cast<TexStorageFn>(p);
  if (void* p = hookImport(lib, "AInputQueue_getEvent", reinterpret_cast<void*>(&getEventHook))) g_getEvent = reinterpret_cast<GetEventFn>(p);
  // ...y las que busca a mano el motor grafico.
  if (void* p = hookImport(lib, "dlsym", reinterpret_cast<void*>(&dlsymHook))) g_dlsym = reinterpret_cast<DlsymFn>(p);
  if (void* p = hookImport(lib, "eglGetProcAddress", reinterpret_cast<void*>(&procHook))) g_procAddress = reinterpret_cast<ProcFn>(p);
  if (!g_getEvent) g_getEvent = &AInputQueue_getEvent;
  log("LeX instalado (densidad %.1f)", g_density);
}

}  // namespace lex
