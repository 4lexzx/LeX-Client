#pragma once

#include <atomic>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <dxgi1_5.h>

#include "platform/windows/hooks/HookSink.h"
#include "platform/windows/hooks/VtableHook.h"

namespace lex::platform::windows::hooks {

// Engancha Present (slot 8) y ResizeBuffers (slot 13) de todas las vtables de
// IDXGISwapChain disponibles. Esas vtables son compartidas por todos los
// swapchains de dxgi.dll, por lo que intercepta tanto D3D11 como D3D12 sin
// necesidad de firmas ni offsets del juego.
class DxgiHook {
 public:
  static DxgiHook& instance();

  DxgiHook(const DxgiHook&) = delete;
  DxgiHook& operator=(const DxgiHook&) = delete;

  bool install(HookSink* sink);
  void shutdown();

  bool installed() const noexcept { return installed_; }
  int presentCallCount() const noexcept { return presentCalls_.load(); }
  int resizeCallCount() const noexcept { return resizeCalls_.load(); }

  // Verificado en dxgi.h: Present = 8, ResizeBuffers = 13.
  static constexpr std::size_t kPresentIndex = 8;
  static constexpr std::size_t kResizeBuffersIndex = 13;

 private:
  struct Originals {
    void* present = nullptr;
    void* resize = nullptr;
  };

  DxgiHook() = default;
  ~DxgiHook();

  void registerVtable(void** vtable);
  void registerSwapChainInterfaces(IDXGISwapChain* swapChain);

  static HRESULT STDMETHODCALLTYPE presentDetour(IDXGISwapChain* self,
                                                 UINT syncInterval, UINT flags);
  static HRESULT STDMETHODCALLTYPE resizeDetour(IDXGISwapChain* self,
                                                UINT bufferCount, UINT width,
                                                UINT height, DXGI_FORMAT format,
                                                UINT flags);

  static DxgiHook* instancePointer_;

  HookSink* sink_ = nullptr;
  bool installed_ = false;
  std::atomic<int> presentCalls_{0};
  std::atomic<int> resizeCalls_{0};

  std::vector<VtableHook> hooks_;
  std::unordered_map<void*, Originals> originals_;

  HWND dummyWindow_ = nullptr;
  ID3D11Device* dummyDevice_ = nullptr;
  ID3D11DeviceContext* dummyContext_ = nullptr;
  IDXGISwapChain* dummySwapChain_ = nullptr;
};

}  // namespace lex::platform::windows::hooks
