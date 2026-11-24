// LEX CLIENT — Overlay Engine: headers (Win32 + superficie BGRA). MinGW-first, MSVC-compatible.
// Nota: el overlay es una ventana WS_EX_LAYERED. Se renderiza en un DIB 32bpp
// premultiplicado y se publica con UpdateLayeredWindow(ULW_ALPHA), así cada
// píxel lleva su propio alfa y el juego se ve a través de los paneles.
#pragma once
#include <Windows.h>
#include <string>
#include <chrono>
#include <vector>
#include <deque>
#include <cstddef>
#include "bridge.h"

namespace lex {
namespace engine {

// Superficie de pixels BGRA premultiplicada que se entrega a UpdateLayeredWindow.
// Está declarada aquí para que las funciones de render privadas puedan usarla.
struct Painter;

// ---- Config del overlay ----
struct Config {
  bool visible = true;
  bool clickThrough = false;
  bool hideOnGameMinimize = true;
  int fpsCap = 144;
  int tickMs = 100;
  int positionX = 10;
  int positionY = 10;
  float opacity = 0.90f;
  // 0 = sin fondo. Valores 0..100 = transparencia ajustable del panel.
  int menuOpacity = 95;
  int hudOpacity = 0;
};

// ---- Estado compartido con módulos de Fase 4 (escrito por engine.cpp cada ~1s) ----
struct Stats {
  float overlayFps = 0.f;
  int gameFpsTga = -1;   // FPS reales del juego (hook de Present);
  std::string gameBridgeApi;  // d3d11 | d3d12 | ""
  int cpsAvg = 0;
  int cpsLeft = 0;
  int cpsRight = 0;
  int cpsClickCount = 0;
  int clickCount = 0;
  int keyCount = 0;
  struct {
    std::string source = "none";
    int x = 0, y = 0, z = 0;
  } gamePos;
};

// ---- Textura animada propia (formato LEXTEX1: BGRA premultiplicado) ----
struct CosmeticTexture {
  std::string path;
  HBITMAP bitmap = nullptr;
  HDC dc = nullptr;
  void* bits = nullptr;
  int width = 0;
  int height = 0;
  int frames = 1;
  int frameMs = 100;
};

// ---- Clase principal: ventana transparente + loop de render ----
class OverlayEngine {
public:
  OverlayEngine();
  ~OverlayEngine();

  static OverlayEngine* instance();

  // Vincula el overlay a la ventana del juego (z-order / minimizado).
  void setGameWindow(HWND hwnd) { gameHwnd_ = hwnd; }

  // Para el bridge: carga state/config de %APPDATA%\lex-client\overlay-state.json
  void reloadState();
  // Persiste transparencia/posición en overlay-state.json.
  void saveOverlayConfig();
  // Recarga módulos y cosméticos compartidos con el launcher.
  void reloadBridgeState();
  // Estilo por módulo: tamaño, opacidad, color, fondo y preset de mira.
  void reloadStyles();
  bridge::ModuleStyle styleFor(const std::string& id) const;
  void saveModuleStyle(const bridge::ModuleStyle& st);
  // Perfiles de módulos (panel izquierdo del ClickGUI).
  void reloadProfiles();
  void applyProfile(int index);
  void saveNewProfile();

  // Inyecta el cargador del puente en el PID indicado (antes de que el juego
  // cree su swapchain si el motor ya estaba esperando).
  void ensureGameBridgePid(DWORD pid);

  // Main message loop — llamado desde main.cpp.
  int run();

  // Estadísticas actualizadas por tics.
  const Stats& stats() const { return stats_; }
  const std::vector<bridge::KeyState>& keyStates() const { return keyLog_; }

