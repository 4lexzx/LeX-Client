// Panel y HUD de LeX sobre el juego (OpenGL ES 3 + ImGui), entrada tactil y
// de teclado, y lectura de los datos que el pack de LeX publica (los mismos
// tamanos de textura que lee la DLL en PC).
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_activity.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>

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
bool g_chat = false;          // chat del juego abierto: la L es una letra
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
std::vector<unsigned> g_pendingChars;

struct Module {
  const char* name;
  const char* description;
  bool on;
  bool available;
};
std::vector<Module> g_modules = {
    {"FPS", "Fotogramas por segundo", true, true},
    {"Reloj", "La hora del dispositivo", false, true},
    {"Teclas", "WASD, espacio y ratón (teclado conectado)", false, true},
    {"Coordenadas", "X Y Z del jugador", true, true},
    {"Vida", "Tu vida con corazón", true, true},
    {"Armadura", "Durabilidad de tu armadura", false, true},
    {"Velocidad", "Bloques por segundo", false, true},
    {"Rumbo", "Hacia dónde miras", false, true},
    {"Zoom", "Acercar la vista (próximamente en Android)", false, false},
    {"Brillo total", "Ver en la oscuridad (próximamente en Android)", false, false},
    {"Física de objetos", "Próximamente en Android", false, false},
    {"Lista de jugadores", "Próximamente en Android", false, false},
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

// true = el evento es de LeX y no llega al juego.
bool handleEvent(AInputEvent* event) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const float x = AMotionEvent_getX(event, 0), y = AMotionEvent_getY(event, 0);
    const int32_t source = AInputEvent_getSource(event);
    const bool mouse = (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE;
    if (action == AMOTION_EVENT_ACTION_DOWN) {
      g_pointer.consumed = g_menu || overButton(x, y);
      g_pointer.down = true;
    }
    if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
      // Toque corto sobre el boton LeX: abre o cierra el panel.
      if (!g_menu && g_pointer.consumed && overButton(x, y)) g_menu = true;
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
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
    const int32_t code = AKeyEvent_getKeyCode(event);
    const bool down = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_DOWN;
    g_keys[code] = down;
    if (down) {
      // Igual que en PC: T, Enter o / abren el chat; Enter o Esc lo cierran.
      if (!g_menu) {
        if (g_chat && (code == AKEYCODE_ENTER || code == AKEYCODE_ESCAPE || code == AKEYCODE_BACK)) g_chat = false;
        else if (!g_chat && (code == AKEYCODE_T || code == AKEYCODE_ENTER || code == AKEYCODE_SLASH)) g_chat = true;
      }
      if (code == AKEYCODE_L && !g_chat && AKeyEvent_getRepeatCount(event) == 0 && !ImGui::GetIO().WantTextInput) {
        g_menu = !g_menu;
        return true;
      }
      if (g_menu && (code == AKEYCODE_ESCAPE || code == AKEYCODE_BACK)) {
        g_menu = false;
        return true;
      }
    }
    if (g_menu) {
      g_pendingKeys.emplace_back(code, down);
      if (down) {
        const int32_t meta = AKeyEvent_getMetaState(event);
        if (code >= AKEYCODE_A && code <= AKEYCODE_Z) g_pendingChars.push_back((meta & AMETA_SHIFT_ON ? 'A' : 'a') + (code - AKEYCODE_A));
        else if (code >= AKEYCODE_0 && code <= AKEYCODE_9) g_pendingChars.push_back('0' + (code - AKEYCODE_0));
        else if (code == AKEYCODE_SPACE) g_pendingChars.push_back(' ');
      }
      return true;
    }
  }
  return false;
}

int32_t getEventHook(AInputQueue* queue, AInputEvent** out) {
  for (;;) {
    const int32_t r = g_getEvent ? g_getEvent(queue, out) : -1;
    if (r < 0 || out == nullptr || *out == nullptr) return r;
    if (!handleEvent(*out)) return r;
    AInputQueue_finishEvent(queue, *out, 1);  // consumido por LeX
  }
}

// ------------------------------------------------------------------ dibujo
const ImU32 kBg = IM_COL32(10, 14, 24, 235);
const ImU32 kCard = IM_COL32(22, 30, 48, 255);
const ImU32 kAccent = IM_COL32(47, 155, 255, 255);
int g_tab = 0;

void hudLine(ImDrawList* d, float& y, const std::string& text) {
  const float pad = 6 * g_density;
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const ImVec2 t = font->CalcTextSizeA(size, FLT_MAX, 0, text.c_str());
  const ImVec2 p(g_width - t.x - pad * 3, y);
  d->AddRectFilled(p, ImVec2(p.x + t.x + pad * 2, p.y + t.y + pad), IM_COL32(8, 12, 20, 170), 6 * g_density);
  d->AddText(font, size, ImVec2(p.x + pad, p.y + pad / 2), IM_COL32(235, 242, 255, 255), text.c_str());
  y += t.y + pad * 1.8f;
}

bool on(const char* name) {
  for (const auto& m : g_modules) if (!strcmp(m.name, name)) return m.on;
  return false;
}

