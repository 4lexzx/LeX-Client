#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lex::core {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

enum class KeyState : std::uint8_t { Up = 0, Down = 1 };

// Instantanea de datos del jugador. Los valores por defecto significan
// "sin fuente de datos". NUNCA rellenar con valores inventados.
struct PlayerSnapshot {
  bool inWorld = false;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  int pingMs = -1;
  std::string biome;
  std::string dimension;
};

// Unica frontera entre el nucleo portable y la plataforma.
// Este archivo NO incluye Windows.h, DirectX ni ningun header de Android.
class IPlatform {
 public:
  virtual ~IPlatform() = default;

  virtual std::string_view name() const = 0;
  virtual Vec2 screenSize() const = 0;
  virtual float nowSeconds() const = 0;
  virtual float fps() const = 0;

  virtual KeyState keyState(int keyCode) const = 0;
  virtual void setCursorVisible(bool visible) = 0;

  virtual void drawRect(const Rect& rect, std::uint32_t argb) = 0;
  virtual void drawText(const Vec2& position, std::string_view text,
                        std::uint32_t argb, float fontSize = 14.0f) = 0;
  virtual void drawTexture(std::string_view assetId, const Rect& rect,
                           std::uint32_t tintArgb) = 0;

  virtual PlayerSnapshot playerSnapshot() const = 0;
};

}  // namespace lex::core
