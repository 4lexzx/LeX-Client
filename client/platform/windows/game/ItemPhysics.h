#pragma once

namespace lex::platform::windows::game {

// Objetos tirados que giran al caer y quedan tumbados. Se engancha la llamada
// de giro del propio juego, localizada por su codigo y sus constantes.
class ItemPhysics {
 public:
  static bool install();  // una vez, al arrancar
  static bool available();
  static void configure(bool on, float tumble, float height);
};

}  // namespace lex::platform::windows::game