void drawHud() {
  ImDrawList* d = ImGui::GetForegroundDrawList();
  float y = 10 * g_density;
  if (on("FPS")) hudLine(d, y, std::to_string(static_cast<int>(g_fps + 0.5f)) + " FPS");
  if (on("Reloj")) {
    time_t t = time(nullptr);
    char text[16];
    strftime(text, sizeof(text), "%H:%M", localtime(&t));
    hudLine(d, y, text);
  }
  if (inWorld()) {
    if (on("Coordenadas")) {
      const int x = number(19, 5) * (value(18) ? -1 : 1), yy = number(24, 3) - 100, z = number(28, 5) * (value(27) ? -1 : 1);
      hudLine(d, y, "XYZ " + std::to_string(x) + " " + std::to_string(yy) + " " + std::to_string(z));
    }
    if (on("Vida")) hudLine(d, y, "\xE2\x99\xA5 " + std::to_string(number(16, 2)));
    if (on("Armadura")) {
      static const char* names[4] = {"Casco", "Peto", "Pantalón", "Botas"};
      for (int i = 0; i < 4; ++i) if (value(i)) hudLine(d, y, std::string(names[i]) + " " + std::to_string(number(4 + i * 3, 3)));
    }
    if (on("Velocidad")) {
      char text[32];
      snprintf(text, sizeof(text), "%.1f b/s", number(34, 3) / 10.0f);
      hudLine(d, y, text);
    }
    if (on("Rumbo")) {
      static const char* dirs[5] = {"Sur", "Oeste", "Norte", "Este", "—"};
      hudLine(d, y, std::string("Rumbo ") + dirs[std::min(4, std::max(0, value(33)))]);
    }
  }
  if (on("Teclas")) {
    const auto key = [&](int code) { auto it = g_keys.find(code); return it != g_keys.end() && it->second; };
    std::string keys;
    keys += key(AKEYCODE_W) ? "[W]" : " W ";
    keys += key(AKEYCODE_A) ? "[A]" : " A ";
    keys += key(AKEYCODE_S) ? "[S]" : " S ";
    keys += key(AKEYCODE_D) ? "[D]" : " D ";
    keys += key(AKEYCODE_SPACE) ? " [ESP]" : "  ESP ";
    hudLine(d, y, keys);
  }

  // Boton LeX flotante (sin teclado no hay L).
  if (!g_menu) {
    const ImVec2 c = buttonCenter();
    d->AddCircleFilled(c, buttonRadius(), IM_COL32(10, 16, 28, 210));
    d->AddCircle(c, buttonRadius(), kAccent, 0, 2.5f * g_density);
    const ImVec2 t = ImGui::CalcTextSize("LeX");
    d->AddText(ImVec2(c.x - t.x / 2, c.y - t.y / 2), IM_COL32(255, 255, 255, 255), "LeX");
  }
}

void drawMenu() {
  const ImVec2 size(g_width * 0.78f, g_height * 0.82f);
  ImGui::SetNextWindowPos(ImVec2(g_width * 0.5f, g_height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, kBg);
  ImGui::Begin("##lex", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  ImGui::TextColored(ImVec4(0.18f, 0.6f, 1, 1), "LeX Client");
  ImGui::SameLine(size.x - 140 * g_density);
  if (ImGui::Button("Cerrar", ImVec2(120 * g_density, 0))) g_menu = false;
  ImGui::Separator();

  const char* tabs[] = {"Mods", "Cosméticos", "Info"};
  for (int i = 0; i < 3; ++i) {
    if (i) ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, g_tab == i ? kAccent : kCard);
    if (ImGui::Button(tabs[i], ImVec2(150 * g_density, 40 * g_density))) g_tab = i;
    ImGui::PopStyleColor();
  }
  ImGui::BeginChild("##body", ImVec2(0, 0), false);
  if (g_tab == 0) {
    for (auto& m : g_modules) {
      ImGui::PushID(m.name);
      ImGui::BeginDisabled(!m.available);
      if (ImGui::Checkbox(m.name, &m.on)) saveSettings();
      ImGui::EndDisabled();
      ImGui::SameLine(260 * g_density);
      ImGui::TextDisabled("%s", m.description);
      ImGui::PopID();
    }
  } else if (g_tab == 1) {
    ImGui::TextWrapped("Lo que llevas puesto. En Android, por ahora, se elige en la pantalla de LeX antes de pulsar Jugar.");
    ImGui::Spacing();
    for (const auto& [slot, id] : loadout()) ImGui::BulletText("%s: %s", slot.c_str(), id.empty() ? "nada" : id.c_str());
  } else {
    ImGui::TextWrapped("LeX Client para Android 0.1 (primera prueba).");
    ImGui::TextWrapped("Datos del juego: %s", inWorld() ? "recibiendo" : "sin datos (entra a un mundo; si no cambia, mándame el registro)");
    ImGui::TextWrapped("Pantalla: %dx%d  ·  densidad %.1f", g_width, g_height, g_density);
    ImGui::TextWrapped("Panel: botón LeX en pantalla o tecla L con teclado.");
  }
  ImGui::EndChild();
  ImGui::End();
  ImGui::PopStyleColor();
}

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
    ImFontConfig cfg;
    cfg.SizePixels = 15 * g_density;
    if (access("/system/fonts/Roboto-Regular.ttf", R_OK) == 0) io.Fonts->AddFontFromFileTTF("/system/fonts/Roboto-Regular.ttf", 15 * g_density);
    else io.Fonts->AddFontDefault(&cfg);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_density);
    style.WindowRounding = 14 * g_density;
    style.FrameRounding = 8 * g_density;
    style.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(kCard);
    style.Colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(kAccent);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    g_ready = true;
    log("Panel listo (%dx%d)", g_width, g_height);
  }
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(g_width), static_cast<float>(g_height));
  io.DeltaTime = 1.0f / std::max(10.0f, g_fps > 1 ? g_fps : 60.0f);
  {
    std::lock_guard<std::mutex> lock(g_lock);
    io.AddMousePosEvent(g_pointer.x, g_pointer.y);
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
  drawHud();
  if (g_menu) drawMenu();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fbo));
}

}  // namespace

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
