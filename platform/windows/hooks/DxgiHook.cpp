#include "platform/windows/hooks/DxgiHook.h"

#include <algorithm>
#include <utility>

#include "core/util/Logger.h"

using lex::core::util::Logger;

namespace lex::platform::windows::hooks {

DxgiHook* DxgiHook::instancePointer_ = nullptr;

DxgiHook& DxgiHook::instance() {
  static DxgiHook hook;
  return hook;
}

DxgiHook::~DxgiHook() { shutdown(); }

bool DxgiHook::install(HookSink* sink) {
  if (installed_) return true;

  sink_ = sink;
  instancePointer_ = this;

  static constexpr wchar_t kWindowClass[] = L"LeXDxgiHookDummy";
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = kWindowClass;
  if (RegisterClassW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    Logger::instance().error("No se pudo registrar ventana dummy DXGI");
    instancePointer_ = nullptr;
    sink_ = nullptr;
    return false;
  }

  dummyWindow_ = CreateWindowW(kWindowClass, L"LeX DXGI", WS_OVERLAPPEDWINDOW,
                               0, 0, 64, 64, nullptr, nullptr,
                               windowClass.hInstance, nullptr);
  if (dummyWindow_ == nullptr) {
    Logger::instance().error("No se pudo crear ventana dummy DXGI");
    instancePointer_ = nullptr;
    sink_ = nullptr;
    return false;
  }

  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 2;
  desc.BufferDesc.Width = 64;
  desc.BufferDesc.Height = 64;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferDesc.RefreshRate.Numerator = 60;
  desc.BufferDesc.RefreshRate.Denominator = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = dummyWindow_;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_0};
  HRESULT result = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
      D3D11_SDK_VERSION, &desc, &dummySwapChain_, &dummyDevice_, nullptr,
      &dummyContext_);

  if (FAILED(result)) {
    result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
        &desc, &dummySwapChain_, &dummyDevice_, nullptr, &dummyContext_);
  }

  if (FAILED(result) || dummySwapChain_ == nullptr) {
    Logger::instance().error(
        "No se pudo crear dispositivo/swapchain dummy para el hook");
    shutdown();
    return false;
  }

  registerSwapChainInterfaces(dummySwapChain_);
  if (originals_.empty()) {
    Logger::instance().error("No se pudo enganchar ninguna vtable DXGI");
    shutdown();
    return false;
  }

  installed_ = true;
  Logger::instance().info(
      "Hook DXGI instalado (" + std::to_string(originals_.size()) +
      " vtables, Present=" + std::to_string(kPresentIndex) +
      ", ResizeBuffers=" + std::to_string(kResizeBuffersIndex) + ")");
  return true;
}

void DxgiHook::registerSwapChainInterfaces(IDXGISwapChain* swapChain) {
  registerVtable(*reinterpret_cast<void***>(swapChain));

  struct InterfaceProbe {
    REFIID id;
    const char* label;
  };

  const InterfaceProbe probes[] = {
      {IID_IDXGISwapChain1, "IDXGISwapChain1"},
      {IID_IDXGISwapChain2, "IDXGISwapChain2"},
      {IID_IDXGISwapChain3, "IDXGISwapChain3"},
      {IID_IDXGISwapChain4, "IDXGISwapChain4"},
  };

  for (const InterfaceProbe& probe : probes) {
    void* interfacePointer = nullptr;
    const HRESULT queryResult = swapChain->QueryInterface(probe.id, &interfacePointer);
    if (SUCCEEDED(queryResult) && interfacePointer != nullptr) {
      registerVtable(*reinterpret_cast<void***>(interfacePointer));
      static_cast<IUnknown*>(interfacePointer)->Release();
    }
    Logger::instance().info(std::string("DXGI QI ") + probe.label + ": " +
                            (SUCCEEDED(queryResult) ? "OK" : "NO"));
  }
}

void DxgiHook::registerVtable(void** vtable) {
  if (vtable == nullptr) return;
  if (originals_.find(vtable) != originals_.end()) return;

  void* originalPresent = nullptr;
  void* originalResize = nullptr;
  VtableHook presentHook;
  VtableHook resizeHook;

  const bool hookedPresent = presentHook.hook(
      vtable, kPresentIndex, reinterpret_cast<void*>(&presentDetour),
      &originalPresent);
  const bool hookedResize = resizeHook.hook(
      vtable, kResizeBuffersIndex, reinterpret_cast<void*>(&resizeDetour),
      &originalResize);

  // Si una falla, los destructores RAII restauran la otra.
  if (!hookedPresent || !hookedResize) return;

  hooks_.push_back(std::move(presentHook));
  hooks_.push_back(std::move(resizeHook));
  originals_.emplace(vtable, Originals{originalPresent, originalResize});
}

HRESULT STDMETHODCALLTYPE DxgiHook::presentDetour(IDXGISwapChain* self,
                                                 UINT syncInterval,
                                                 UINT flags) {
  DxgiHook* owner = instancePointer_;
  if (owner == nullptr || self == nullptr) return E_FAIL;

  owner->presentCalls_.fetch_add(1);

  void** vtable = *reinterpret_cast<void***>(self);
  auto found = owner->originals_.find(vtable);
  void* original = found != owner->originals_.end() ? found->second.present
                                                    : nullptr;

  if (owner->sink_ != nullptr) owner->sink_->onPresent(self);
  if (original == nullptr) return E_FAIL;

  using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
  return reinterpret_cast<PresentFunction>(original)(self, syncInterval, flags);
}

HRESULT STDMETHODCALLTYPE DxgiHook::resizeDetour(IDXGISwapChain* self,
                                                UINT bufferCount, UINT width,
                                                UINT height, DXGI_FORMAT format,
                                                UINT flags) {
  DxgiHook* owner = instancePointer_;
  if (owner == nullptr || self == nullptr) return E_FAIL;

  owner->resizeCalls_.fetch_add(1);

  void** vtable = *reinterpret_cast<void***>(self);
  auto found = owner->originals_.find(vtable);
  void* original = found != owner->originals_.end() ? found->second.resize
                                                    : nullptr;

  if (owner->sink_ != nullptr) {
    owner->sink_->onResizeBuffers(self, bufferCount, width, height,
                                  static_cast<std::uint32_t>(format), flags);
  }
  if (original == nullptr) return E_FAIL;

  using ResizeFunction = HRESULT(STDMETHODCALLTYPE*)(
      IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
  return reinterpret_cast<ResizeFunction>(original)(self, bufferCount, width,
                                                    height, format, flags);
}

void DxgiHook::shutdown() {
  hooks_.clear();
  originals_.clear();

  if (dummySwapChain_ != nullptr) {
    dummySwapChain_->Release();
    dummySwapChain_ = nullptr;
  }
  if (dummyContext_ != nullptr) {
    dummyContext_->Release();
    dummyContext_ = nullptr;
  }
  if (dummyDevice_ != nullptr) {
    dummyDevice_->Release();
    dummyDevice_ = nullptr;
  }
  if (dummyWindow_ != nullptr) {
    DestroyWindow(dummyWindow_);
    dummyWindow_ = nullptr;
  }
  UnregisterClassW(L"LeXDxgiHookDummy", GetModuleHandleW(nullptr));

  sink_ = nullptr;
  installed_ = false;
  if (instancePointer_ == this) instancePointer_ = nullptr;
}

}  // namespace lex::platform::windows::hooks
