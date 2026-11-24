#include "platform/windows/WindowsPlatform.h"

#include <windows.h>

namespace lex::platform::windows {

WindowsPlatform::WindowsPlatform() : start_(std::chrono::steady_clock::now()) {}

std::string_view WindowsPlatform::name() const { return "windows-x64"; }

core::Vec2 WindowsPlatform::screenSize() const {
  return core::Vec2{static_cast<float>(GetSystemMetrics(SM_CXSCREEN)),
                    static_cast<float>(GetSystemMetrics(SM_CYSCREEN))};
}

float WindowsPlatform::nowSeconds() const {
  const std::chrono::duration<float> elapsed =
      std::chrono::steady_clock::now() - start_;
  return elapsed.count();
}

float WindowsPlatform::fps() const { return fps_; }

core::KeyState WindowsPlatform::keyState(int keyCode) const {
  return (GetAsyncKeyState(keyCode) & 0x8000) != 0 ? core::KeyState::Down
                                                   : core::KeyState::Up;
}

void WindowsPlatform::setCursorVisible(bool visible) {
  // ShowCursor usa un contador por hilo: se ajusta hasta alcanzar el estado.
  const int limit = 64;
  if (visible) {
    for (int attempt = 0; attempt < limit && ShowCursor(TRUE) < 0; ++attempt) {
    }
  } else {
    for (int attempt = 0; attempt < limit && ShowCursor(FALSE) >= 0; ++attempt) {
    }
  }
}

void WindowsPlatform::drawRect(const core::Rect& rect, std::uint32_t argb) {
  (void)rect;
  (void)argb;
  // TODO Fase 2: backend ImGui dx11/dx12.
}

void WindowsPlatform::drawText(const core::Vec2& position, std::string_view text,
                               std::uint32_t argb, float fontSize) {
  (void)position;
  (void)text;
  (void)argb;
  (void)fontSize;
  // TODO Fase 2: backend ImGui dx11/dx12.
}

void WindowsPlatform::drawTexture(std::string_view assetId,
                                  const core::Rect& rect,
                                  std::uint32_t tintArgb) {
  (void)assetId;
  (void)rect;
  (void)tintArgb;
  // TODO Fase 2: texturas embebidas/iconos.
}

core::PlayerSnapshot WindowsPlatform::playerSnapshot() const {
  // TODO(INVESTIGACION): No hay fuente de datos fiable todavia.
  // Para coords/rotacion/bioma/ping necesito, de una version concreta del juego:
  //   1) patron de bytes (signature) de la funcion que actualiza la camara/
  //      PlayerPosition,	o el offset del Level::Player dentro del modulo del juego.
  //   2) direccion de la funcion de tick del world para publicar OnWorldJoin.
  // Hasta entonces se devuelve el snapshot vacio: NUNCA datos inventados.
  return core::PlayerSnapshot{};
}

void WindowsPlatform::onFrame(float deltaSeconds) {
  if (deltaSeconds <= 0.0f) return;
  accumulator_ += deltaSeconds;
  ++frames_;
  if (accumulator_ >= 0.5f) {
    fps_ = static_cast<float>(frames_) / accumulator_;
    accumulator_ = 0.0f;
    frames_ = 0;
  }
}

}  // namespace lex::platform::windows
