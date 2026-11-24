#include "platform/windows/gui/ImGuiLayer.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include "core/util/Logger.h"
#include "platform/windows/gui/ClickGuiRenderer.h"
#include "platform/windows/gui/IconAtlas.h"

using lex::core::util::Logger;
#include "platform/windows/gui/SettingEditor.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg,
                                                      WPARAM wParam,
                                                      LPARAM lParam);

namespace lex::platform::windows::gui {

ImGuiLayer* ImGuiLayer::instancePointer_ = nullptr;

ImGuiLayer& ImGuiLayer::instance() {
  static ImGuiLayer layer;
  return layer;
}

ImGuiLayer::~ImGuiLayer() { shutdown(); }

void ImGuiLayer::onPresent(IDXGISwapChain* swapChain) {
  if (swapChain == nullptr) return;

  const auto now = std::chrono::steady_clock::now();
  float deltaSeconds = 1.0f / 60.0f;
  if (lastFrame_.time_since_epoch().count() != 0) {
    const std::chrono::duration<float> elapsed = now - lastFrame_;
    if (elapsed.count() > 0.0f && elapsed.count() < 1.0f) {
      deltaSeconds = elapsed.count();
    }
  }
  lastFrame_ = now;
  instancePointer_ = this;

  if (!initialized_) {
    ID3D12Device* d3d12Device = nullptr;
    if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&d3d12Device))) &&
        d3d12Device != nullptr) {
      d3d12Device->Release();
      initialized_ = initializeDx12(swapChain);
      backend_ = initialized_ ? Backend::Dx12 : Backend::None;
    } else {
      initialized_ = initializeDx11(swapChain);
      backend_ = initialized_ ? Backend::Dx11 : Backend::None;
    }
    if (!initialized_) return;
  }

  if (resizePending_) {
    if (backend_ == Backend::Dx11) ImGui_ImplDX11_CreateDeviceObjects();
    resizePending_ = false;
  }

  drawFrame(deltaSeconds);
  if (backend_ == Backend::Dx11) {
    renderDx11(swapChain);
  } else if (backend_ == Backend::Dx12) {
    renderDx12(swapChain);
  }
}

bool ImGuiLayer::initializeDx11(IDXGISwapChain* swapChain) {
  ID3D11Device* device = nullptr;
  if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))) || device == nullptr) {
    return false;
  }

  ID3D11DeviceContext* context = nullptr;
  device->GetImmediateContext(&context);

  DXGI_SWAP_CHAIN_DESC description{};
  if (FAILED(swapChain->GetDesc(&description)) || description.OutputWindow == nullptr) {
    if (context) context->Release();
    device->Release();
    return false;
  }

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  window_ = description.OutputWindow;
  dxgiFormat_ = description.BufferDesc.Format;

  if (!ImGui_ImplWin32_Init(window_)) {
    if (context) context->Release();
    device->Release();
    return false;
  }
  if (!ImGui_ImplDX11_Init(device, context)) {
    ImGui_ImplWin32_Shutdown();
    if (context) context->Release();
    device->Release();
    return false;
  }

  d3d11Device_ = device;
  d3d11Context_ = context;
  IconAtlas::instance().initializeDX11(device);
  originalWndProc_ = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));
  wndProcHooked_ = originalWndProc_ != nullptr;

  Logger::instance().info("ImGui inicializado con backend D3D11");
  return true;
}

bool ImGuiLayer::initializeDx12(IDXGISwapChain* swapChain) {
  ID3D12Device* device = nullptr;
  if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))) || device == nullptr) {
    return false;
  }

  D3D12_COMMAND_QUEUE_DESC queueDescription{};
  queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  if (FAILED(device->CreateCommandQueue(&queueDescription,
                                        IID_PPV_ARGS(&d3d12Queue_)))) {
    device->Release();
    return false;
  }

  D3D12_DESCRIPTOR_HEAP_DESC srvDescription{};
  srvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  // 0 = font atlas de ImGui; el resto = iconos embebidos.
  srvDescription.NumDescriptors = 1 + IconAtlas::kIconCount;
  srvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(&srvDescription,
                                          IID_PPV_ARGS(&d3d12SrvHeap_)))) {
    d3d12Queue_->Release();
    d3d12Queue_ = nullptr;
    device->Release();
    return false;
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtvDescription{};
  rtvDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescription.NumDescriptors = 8;
  rtvDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (FAILED(device->CreateDescriptorHeap(&rtvDescription,
                                          IID_PPV_ARGS(&d3d12RtvHeap_)))) {
    d3d12SrvHeap_->Release();
    d3d12SrvHeap_ = nullptr;
    d3d12Queue_->Release();
    d3d12Queue_ = nullptr;
    device->Release();
    return false;
  }

  if (FAILED(device->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12Allocator_))) ||
      FAILED(device->CreateCommandList(
          0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12Allocator_, nullptr,
          IID_PPV_ARGS(&d3d12CommandList_))) ||
      FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                 IID_PPV_ARGS(&d3d12Fence_)))) {
    Logger::instance().error("No se pudieron crear recursos D3D12 para ImGui");
    return false;
  }
  d3d12FenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  DXGI_SWAP_CHAIN_DESC description{};
  if (FAILED(swapChain->GetDesc(&description)) || description.OutputWindow == nullptr) {
    return false;
  }
  window_ = description.OutputWindow;
  dxgiFormat_ = description.BufferDesc.Format;

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  if (!ImGui_ImplWin32_Init(window_)) return false;
  ImGui_ImplDX12_Init(device, 2, dxgiFormat_, d3d12SrvHeap_,
                      d3d12SrvHeap_->GetCPUDescriptorHandleForHeapStart(),
                      d3d12SrvHeap_->GetGPUDescriptorHandleForHeapStart());

  d3d12Device_ = device;
  IconAtlas::instance().initializeDX12(device, d3d12SrvHeap_, 1, d3d12Queue_);
  originalWndProc_ = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));
  wndProcHooked_ = originalWndProc_ != nullptr;

  Logger::instance().info("ImGui inicializado con backend D3D12");
  return true;
}

