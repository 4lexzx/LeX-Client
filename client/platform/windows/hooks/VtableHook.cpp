#include "platform/windows/hooks/VtableHook.h"

#include <windows.h>

namespace lex::platform::windows::hooks {

VtableHook::VtableHook(VtableHook&& other) noexcept
    : table_(other.table_),
      index_(other.index_),
      original_(other.original_),
      oldProtection_(other.oldProtection_),
      hooked_(other.hooked_) {
  other.hooked_ = false;
  other.table_ = nullptr;
  other.original_ = nullptr;
}

VtableHook& VtableHook::operator=(VtableHook&& other) noexcept {
  if (this != &other) {
    restore();
    table_ = other.table_;
    index_ = other.index_;
    original_ = other.original_;
    oldProtection_ = other.oldProtection_;
    hooked_ = other.hooked_;
    other.hooked_ = false;
    other.table_ = nullptr;
    other.original_ = nullptr;
  }
  return *this;
}

VtableHook::~VtableHook() { restore(); }

bool VtableHook::hook(void** vtable, std::size_t index, void* detour,
                      void** original) {
  if (hooked_ || vtable == nullptr || detour == nullptr) return false;

  void* target = vtable[index];
  if (target == nullptr || target == detour) return false;

  void* address = &vtable[index];
  unsigned long ignored = 0;
  if (!VirtualProtect(address, sizeof(void*), PAGE_READWRITE, &oldProtection_)) {
    return false;
  }

  vtable[index] = detour;
  VirtualProtect(address, sizeof(void*), oldProtection_, &ignored);

  table_ = vtable;
  index_ = index;
  original_ = target;
  if (original != nullptr) *original = target;
  hooked_ = true;
  return true;
}

void VtableHook::restore() {
  if (!hooked_) return;

  void* address = &table_[index_];
  unsigned long ignored = 0;
  if (VirtualProtect(address, sizeof(void*), PAGE_READWRITE, &ignored)) {
    table_[index_] = original_;
    VirtualProtect(address, sizeof(void*), ignored, &ignored);
  }
  hooked_ = false;
  table_ = nullptr;
  original_ = nullptr;
  index_ = 0;
}

}  // namespace lex::platform::windows::hooks
