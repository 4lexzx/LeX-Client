#pragma once

#include <chrono>

#include "core/platform/IPlatform.h"

namespace lex::platform::windows {

// Unica implementacion concreta de IPlatform para Windows en esta fase.
// DirectX/ImGui entraran en Fase 2; aqui los primitivos de dibujo son no-op.
class WindowsPlatform final : public core::IPlatform {
 public:
  WindowsPlatform();

  std::string_view name() const override;
  core::Vec2 screenSize() const override;
  float nowSeconds() const override;
  float fps() const override;

  core::KeyState keyState(int keyCode) const override;
  void setCursorVisible(bool visible) override;

  void drawRect(const core::Rect& rect, std::uint32_t argb) override;
  void drawText(const core::Vec2& position, std::string_view text,
                std::uint32_t argb, float fontSize) override;
  void drawTexture(std::string_view assetId, const core::Rect& rect,
                   std::uint32_t tintArgb) override;

  core::PlayerSnapshot playerSnapshot() const override;

  // Lo llama el export lexTick antes de publicar los eventos.
  void onFrame(float deltaSeconds);

 private:
  std::chrono::steady_clock::time_point start_;
  float fps_ = 0.0f;
  float accumulator_ = 0.0f;
  int frames_ = 0;
};

}  // namespace lex::platform::windows