void ImGuiLayer::pollInput() {
  if (model_ == nullptr) return;

  core::settings::KeybindSetting* armed =
      SettingEditor::instance().armedKeybind();
  for (int key = 1; key < 256; ++key) {
    const bool pressed = (GetAsyncKeyState(key) & 0x8000) != 0;
    if (armed != nullptr && pressed && !previousKeys_[key] &&
        key != VK_LBUTTON && key != VK_RBUTTON) {
      core::settings::Keybind captured;
      captured.key = key;
      captured.control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
      captured.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
      captured.alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
      armed->setValue(captured);
      SettingEditor::instance().armKeybind(nullptr);
    }
    previousKeys_[key] = pressed;
  }

  const core::settings::Keybind& menuKey = model_->theme().menuKey;
  const bool togglePressed =
      menuKey.key != 0 && (GetAsyncKeyState(menuKey.key) & 0x8000) != 0;
  if (togglePressed && !lastTogglePressed_) model_->toggle();
  lastTogglePressed_ = togglePressed;

  const bool cursorVisible = model_->isOpen();
  ImGui::GetIO().MouseDrawCursor = cursorVisible;
  if (cursorVisible != lastCursorVisible_) {
    SetCursor(cursorVisible ? LoadCursor(nullptr, IDC_ARROW) : nullptr);
    lastCursorVisible_ = cursorVisible;
  }
}

void ImGuiLayer::drawFrame(float deltaSeconds) {
  if (backend_ == Backend::Dx11) {
    ImGui_ImplDX11_NewFrame();
  } else if (backend_ == Backend::Dx12) {
    ImGui_ImplDX12_NewFrame();
  }
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  if (model_ != nullptr) {
    model_->update(deltaSeconds);
    pollInput();
    if (platform_ != nullptr) {
      ClickGuiRenderer::instance().draw(*model_, *platform_);
    }
  }
  ImGui::Render();
}

void ImGuiLayer::renderDx11(IDXGISwapChain* swapChain) {
  if (d3d11Device_ == nullptr || d3d11Context_ == nullptr) return;

  ID3D11RenderTargetView* previousTarget = nullptr;
  ID3D11DepthStencilView* previousDepth = nullptr;
  d3d11Context_->OMGetRenderTargets(1, &previousTarget, &previousDepth);

  ID3D11Texture2D* backBuffer = nullptr;
  ID3D11RenderTargetView* renderTarget = nullptr;
  if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
    if (SUCCEEDED(d3d11Device_->CreateRenderTargetView(backBuffer, nullptr,
                                                       &renderTarget))) {
      d3d11Context_->OMSetRenderTargets(1, &renderTarget, nullptr);
      ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
      d3d11Context_->OMSetRenderTargets(1, &previousTarget, previousDepth);
    }
  }

  if (renderTarget != nullptr) renderTarget->Release();
  if (backBuffer != nullptr) backBuffer->Release();
  if (previousTarget != nullptr) previousTarget->Release();
  if (previousDepth != nullptr) previousDepth->Release();
}

