#pragma once

#include <cstddef>

namespace lex::platform::windows::hooks {

// Sustituye una entrada de una vtable COM protegiendo la pagina con
// VirtualProtect. No parchea codigo: solo cambia un puntero de funcion.
class VtableHook {
 public:
  VtableHook() = default;
  VtableHook(const VtableHook&) = delete;
  VtableHook& operator=(const VtableHook&) = delete;
  VtableHook(VtableHook&& other) noexcept;
  VtableHook& operator=(VtableHook&& other) noexcept;
  ~VtableHook();

  bool hook(void** vtable, std::size_t index, void* detour, void** original);
  void restore();
  bool isHooked() const noexcept { return hooked_; }

 private:
  void** table_ = nullptr;
  std::size_t index_ = 0;
  void* original_ = nullptr;
  unsigned long oldProtection_ = 0;
  bool hooked_ = false;
};

}  // namespace lex::platform::windows::hooks