  // Clic físico real (llamado por el hook global de ratón). Cada evento cuenta 1.
  void registerLocalClick(int vk);
  // Estado real de pulsación (para keystrokes en vivo).
  void registerKeyState(int vk, bool down);
  // Keystroke capturado por hook LLM (passthrough). No bloqueamos.
  void registerKeystroke(int vk);
  // true => el menú LEX usa esta tecla y NO debe llegar al juego/otras apps.
  bool consumeKey(int vk);
  void pumpKeys();

  // called by WndProc WM_TIMER.
  void onTimerTick();

  // Marca el frame pendiente: invalida el cliente para publicar el buffer BGRA.
  void requestTick() {
    tickCount_++;
    dirty_ = true;   // coalesce: el temporizador publica una sola vez por tick
  }

public:
  static ATOM registerWindowClass(HINSTANCE hInstance);
private:
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  // Renderiza el buffer BGRA y lo entrega a UpdateLayeredWindow (alpha real).
  void renderFrame();
  // Doble buffer: DIB 32bpp en memoria con canal alfa premultiplicado.
  bool ensureRenderBuffer(int w, int h);
  void releaseRenderBuffer();
  // Modal F8/HUD edit: liberar el cursor del juego y devolverlo al cerrar.
  void acquireMenuCursor();
  void releaseMenuCursor();
  bool gameIsForeground() const;
  // Sólo repinta cuando hay cambios reales: evita el parpadeo por clic.
  bool overlayNeedsFrame() const;
  // Inyección del puente de render (FPS reales) en el PID del juego.
  void ensureGameBridge();
  void renderMenu(Painter& p);
  void renderHud(Painter& p);
  // Dibuja el cosmético animado activo (frame por tiempo) sobre el HUD del juego.
  void renderCosmetics(Painter& p, RECT rc);
  bool ensureCosmeticTexture(const std::string& path);
  void releaseCosmeticTextures();
  void renderDetail(Painter& p);
  // Clic sobre el panel (coordenadas de cliente): categorías y tarjetas.
  void onMenuClick(int x, int y);
  // Rueda del ratón y arrastre de la barra de desplazamiento.
  void onMenuWheel(int delta, int x, int y);
  void onMenuMouseMove(int x, int y);
  void onMenuMouseUp();
  // Modo HUD edit: arrastrar paneles con el ratón (sin atajos de teclado).
  void enterHudEdit();
  void exitHudEdit(bool toMenu);
  void onHudMouseDown(int x, int y);
  void onHudMouseMove(int x, int y);
  void onHudMouseUp();
  // Ciclo de vida: sigue a la ventana del juego y se cierra con ella.
  void syncWithGame();
  // Coloca y dimensiona la ventana según el menú o el HUD, sobre el juego.
  void updateGeometry();
  void reloadCatalog();
  void reloadHudLayout();
  void saveHudLayout();
  // Índices de modules_ visibles en la categoría actual del mod menu.
  std::vector<size_t> visibleModules() const;
  // Módulos con representación real en el HUD del juego.
  std::vector<size_t> hudModules() const;
  std::wstring moduleValue(const std::string& id) const;
  void updateGameFps();
  void updateCps();
  void writeOverlayStatsFile();
  // Avisa al DLL inyectado de que el menú modal está abierto (cursor del juego).
  void writeUiStateNow();
  void setVisible(bool v);
  void updatePosition();
  void toggleClickThrough();

  HWND hWnd_ = nullptr;
  HWND gameHwnd_ = nullptr;
  // Fuentes GDI (creadas en run(), destruidas en el destructor).
  HFONT fontTitle_ = nullptr;  // encabezado del menú
  HFONT fontBody_ = nullptr;   // líneas del menú / HUD
  HFONT fontValue_ = nullptr;  // cifras del HUD
  HFONT fontSmall_ = nullptr;  // descripciones de tarjeta / hints

  Stats stats_;

  Config config_;

