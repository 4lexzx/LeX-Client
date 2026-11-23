// LEX CLIENT — Hooks (low-level). Click/teclado passthrough sin bloquear eventos.
#pragma once
#include <Windows.h>

namespace lex {
namespace engine {

struct HookHandles {
  HHOOK mouse = nullptr;
  HHOOK keyboard = nullptr;
};

HookHandles installHooks();
void uninstallHooks(const HookHandles& handles);

}  // namespace engine
}  // namespace lex
