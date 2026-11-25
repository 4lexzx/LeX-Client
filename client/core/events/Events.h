#pragma once

#include <string>

namespace lex::core::events {

struct Event {
  bool cancelled = false;
};

struct TickEvent : Event {
  float deltaSeconds = 0.0f;
};

struct RenderEvent : Event {
  float deltaSeconds = 0.0f;
};

struct KeyEvent : Event {
  int key = 0;
  bool pressed = false;
  bool repeat = false;
};

struct MouseEvent : Event {
  int button = 0;
  bool pressed = false;
  float x = 0.0f;
  float y = 0.0f;
};

struct WorldJoinEvent : Event {
  std::string worldName;
};

struct ShutdownEvent : Event {};

}  // namespace lex::core::events
