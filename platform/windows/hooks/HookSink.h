#pragma once

#include <cstdint>

struct IDXGISwapChain;

namespace lex::platform::windows::hooks {

// Puerta entre el hook DXGI (que conoce COM) y el render de ImGui.
class HookSink {
 public:
  virtual ~HookSink() = default;

  virtual void onPresent(IDXGISwapChain* swapChain) = 0;
  virtual void onResizeBuffers(IDXGISwapChain* swapChain,
                               std::uint32_t bufferCount, std::uint32_t width,
                               std::uint32_t height, std::uint32_t format,
                               std::uint32_t flags) = 0;
};

}  // namespace lex::platform::windows::hooks
