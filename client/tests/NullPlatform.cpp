#include "tests/NullPlatform.h"

namespace lex::tests {

std::string_view NullPlatform::name() const { return "null-test"; }

core::Vec2 NullPlatform::screenSize() const { return core::Vec2{1920.0f, 1080.0f}; }

float NullPlatform::nowSeconds() const { return 0.0f; }

float NullPlatform::fps() const { return 60.0f; }

core::KeyState NullPlatform::keyState(int keyCode) const {
  (void)keyCode;
  return core::KeyState::Up;
}

void NullPlatform::setCursorVisible(bool visible) { (void)visible; }

void NullPlatform::drawRect(const core::Rect& rect, std::uint32_t argb) {
  (void)rect;
  (void)argb;
}

void NullPlatform::drawText(const core::Vec2& position, std::string_view text,
                            std::uint32_t argb, float fontSize) {
  (void)position;
  (void)text;
  (void)argb;
  (void)fontSize;
}

void NullPlatform::drawTexture(std::string_view assetId, const core::Rect& rect,
                               std::uint32_t tintArgb) {
  (void)assetId;
  (void)rect;
  (void)tintArgb;
}

core::PlayerSnapshot NullPlatform::playerSnapshot() const {
  return core::PlayerSnapshot{};
}

}  // namespace lex::tests
