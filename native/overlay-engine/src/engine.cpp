// LEX CLIENT — Overlay Engine: implementación Win32 + Direct2D. MinGW-first, MSVC-compatible.
#include "engine.h"
#include "bridge.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <algorithm>

#if defined(_MSC_VER)
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

namespace lex {
namespace engine {

static OverlayEngine* g_instance = nullptr;
OverlayEngine* OverlayEngine::instance() { return g_instance; }

static std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n > 0 ? n : 1, L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

static std::string wideToUtf8(const std::wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n > 0 ? n : 1, '\0');
  if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
  return s;
}

static std::string sanitizeId(const std::string& name) {
  std::string out = "custom-";
  for (unsigned char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out += (char)c;
    else if (c == ' ') out += '-';
  }
  if (out == "custom-" || out == "custom") out = "custom-overlay";
  return out;
}

// MinGW: usemos GUID estático estático del factory (sin IID externo).
static const GUID kIdWriteFactory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};


OverlayEngine::OverlayEngine() {
  stats_.gamePos.source = "none";
  g_instance = this;
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

OverlayEngine::~OverlayEngine() {
  if (textSmall_) textSmall_->Release();
  if (accentBrush_) accentBrush_->Release();
  if (dimBrush_) dimBrush_->Release();
  if (brush_) brush_->Release();
  if (target_) target_->Release();
  if (writeFactory_) writeFactory_->Release();
  if (factory_) factory_->Release();
  if (hWnd_) DestroyWindow(hWnd_);
  UnregisterClassW(L"LexOverlayWnd", GetModuleHandleW(nullptr));
  CoUninitialize();
  g_instance = nullptr;
}

ATOM OverlayEngine::registerWindowClass(HINSTANCE hInstance) {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"LexOverlayWnd";
  wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
  wc.style |= CS_OWNDC;
  return RegisterClassW(&wc);
}

static HWND createWindow() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  HWND hwnd = CreateWindowExW(exStyle, L"LexOverlayWnd", L"lex-overlay", WS_POPUP, 10, 10, 460, 420, nullptr, nullptr, hInstance, nullptr);
  if (hwnd) {
    // Transparencia por color key: negro = transparente, el resto visible.
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
  }
  return hwnd;
}

