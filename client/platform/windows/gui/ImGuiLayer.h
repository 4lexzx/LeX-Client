#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <imgui.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "core/gui/ClickGuiModel.h"
#include "core/platform/IPlatform.h"
#include "platform/windows/hooks/HookSink.h"

namespace lex::platform::windows::gui {

// Dibuja el HUD y el menu sobre el swapchain del juego (D3D11 o D3D12).
class ImGuiLayer final : public hooks::HookSink {
 public:
  enum class Backend { None, Dx11, Dx12 };

  static ImGuiLayer& instance();

  void setModel(core::gui::ClickGuiModel* model) noexcept { model_ = model; }
  void setPlatform(core::IPlatform* platform) noexcept { platform_ = platform; }
  // Se llama una vez por fotograma, antes de dibujar.
  void setFrameCallback(std::function<void(float)> callback) { onFrame_ = std::move(callback); }

  void onPresent(IDXGISwapChain* swapChain) override;
  void onResizeBuffers(IDXGISwapChain*, std::uint32_t, std::uint32_t, std::uint32_t,
                       std::uint32_t, std::uint32_t) override;

  // Cola directa con la que el juego envia trabajo a la GPU (D3D12).
  static void setGameQueue(ID3D12CommandQueue* queue);

  void shutdown();
  Backend backend() const noexcept { return backend_; }

 private:
  struct Frame {
    ID3D12CommandAllocator* allocator = nullptr;
    std::uint64_t fence = 0;
  };

  ImGuiLayer() = default;

  bool initialize(IDXGISwapChain* swapChain);
  void initContext();
  bool initializeDx11(ID3D11Device* device);
  bool initializeDx12(IDXGISwapChain* swapChain, ID3D12Device* device);
  void releaseDx12();
  void handleInput();
  void drawFrame();
  void renderDx11(IDXGISwapChain* swapChain);
  void renderDx12(IDXGISwapChain* swapChain);
  void waitGpu();

  static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

  static ImGuiLayer* active_;
  static std::atomic<ID3D12CommandQueue*> gameQueue_;
  static inline std::atomic<bool> chatOpen{false};  // lo lee wndProc (otro hilo)

  core::gui::ClickGuiModel* model_ = nullptr;
  core::IPlatform* platform_ = nullptr;
  std::function<void(float)> onFrame_;
  Backend backend_ = Backend::None;
  IDXGISwapChain* target_ = nullptr;  // identidad, sin referencia
  int closingKey_ = 0;            // tecla que cerro el menu y sigue bloqueada
  ULONGLONG escapeAt_ = 0;
  bool paused_ = false;
  // Chat del juego abierto (T, Enter o /): ahi la L y los atajos son letras.
  bool chatOpen_ = false;
  bool wasGameplay_ = false;
  ULONGLONG guardUntil_ = 0;
  HWND window_ = nullptr;
  WNDPROC originalWndProc_ = nullptr;
  std::chrono::steady_clock::time_point lastFrame_;

  ID3D11Device* d3d11Device_ = nullptr;
  ID3D11DeviceContext* d3d11Context_ = nullptr;

  ID3D12Device* d3d12Device_ = nullptr;
  ID3D12CommandQueue* d3d12Queue_ = nullptr;
  ID3D12DescriptorHeap* srvHeap_ = nullptr;
  ID3D12DescriptorHeap* rtvHeap_ = nullptr;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  ID3D12Fence* fence_ = nullptr;
  HANDLE fenceEvent_ = nullptr;
  std::uint64_t fenceValue_ = 0;
  std::vector<Frame> frames_;
  std::uint64_t frameNumber_ = 0;
};

}  // namespace lex::platform::windows::gui
