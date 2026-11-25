#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace lex::platform::windows::game {

// Lectura que nunca falla con una excepcion aunque la pagina desaparezca.
bool safeRead(std::uintptr_t address, void* out, std::size_t bytes);
// Recorre la memoria privada escribible del proceso por trozos alineados.
void forEachChunk(const std::function<void(std::uintptr_t, const std::uint64_t*, std::size_t)>& visit);

// Opciones del juego en vivo. Se localizan por la forma del objeto
// {vtable, info, min, max, valor, defecto} y por su etiqueta de options.txt,
// asi que no hay offsets propios de una version.
class Options {
 public:
  static Options& instance();
  void start();
  bool ready() const noexcept { return ready_.load(); }
  bool has(const std::string& tag) const;

  // Los enteros y booleanos se leen y escriben tambien como float.
  bool get(const std::string& tag, float& out) const;
  bool set(const std::string& tag, float value);

 private:
  enum class Kind { Float, Bool, Int };
  struct Entry {
    std::uintptr_t address = 0;  // apunta al puntero "info"
    std::uintptr_t info = 0;
    Kind kind = Kind::Float;
  };
  Options() = default;
  void scan();

  mutable std::mutex mutex_;
  std::map<std::string, Entry> entries_;
  std::atomic<bool> ready_{false};
  std::atomic<bool> started_{false};
};

}  // namespace lex::platform::windows::game