static void enableClickThrough(HWND hwnd, bool enable) {
  DWORD ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
  if (enable) ex |= WS_EX_TRANSPARENT;
  else ex &= ~WS_EX_TRANSPARENT;
  SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayEngine::setVisible(bool v) {
  if (!hWnd_) return;
  if (v) {
    SetWindowPos(hWnd_, HWND_TOPMOST, config_.positionX, config_.positionY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd_);
  } else {
    ShowWindow(hWnd_, SW_HIDE);
  }
}

void OverlayEngine::updatePosition() {
  if (!hWnd_) return;
  SetWindowPos(hWnd_, HWND_TOPMOST, config_.positionX, config_.positionY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void OverlayEngine::toggleClickThrough() {
  if (!hWnd_) return;
  config_.clickThrough = !config_.clickThrough;
  enableClickThrough(hWnd_, config_.clickThrough);
}

void OverlayEngine::updateGameFps() {
  float dt = std::max(0.001f, std::chrono::duration<float>(std::chrono::steady_clock::now() - lastRender_).count());
  float localFps = (dt > 0.001f) ? (1.0f / dt) : 0.f;
  float ema = (gameFpsEma_ < 0) ? localFps : (gameFpsEma_ * 0.75f + localFps * 0.25f);
  gameFpsEma_ = ema;
  if (stats_.gameFpsTga != -1) stats_.overlayFps = (float)stats_.gameFpsTga;
  else stats_.overlayFps = ema;
}

void OverlayEngine::updateCps() {
  stats_.cpsAvg = stats_.clickCount > 0 ? stats_.clickCount * 5 : 0;
  cpsRing[cpsRingIdx_] = stats_.clickCount;
  cpsRingIdx_ = (cpsRingIdx_ + 1) % 4;
  int sum = 0;
  for (int i = 0; i < 4; i++) sum += cpsRing[i];
  if (sum > 0) stats_.cpsAvg = (sum / 4) * 5;
  stats_.clickCount = 0;
}

void OverlayEngine::registerLocalClick() {
  stats_.clickCount++;
  requestTick();
}

void OverlayEngine::registerKeystroke(int vk) {
  // Hotkey del menú in-game (F8 por defecto): overlay externo, sin inyección.
  if (vk == menuHotkey_) {
    menuVisible_ = !menuVisible_;
    menuPage_ = 0;
    if (menuVisible_) reloadBridgeState();
    requestTick();
    return;
  }
  if (menuVisible_) {
    // Alta de overlay personalizado: escribe un nombre y Enter.
    if (addingOverlay_) {
      if (vk == VK_RETURN) {
        const std::string raw = wideToUtf8(pendingName_);
        if (!raw.empty()) {
          bridge::ModuleState custom;
          custom.id = sanitizeId(raw);
          custom.enabled = true;
          const bool exists = std::any_of(modules_.begin(), modules_.end(), [&](const bridge::ModuleState& m) { return m.id == custom.id; });
          if (!exists) {
            modules_.push_back(custom);
            bridge::writeModuleStates(bridge::getModulesStatePath(), modules_);
          }
        }
        addingOverlay_ = false;
        pendingName_.clear();
        menuPage_ = 0;
        requestTick();
        return;
      }
      if (vk == VK_ESCAPE) { addingOverlay_ = false; pendingName_.clear(); requestTick(); return; }
      if (vk == VK_BACK && !pendingName_.empty()) { pendingName_.pop_back(); requestTick(); return; }
      if (pendingName_.size() < 40) {
        if (vk >= 'A' && vk <= 'Z') pendingName_ += (wchar_t)('A' + (vk - 'A'));
        else if (vk >= '0' && vk <= '9') pendingName_ += (wchar_t)('0' + (vk - '0'));
        else if (vk == VK_SPACE) pendingName_ += L' ';
        else if (vk == VK_OEM_MINUS) pendingName_ += L'-';
        else if (vk == VK_OEM_5) pendingName_ += L'_';
        requestTick();
        return;
      }
    }
    if (vk == VK_ESCAPE) { menuVisible_ = false; addingOverlay_ = false; pendingName_.clear(); requestTick(); return; }
    if (vk == 'A' && menuPage_ == 0) { addingOverlay_ = true; pendingName_.clear(); requestTick(); return; }
    if (vk == 'M') { menuPage_ = 0; requestTick(); return; }
    if (vk == 'C') { menuPage_ = 1; reloadBridgeState(); requestTick(); return; }
    if (vk == 'H') { menuPage_ = 2; requestTick(); return; }
    if (vk >= '1' && vk <= '9') {
      const size_t idx = static_cast<size_t>(vk - '1');
      if (menuPage_ == 0 && idx < modules_.size()) {
        modules_[idx].enabled = !modules_[idx].enabled;
        bridge::writeModuleStates(bridge::getModulesStatePath(), modules_);
        requestTick();
        return;
      }
    }
  }
  // Keystrokes capturados: almacenamos en ring de KEY_LOG_SIZE.
  // Fuente honesta: engine overlay (LLM teclado passthrough).
  if (keyLog_.size() >= KEY_LOG_SIZE) keyLog_.erase(keyLog_.begin());
  keyLog_.push_back({ vk, true });
  requestTick();
}

void OverlayEngine::pumpKeys() {
  // Consume a los hooks (stats sync). Timer-side.
}

void OverlayEngine::writeOverlayStatsFile() {
  bridge::OverlayStats st;
  st.overlayFps = stats_.overlayFps;
  st.gameFpsTga = stats_.gameFpsTga;
  st.cpsAvg = stats_.cpsAvg;
  st.cpsClickCount = stats_.clickCount;
  st.keyCount = (int)keyLog_.size();
  st.menuVisible = menuVisible_;
  st.menuPage = menuPage_;
  st.moduleCount = (int)modules_.size();
  st.cosmeticCount = (int)cosmetics_.size();
  st.timestamp = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  bridge::writeOverlayStats(bridge::getOverlayStatsPath(), st);
}

LRESULT CALLBACK OverlayEngine::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = g_instance;
  switch (uMsg) {
    case WM_TIMER:
      if (self) self->onTimerTick();
      break;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      if (self) self->registerKeystroke((int)wParam);
      break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
      if (self) self->registerLocalClick();
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void OverlayEngine::onTimerTick() {
  tickCount_++;
  updateGameFps();
  if (tickCount_ % 5 == 0) updateCps();
  const double age = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastBridgeReload_).count();
  if (age > 1.0) reloadBridgeState();
  writeOverlayStatsFile();
  renderFrame();
}

void OverlayEngine::renderFrame() {
  if (!target_) return;
  lastRender_ = std::chrono::steady_clock::now();

  target_->BeginDraw();
  // Negro opaco = transparente por LWA_COLORKEY.
  target_->Clear(D2D1_COLOR_F(0.f, 0.f, 0.f, 1.f));

  if (menuVisible_) {
    renderMenu();
    target_->EndDraw();
    frameCount_++;
    return;
  }

  const float scale = 1.0f;
  float opacity = config_.opacity > 0 ? config_.opacity : 0.9f;
  D2D1_COLOR_F accent = D2D1_COLOR_F(0.0078f, 0.7843f, 0.9216f, opacity);

  // Cubo pequeño identificativo (brand) en la parte superior.
  {
    D2D1_RECT_F bg = {0, 0, 26 * scale, 26 * scale};
    brush_->SetColor(D2D1_COLOR_F(0.1f, 0.117f, 0.14f, 0.85f));
    target_->FillRectangle(bg, brush_);
    brush_->SetColor(accent);
    target_->DrawRectangle(bg, brush_, 1.5f * scale);

    D2D1_RECT_F L = {5 * scale, 11 * scale, 14 * scale, 15 * scale};
    D2D1_RECT_F A = {3 * scale, 4 * scale, 8 * scale, 20 * scale};
    D2D1_RECT_F B = {14 * scale, 4 * scale, 19 * scale, 20 * scale};
    target_->FillRectangle(L, brush_);
    target_->FillRectangle(A, brush_);
    target_->FillRectangle(B, brush_);
  }

  // Mini-HUD de textos (izquierda, debajo del cubo).
  float y = 22 * scale;
  auto drawLine = [&](const std::wstring& s, float x) {
    if (!s.empty()) {
      brush_->SetColor(accent);
      D2D1_RECT_F rect{x, y, 400.f * scale, y + 16.f * scale};
      target_->DrawText(s.c_str(), (UINT32)s.size(), textSmall_, &rect, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
    }
    y += 14.f * scale;
  };
  drawLine(L"FPS", 4.f);
  {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", (int)(stats_.overlayFps + 0.5f));
    drawLine(buf, 4.f);
  }
  drawLine(L"CPS", 4.f);
  {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", stats_.cpsAvg);
    drawLine(buf, 4.f);
  }
  // Keystrokes (ring de engine, honesto).
  {
    std::wstring keysStr;
    for (const auto& k : keyLog_) {
      if (!keysStr.empty()) keysStr += L" ";
      wchar_t kb[8];
      swprintf(kb, 8, L"%03X", k.vk);
      keysStr += kb;
    }
    if (!keysStr.empty()) drawLine(keysStr, 4.f);
  }
  // Overlays personalizados creados desde el menú in-game (F8 → A).
  {
    int shown = 0;
    for (const auto& m : modules_) {
      if (!m.enabled || m.id.rfind("custom-", 0) != 0) continue;
      if (shown++ >= 6) break;
      const std::string label = m.id.substr(7);
      if (!label.empty()) drawLine(utf8ToWide(label), 4.f);
    }
  }

  target_->EndDraw();
  frameCount_++;
}

void OverlayEngine::renderMenu() {
  if (!target_ || !brush_ || !textSmall_) return;

  auto W = [](const std::string& s) { return utf8ToWide(s); };

  float opacity = config_.opacity > 0 ? config_.opacity : 0.9f;
  D2D1_COLOR_F accent = D2D1_COLOR_F(0.0078f, 0.7843f, 0.9216f, opacity);

  D2D1_RECT_F panel = {0, 0, 460, 420};
  brush_->SetColor(D2D1_COLOR_F(0.063f, 0.067f, 0.086f, 0.94f));
  target_->FillRectangle(panel, brush_);
  brush_->SetColor(accent);
  target_->DrawRectangle(panel, brush_, 1.5f);

  float y = 12.f;
  auto line = [&](const std::wstring& s, float size = 14.f) {
    if (s.empty()) { y += size; return; }
    brush_->SetColor(accent);
    D2D1_RECT_F r = {12.f, y, 448.f, y + size + 2.f};
    target_->DrawText(s.c_str(), (UINT32)s.size(), textSmall_, &r, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
    y += size;
  };

  line(L"LEX CLIENT  -  MENU IN-GAME", 18.f);
  if (addingOverlay_) {
    line(L"NUEVO OVERLAY — escribe un nombre y ENTER", 13.f);
    std::wstring shown = pendingName_ + L'|';
    line(shown, 16.f);
    line(L"ESC cancela", 12.f);
    target_->EndDraw();
    return;
  }
  line(L"[M] MODULOS  [A] AÑADIR OVERLAY  [C] COSMETICOS  [H] AYUDA", 12.f);
  y += 6.f;

  if (menuPage_ == 0) {
    if (modules_.empty()) {
      line(L"Sin modulos guardados. Abre el launcher.", 13.f);
    } else {
      const size_t max = modules_.size() < 15 ? modules_.size() : 15;
      for (size_t i = 0; i < max; ++i) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[%s] %d  %s", modules_[i].enabled ? "X" : " ", (int)(i + 1), modules_[i].id.c_str());
        line(W(buf), 13.f);
      }
    }
  } else if (menuPage_ == 1) {
    if (cosmetics_.empty()) {
      line(L"Sin cosmeticos. Importa JSON en el launcher.", 13.f);
      line(L"(se guardan en %APPDATA%\\lex-client)", 12.f);
    } else {
      const size_t max = cosmetics_.size() < 15 ? cosmetics_.size() : 15;
      for (size_t i = 0; i < max; ++i) {
        char buf[192];
        snprintf(buf, sizeof(buf), "%d  %s  [%s]", (int)(i + 1), cosmetics_[i].name.c_str(), cosmetics_[i].source.c_str());
        line(W(buf), 13.f);
      }
    }
  } else {
    line(L"F8      Abrir / cerrar este menu", 13.f);
    line(L"M       Pagina de modulos", 13.f);
    line(L"C       Pagina de cosmeticos", 13.f);
    line(L"A       Añadir overlay personalizado (en MODULOS)", 13.f);
    line(L"1 - 9   Activar / desactivar modulo", 13.f);
    line(L"ESC     Cerrar menu", 13.f);
    line(L"", 6.f);
    line(L"Overlay externo: sin inyeccion ni archivos del juego.", 12.f);
    line(L"Los cosmeticos son locales de LEX CLIENT.", 12.f);
  }

  brush_->SetColor(accent);
  D2D1_RECT_F footer = {12.f, 396.f, 448.f, 414.f};
  const wchar_t* hint = L"F8 cerrar | M modulos | A añadir | C cosmeticos | 1-9 toggle";
  target_->DrawText(hint, (UINT32)wcslen(hint), textSmall_, &footer, brush_, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
}

void OverlayEngine::reloadState() {
  // Lee %APPDATA%\lex-client\overlay-state.json (visual/position/click-through).
  bridge::OverlayState st = bridge::readOverlayState(bridge::getOverlayStatePath());
  config_.visible = st.visible;
  config_.clickThrough = st.clickThrough;
  config_.hideOnGameMinimize = st.hideOnGameMinimize;
  if (st.fpsCap > 0 && st.fpsCap <= 1000) config_.fpsCap = st.fpsCap;
  if (st.positionX >= 0 && st.positionX <= 5000) config_.positionX = st.positionX;
  if (st.positionY >= 0 && st.positionY <= 5000) config_.positionY = st.positionY;
  if (st.opacity >= 0.f && st.opacity <= 1.f) config_.opacity = st.opacity;
  updatePosition();
  if (config_.visible) ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
  else ShowWindow(hWnd_, SW_HIDE);
}

void OverlayEngine::reloadBridgeState() {
  modules_ = bridge::readModuleStates(bridge::getModulesStatePath());
  cosmetics_ = bridge::readCosmetics(bridge::getCosmeticsPath());
  lastBridgeReload_ = std::chrono::steady_clock::now();
}

int OverlayEngine::run() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  if (!registerWindowClass(hInstance)) { fprintf(stderr, "[LEX] registerWindowClass falló\n"); return 1; }
  hWnd_ = createWindow();
  if (!hWnd_) { fprintf(stderr, "[LEX] createWindow falló\n"); return 1; }

  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_PPV_ARGS(&factory_));
  if (FAILED(hr)) { fprintf(stderr, "[LEX] D2D1CreateFactory falló hr=%lx\n", (unsigned long)hr); return 1; }
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, kIdWriteFactory, reinterpret_cast<IUnknown**>(&writeFactory_));
  if (FAILED(hr)) { fprintf(stderr, "[LEX] DWriteCreateFactory falló hr=%lx\n", (unsigned long)hr); return 1; }

  D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties();
  D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd_, D2D1_SIZE_U{0, 0}, D2D1_PRESENT_OPTIONS_NONE);

  factory_->CreateHwndRenderTarget(&rtp, &hwndProps, &target_);
  if (!target_) { fprintf(stderr, "[LEX] CreateHwndRenderTarget falló\n"); return 1; }

  D2D1_COLOR_F white = D2D1::ColorF(0.95f, 0.95f, 0.97f, 0.95f);
  D2D1_COLOR_F accentColor = D2D1::ColorF(0.0078f, 0.7843f, 0.9216f, 0.9f);
  D2D1_COLOR_F dimColor = D2D1::ColorF(0.25f, 0.25f, 0.30f, 0.5f);
  target_->CreateSolidColorBrush(&white, nullptr, &brush_);
  target_->CreateSolidColorBrush(&accentColor, nullptr, &accentBrush_);
  target_->CreateSolidColorBrush(&dimColor, nullptr, &dimBrush_);
  writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.f, L"en-us", &textSmall_);

  reloadState();
  SetTimer(hWnd_, 1, config_.tickMs, nullptr);
  SetTimer(hWnd_, 2, 500, nullptr);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return (int)msg.wParam;
}

}  // namespace engine
}  // namespace lex
