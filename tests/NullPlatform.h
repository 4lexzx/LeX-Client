#pragma once

#include "core/platform/IPlatform.h"

namespace lex::tests {

// Plataforma falsa para tests del nucleo: sin Windows.h y sin DirectX.
class NullPlatform final : public core::IPlatform {
 public:
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
};

}  // namespace lex::tests
