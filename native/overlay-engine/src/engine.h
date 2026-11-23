// LEX CLIENT — Overlay Engine: headers (Win32 + Direct2D). MinGW-first, MSVC-compatible.
#pragma once
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <chrono>
#include <vector>
#include <cstddef>
#include "bridge.h"

namespace lex {
namespace engine {

// ---- Config del overlay ----
struct Config {
  bool visible = true;
  bool clickThrough = false;
  bool hideOnGameMinimize = true;
  int fpsCap = 144;
  int tickMs = 200;
  int positionX = 10;
  int positionY = 10;
  float opacity = 0.90f;
};

// ---- Estado compartido con módulos de Fase 4 (escrito por engine.cpp cada ~1s) ----
struct Stats {
  float overlayFps = 0.f;
  int gameFpsTga = -1;   // best effort (requiere hook TGA);
  int cpsAvg = 0;
  int cpsClickCount = 0;
  int clickCount = 0;
  int keyCount = 0;
  struct {
    std::string source = "none";
    int x = 0, y = 0, z = 0;
  } gamePos;
};

// ---- Clase principal: ventana transparente + loop de render ----
class OverlayEngine {
public:
  OverlayEngine();
  ~OverlayEngine();

  static OverlayEngine* instance();

  // Para el bridge: carga state/config de %APPDATA%\lex-client\overlay-state.json
  void reloadState();
  // Recarga módulos y cosméticos compartidos con el launcher.
  void reloadBridgeState();

  // Main message loop — llamado desde main.cpp.
  int run();

  // Estadísticas actualizadas por tics.
  const Stats& stats() const { return stats_; }
  const std::vector<bridge::KeyState>& keyStates() const { return keyLog_; }

  // Simula clic local (para cps). Llamado por hooks.cpp y WndProc.
  void registerLocalClick();
  // Keystroke capturado por hook LLM (passthrough). No bloqueamos.
  void registerKeystroke(int vk);
  void pumpKeys();

  // called by WndProc WM_TIMER.
  void onTimerTick();

  void requestTick() { tickCount_++; }

public:
  static ATOM registerWindowClass(HINSTANCE hInstance);
private:
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  void renderFrame();
  void renderMenu();
  void updateGameFps();
  void updateCps();
  void writeOverlayStatsFile();
  void setVisible(bool v);
  void updatePosition();
  void toggleClickThrough();

  HWND hWnd_ = nullptr;
  ID2D1Factory* factory_ = nullptr;
  ID2D1HwndRenderTarget* target_ = nullptr;
  IDWriteFactory* writeFactory_ = nullptr;
  ID2D1SolidColorBrush* brush_ = nullptr;
  ID2D1SolidColorBrush* accentBrush_ = nullptr;
  ID2D1SolidColorBrush* dimBrush_ = nullptr;
  IDWriteTextFormat* textSmall_ = nullptr;

  Stats stats_;

  Config config_;

  std::chrono::steady_clock::time_point lastRender_, lastStatsTick_;
  int tickCount_ = 0;
  int frameCount_ = 0;
  int cpsRing[4] = {0, 0, 0, 0};
  int cpsRingIdx_ = 0;
  float gameFpsEma_ = -1.f;

  std::vector<bridge::KeyState> keyLog_;
  static constexpr size_t KEY_LOG_SIZE = 8;

  // Menú in-game: overlay externo (sin inyección en el juego).
  bool menuVisible_ = false;
  int menuPage_ = 0;               // 0=módulos, 1=cosméticos, 2=ayuda
  int menuHotkey_ = VK_F8;
  std::vector<bridge::ModuleState> modules_;
  std::vector<bridge::Cosmetic> cosmetics_;
  std::chrono::steady_clock::time_point lastBridgeReload_;

  // Alta de overlays personalizados desde el menú in-game.
  bool addingOverlay_ = false;
  std::wstring pendingName_;
};

}  // namespace engine
}  // namespace lex
