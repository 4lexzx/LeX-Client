#pragma once

#include <chrono>
#include <cstdint>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_5.h>

#include "core/gui/ClickGuiModel.h"
#include "core/platform/IPlatform.h"
#include "platform/windows/hooks/HookSink.h"

namespace lex::platform::windows::gui {

// Puente entre el hook DXGI y el ClickGUI. Detecta automaticamente si el
// swapchain pertenece a D3D11 o D3D12 y usa el backend de ImGui correcto.
class ImGuiLayer final : public hooks::HookSink {
 public:
  enum class Backend { None, Dx11, Dx12 };

  static ImGuiLayer& instance();

  void setModel(core::gui::ClickGuiModel* model) noexcept { model_ = model; }
  void setPlatform(core::IPlatform* platform) noexcept { platform_ = platform; }

  void onPresent(IDXGISwapChain* swapChain) override;
  void onResizeBuffers(IDXGISwapChain* swapChain, std::uint32_t bufferCount,
                       std::uint32_t width, std::uint32_t height,
                       std::uint32_t format, std::uint32_t flags) override;

  void shutdown();
  bool initialized() const noexcept { return initialized_; }
  Backend backend() const noexcept { return backend_; }

 private:
  ImGuiLayer() = default;
  ~ImGuiLayer() override;

  bool initializeDx11(IDXGISwapChain* swapChain);
  bool initializeDx12(IDXGISwapChain* swapChain);
  void pollInput();
  void drawFrame(float deltaSeconds);
  void renderDx11(IDXGISwapChain* swapChain);
  void renderDx12(IDXGISwapChain* swapChain);

  static LRESULT CALLBACK wndProc(HWND window, UINT message, WPARAM wParam,
                                  LPARAM lParam);

  static ImGuiLayer* instancePointer_;

  core::gui::ClickGuiModel* model_ = nullptr;
  core::IPlatform* platform_ = nullptr;
  Backend backend_ = Backend::None;
  bool initialized_ = false;
  bool resizePending_ = false;
  bool wndProcHooked_ = false;
  bool lastCursorVisible_ = false;
  bool lastTogglePressed_ = false;
  bool previousKeys_[256] = {};
  HWND window_ = nullptr;
  WNDPROC originalWndProc_ = nullptr;
  std::chrono::steady_clock::time_point lastFrame_;

  ID3D11Device* d3d11Device_ = nullptr;
  ID3D11DeviceContext* d3d11Context_ = nullptr;

  ID3D12Device* d3d12Device_ = nullptr;
  ID3D12CommandQueue* d3d12Queue_ = nullptr;
  ID3D12DescriptorHeap* d3d12SrvHeap_ = nullptr;
  ID3D12DescriptorHeap* d3d12RtvHeap_ = nullptr;
  ID3D12CommandAllocator* d3d12Allocator_ = nullptr;
  ID3D12GraphicsCommandList* d3d12CommandList_ = nullptr;
  ID3D12Fence* d3d12Fence_ = nullptr;
  HANDLE d3d12FenceEvent_ = nullptr;
  std::uint64_t d3d12FenceValue_ = 0;
  DXGI_FORMAT dxgiFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;
};

}  // namespace lex::platform::windows::gui
