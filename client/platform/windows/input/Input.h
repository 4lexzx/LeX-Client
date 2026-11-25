#pragma once

#include <windows.h>

#include <array>
#include <deque>
#include <mutex>

namespace lex::platform::windows {

// Estado de teclado/raton del juego. Solo se muestrea mientras la ventana del
// juego esta en primer plano y nunca se escribe a disco.
class Input {
 public:
  static Input& instance();

  void sample(HWND window);
  // Hilo con gancho de raton: no pierde clics entre fotogramas.
  void startClickCounter();

  bool down(int key) const noexcept { return valid(key) && now_[key]; }
  bool pressed(int key) const noexcept {
    return valid(key) && now_[key] && !before_[key];
  }
  ULONGLONG lastLeftClick() const noexcept { return lastLeft_; }
  // El raton esta capturado por el juego: no hay chat, inventario ni menus.
  bool gameplay() const noexcept { return gameplay_; }
  bool focused() const noexcept { return focused_; }

  int leftCps() const noexcept { return static_cast<int>(left_.size()); }
  int rightCps() const noexcept { return static_cast<int>(right_.size()); }
  unsigned long long totalClicks() const noexcept { return total_; }

  void hookClick(bool left);

 private:
  void note(bool left, ULONGLONG time);
  static bool valid(int key) noexcept { return key > 0 && key < 256; }

  std::array<bool, 256> now_{};
  std::array<bool, 256> before_{};
  std::deque<ULONGLONG> left_;
  std::deque<ULONGLONG> right_;
  unsigned long long total_ = 0;
  bool focused_ = false;
  bool gameplay_ = false;
  ULONGLONG lastLeft_ = 0;
  std::mutex clicks_;
};

// Hooks de user32 que evitan que el juego recapture el raton con el menu abierto.
namespace mouse_guard {
bool install();
void setMenuOpen(bool open);
bool menuOpen();
// Minecraft tambien lee el raton por GameInput: hay que congelarlo ahi.
void startGameInputGuard();
}  // namespace mouse_guard

}  // namespace lex::platform::windows