  std::chrono::steady_clock::time_point lastRender_, lastStatsTick_;
  int tickCount_ = 0;
  int frameCount_ = 0;
  bool dirty_ = true;   // un solo repaint por tick, aunque haya muchos clics
  bool resizingHud_ = false;
  int resizeIndex_ = -1;
  int resizeStartX_ = 0, resizeStartY_ = 0;
  int resizeStartSize_ = 100;
  bool scrollbarDrag_ = false;
  int scrollbarAnchorY_ = 0;
  int scrollbarStart_ = 0;
  int hotkeyCount_ = 0;
  // CPS exactos: un timestamp por clic físico, separado por botón (window 1000 ms).
  std::deque<long long> leftClickTimes_;
  std::deque<long long> rightClickTimes_;
  long long totalClicks_ = 0;
  float gameFpsEma_ = -1.f;

  // Doble buffer para que el menú no parpadee al repintarse.
  HDC bufferDc_ = nullptr;
  HBITMAP bufferBmp_ = nullptr;
  HGDIOBJ bufferOldBmp_ = nullptr;
  void* bufferBits_ = nullptr;   // puntero al DIB 32bpp (BGRA premultiplicado)
  int bufferW_ = 0, bufferH_ = 0;

  // Estado del cursor mientras el menú/HUD edit está abierto.
  bool cursorHeld_ = false;
  RECT savedClip_ = {};
  HCURSOR savedCursor_ = nullptr;
  int savedShowCount_ = 0;

  // La geometría sólo se toca cuando cambia: llamar a SetWindowPos cada tick
  // hace que la ventana layered parpadee sobre el juego.
  RECT lastGeomRect_{};
  bool lastGeomValid_ = false;
  bool lastGeomClickThrough_ = false;
  bool lastGameShown_ = true;
  bool lastGameFocused_ = true;

  // DLL de puente inyectada en el juego (una vez por PID).
  DWORD injectedPid_ = 0;

  std::vector<bridge::KeyState> keyLog_;
  static constexpr size_t KEY_LOG_SIZE = 8;

  // Menú in-game: overlay externo (sin inyección en el juego).
  bool menuVisible_ = false;
  bool hudEdit_ = false;           // HUD edit mode: paneles arrastrables sobre el juego
  bool keyboardOpen_ = false;      // teclado en pantalla: buscar sin usar teclas
  std::wstring searchText_;
  bool detailOpen_ = false;        // panel de ajustes del módulo seleccionado
  int menuPage_ = 0;               // 0 = MODS, 1 = COSMETICS, 2 = HUD
  int menuSel_ = 0;                // tarjeta seleccionada dentro de la categoría
  int menuScroll_ = 0;             // primera fila visible de la rejilla
  int cosmeticPage_ = 0;           // página de la galería visual de cosméticos
  int menuHotkey_ = VK_F8;
  std::vector<bridge::ModuleState> modules_;
  std::vector<bridge::ModuleStyle> styles_;
  std::vector<bridge::Profile> profiles_;
  int activeProfile_ = 0;
  std::vector<bridge::ModuleMeta> catalog_;
  std::vector<bridge::Cosmetic> cosmetics_;
  std::vector<CosmeticTexture> cosmeticTextures_;
  std::string cosmeticsSignature_;  // evita recargar atlas cada segundo
  std::vector<bridge::HudPos> hudLayout_;
  std::chrono::steady_clock::time_point lastBridgeReload_;
  std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();

  // Estado real de teclado para keystrokes (no sólo ring de pulsaciones).
  bool keyDown_[256] = {};
  // Momento de la última pulsación: da el flash corto de los keycaps tipo Flarial.
  std::chrono::steady_clock::time_point keyFlash_[256]{};
  int dragIndex_ = -1;
  bool draggingHud_ = false;
  int dragOffX_ = 0, dragOffY_ = 0;

  // Ciclo de vida atado a la ventana del juego (es un cliente del juego,
  // no algo de todo el sistema): la ventana se cierra con el juego.
  int gameGoneTicks_ = 0;
};

}  // namespace engine
}  // namespace lex