void ImGuiLayer::renderDx12(IDXGISwapChain* swapChain) {
  if (d3d12Device_ == nullptr || d3d12CommandList_ == nullptr ||
      d3d12Queue_ == nullptr) {
    return;
  }

  IDXGISwapChain3* swapChain3 = nullptr;
  UINT backBufferIndex = 0;
  if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))) &&
      swapChain3 != nullptr) {
    backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
    swapChain3->Release();
  }

  ID3D12Resource* backBuffer = nullptr;
  if (FAILED(swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer)))) {
    return;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE renderTarget =
      d3d12RtvHeap_->GetCPUDescriptorHandleForHeapStart();
  renderTarget.ptr += static_cast<SIZE_T>(backBufferIndex) *
                      d3d12Device_->GetDescriptorHandleIncrementSize(
                          D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  D3D12_RENDER_TARGET_VIEW_DESC renderTargetDescription{};
  renderTargetDescription.Format = dxgiFormat_;
  renderTargetDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  d3d12Device_->CreateRenderTargetView(backBuffer, &renderTargetDescription,
                                       renderTarget);
  backBuffer->Release();

  if (FAILED(d3d12Allocator_->Reset()) ||
      FAILED(d3d12CommandList_->Reset(d3d12Allocator_, nullptr))) {
    return;
  }
  d3d12CommandList_->SetDescriptorHeaps(1, &d3d12SrvHeap_);
  d3d12CommandList_->OMSetRenderTargets(1, &renderTarget, FALSE, nullptr);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12CommandList_);
  d3d12CommandList_->Close();

  ID3D12CommandList* commands[] = {d3d12CommandList_};
  d3d12Queue_->ExecuteCommandLists(1, commands);
  ++d3d12FenceValue_;
  d3d12Queue_->Signal(d3d12Fence_, d3d12FenceValue_);
  if (d3d12Fence_->GetCompletedValue() < d3d12FenceValue_) {
    d3d12Fence_->SetEventOnCompletion(d3d12FenceValue_, d3d12FenceEvent_);
    WaitForSingleObject(d3d12FenceEvent_, INFINITE);
  }
}

void ImGuiLayer::onResizeBuffers(IDXGISwapChain* swapChain,
                                 std::uint32_t bufferCount, std::uint32_t width,
                                 std::uint32_t height, std::uint32_t format,
                                 std::uint32_t flags) {
  (void)swapChain;
  (void)bufferCount;
  (void)width;
  (void)height;
  (void)format;
  (void)flags;
  if (!initialized_) return;
  resizePending_ = true;
  if (backend_ == Backend::Dx11) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
}

LRESULT CALLBACK ImGuiLayer::wndProc(HWND window, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
    return true;
  }
  ImGuiLayer* layer = instancePointer_;
  if (layer != nullptr && layer->originalWndProc_ != nullptr) {
    return CallWindowProcW(layer->originalWndProc_, window, message, wParam,
                           lParam);
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

void ImGuiLayer::shutdown() {
  IconAtlas::instance().shutdown();
  if (!initialized_ && backend_ == Backend::None) return;

  if (wndProcHooked_ && window_ != nullptr && originalWndProc_ != nullptr) {
    SetWindowLongPtrW(window_, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(originalWndProc_));
    wndProcHooked_ = false;
  }

  if (backend_ == Backend::Dx11) {
    ImGui_ImplDX11_Shutdown();
  } else if (backend_ == Backend::Dx12) {
    ImGui_ImplDX12_Shutdown();
  }
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (d3d11Context_ != nullptr) {
    d3d11Context_->Release();
    d3d11Context_ = nullptr;
  }
  if (d3d11Device_ != nullptr) {
    d3d11Device_->Release();
    d3d11Device_ = nullptr;
  }

  if (d3d12FenceEvent_ != nullptr) {
    CloseHandle(d3d12FenceEvent_);
    d3d12FenceEvent_ = nullptr;
  }
  if (d3d12Fence_ != nullptr) {
    d3d12Fence_->Release();
    d3d12Fence_ = nullptr;
  }
  if (d3d12CommandList_ != nullptr) {
    d3d12CommandList_->Release();
    d3d12CommandList_ = nullptr;
  }
  if (d3d12Allocator_ != nullptr) {
    d3d12Allocator_->Release();
    d3d12Allocator_ = nullptr;
  }
  if (d3d12RtvHeap_ != nullptr) {
    d3d12RtvHeap_->Release();
    d3d12RtvHeap_ = nullptr;
  }
  if (d3d12SrvHeap_ != nullptr) {
    d3d12SrvHeap_->Release();
    d3d12SrvHeap_ = nullptr;
  }
  if (d3d12Queue_ != nullptr) {
    d3d12Queue_->Release();
    d3d12Queue_ = nullptr;
  }
  if (d3d12Device_ != nullptr) {
    d3d12Device_->Release();
    d3d12Device_ = nullptr;
  }

  backend_ = Backend::None;
  initialized_ = false;
  window_ = nullptr;
  originalWndProc_ = nullptr;
  model_ = nullptr;
  platform_ = nullptr;
  if (instancePointer_ == this) instancePointer_ = nullptr;
}

}  // namespace lex::platform::windows::gui
