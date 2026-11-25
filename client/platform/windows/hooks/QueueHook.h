#pragma once

namespace lex::platform::windows::hooks {
// Captura la cola D3D12 del juego. No hace nada si el juego usa D3D11.
bool installQueueHook();
}  // namespace lex::platform::windows::hooks
