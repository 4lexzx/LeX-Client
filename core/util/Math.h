#pragma once

#include <algorithm>
#include <cmath>

namespace lex::core::util {

template <typename T>
constexpr T clamp(T value, T minimum, T maximum) noexcept {
  return std::clamp(value, minimum, maximum);
}

constexpr float lerp(float a, float b, float t) noexcept {
  return a + (b - a) * t;
}

// Interpolacion suave dependiente del framerate (util en animaciones Fase 5).
inline float damp(float current, float target, float smoothing,
                  float deltaSeconds) noexcept {
  return lerp(current, target,
              1.0f - std::pow(smoothing, std::max(deltaSeconds, 0.0f)));
}

}  // namespace lex::core::util
