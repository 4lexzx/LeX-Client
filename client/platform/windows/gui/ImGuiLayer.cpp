#include "platform/windows/gui/ImGuiLayer.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>
#include <string>

#include "core/module/ModuleManager.h"
#include "core/util/Logger.h"
#include "platform/windows/gui/ClickGuiRenderer.h"
#include "platform/windows/game/Cosmetics.h"
#include "platform/windows/game/LiveTextures.h"
#include "platform/windows/gui/Preview.h"
#include "platform/windows/gui/IconAtlas.h"
#include "platform/windows/gui/TextureStore.h"
#include "platform/windows/hud/HudModule.h"
#include "platform/windows/input/Input.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace lex::platform::windows::gui {

using core::util::Logger;

ImGuiLayer* ImGuiLayer::active_ = nullptr;
std::atomic<ID3D12CommandQueue*> ImGuiLayer::gameQueue_{nullptr};

namespace {

// Reparto del heap: 0 fuente, 1.. iconos y despues imagenes.
constexpr UINT kHeapSize = 256;
constexpr UINT kFirstImage = 1 + IconAtlas::kIconCount;

template <typename T>
void release(T*& pointer) {
  if (pointer != nullptr) pointer->Release();
  pointer = nullptr;
}

}  // namespace

ImGuiLayer& ImGuiLayer::instance() {
  // Vida de proceso: evita destructores COM durante DLL_PROCESS_DETACH.
  static ImGuiLayer* layer = new ImGuiLayer();
  return *layer;
}

void ImGuiLayer::setGameQueue(ID3D12CommandQueue* queue) {
  if (queue == nullptr || gameQueue_.load() != nullptr) return;
  if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return;
  queue->AddRef();
  ID3D12CommandQueue* expected = nullptr;
  if (!gameQueue_.compare_exchange_strong(expected, queue)) queue->Release();
}

void ImGuiLayer::initContext() {
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  ImFontConfig config;
  config.OversampleH = 2;
  const auto load = [&](const char* file) -> ImFont* {
    char path[MAX_PATH];
    GetWindowsDirectoryA(path, MAX_PATH);
    const std::string full = std::string(path) + "\\Fonts\\" + file;
    if (GetFileAttributesA(full.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
    return io.Fonts->AddFontFromFileTTF(full.c_str(), 20.0f, &config, io.Fonts->GetGlyphRangesDefault());
  };
  hud::fonts().regular = load("segoeui.ttf");
  hud::fonts().bold = load("bahnschrift.ttf");
  if (hud::fonts().regular == nullptr) hud::fonts().regular = io.Fonts->AddFontDefault();
  if (hud::fonts().bold == nullptr) hud::fonts().bold = hud::fonts().regular;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScrollbarRounding = 8;
  style.ChildBorderSize = 0;
  style.PopupRounding = 8;
  style.ItemSpacing = ImVec2(8, 8);
}

bool ImGuiLayer::initialize(IDXGISwapChain* swapChain) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swapChain->GetDesc(&desc))) return false;
  // El juego puede crear swapchains auxiliares; solo interesa la de su ventana.
  if (desc.OutputWindow == nullptr || !IsWindowVisible(desc.OutputWindow)) return false;

  bool ok = false;
  ID3D12Device* device12 = nullptr;
  ID3D11Device* device11 = nullptr;
  if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&device12)))) {
    if (gameQueue_.load() != nullptr) {
      window_ = desc.OutputWindow;
      ok = initializeDx12(swapChain, device12);
    }
    device12->Release();
  } else if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&device11)))) {
    window_ = desc.OutputWindow;
    ok = initializeDx11(device11);
    device11->Release();
  }
  if (!ok) return false;

  target_ = swapChain;
  active_ = this;
  originalWndProc_ = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));
  Logger::instance().info(backend_ == Backend::Dx12 ? "Overlay activo (D3D12)" : "Overlay activo (D3D11)");
  return true;
}

bool ImGuiLayer::initializeDx11(ID3D11Device* device) {
  initContext();
  device->AddRef();
  d3d11Device_ = device;
  device->GetImmediateContext(&d3d11Context_);
  ImGui_ImplWin32_Init(window_);
  ImGui_ImplDX11_Init(device, d3d11Context_);
  IconAtlas::instance().initializeDX11(device);
  backend_ = Backend::Dx11;
  return true;
}

bool ImGuiLayer::initializeDx12(IDXGISwapChain* swapChain, ID3D12Device* device) {
  DXGI_SWAP_CHAIN_DESC desc{};
  swapChain->GetDesc(&desc);
  d3d12Queue_ = gameQueue_.load();

  D3D12_DESCRIPTOR_HEAP_DESC srv{};
  srv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srv.NumDescriptors = kHeapSize;
  srv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  D3D12_DESCRIPTOR_HEAP_DESC rtv{};
  rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtv.NumDescriptors = DXGI_MAX_SWAP_CHAIN_BUFFERS;

  const UINT count = std::max<UINT>(2, desc.BufferCount);
  frames_.resize(count);
  bool ok = SUCCEEDED(device->CreateDescriptorHeap(&srv, IID_PPV_ARGS(&srvHeap_))) &&
            SUCCEEDED(device->CreateDescriptorHeap(&rtv, IID_PPV_ARGS(&rtvHeap_))) &&
            SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
  for (Frame& frame : frames_) {
    ok = ok && SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)));
  }
  ok = ok && SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].allocator, nullptr, IID_PPV_ARGS(&commandList_)));
  if (!ok) {
    Logger::instance().error("No se pudieron crear recursos D3D12");
    releaseDx12();
    return false;
  }
  commandList_->Close();
  fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  device->AddRef();
  d3d12Device_ = device;

  initContext();
  ImGui_ImplWin32_Init(window_);
  ImGui_ImplDX12_Init(device, static_cast<int>(count), desc.BufferDesc.Format, srvHeap_,
                      srvHeap_->GetCPUDescriptorHandleForHeapStart(), srvHeap_->GetGPUDescriptorHandleForHeapStart());
  IconAtlas::instance().initializeDX12(device, srvHeap_, 1, d3d12Queue_);
  backend_ = Backend::Dx12;
  return true;
}

void ImGuiLayer::onPresent(IDXGISwapChain* swapChain) {
  if (model_ == nullptr || platform_ == nullptr) return;
  if (backend_ == Backend::None && !initialize(swapChain)) return;
  if (swapChain != target_) return;

  const auto now = std::chrono::steady_clock::now();
  float delta = 1.0f / 60.0f;
  if (lastFrame_.time_since_epoch().count() != 0) {
    delta = std::clamp(std::chrono::duration<float>(now - lastFrame_).count(), 0.0001f, 1.0f);
  }
  lastFrame_ = now;

  Input::instance().sample(window_);
  if (onFrame_) onFrame_(delta);
  model_->update(delta);
  handleInput();
  drawFrame();
  if (backend_ == Backend::Dx11) renderDx11(swapChain);
  if (backend_ == Backend::Dx12) renderDx12(swapChain);
}

void ImGuiLayer::handleInput() {
  const Input& input = Input::instance();
  const bool wasOpen = model_->isOpen();
  ClickGuiRenderer& gui = ClickGuiRenderer::instance();

  // Pausa: se llego con Esc desde el juego y aun no se ha pulsado nada en ella.
  // Asi la L nunca se confunde con una letra del chat o de un cuadro de texto.
  const ULONGLONG clock = GetTickCount64();
  if (input.pressed(VK_ESCAPE)) escapeAt_ = clock;
  if (!wasOpen && closingKey_ == 0) {
    if (input.gameplay()) paused_ = false;
    else if (wasGameplay_) paused_ = clock - escapeAt_ < 700;
    else if (input.pressed(VK_LBUTTON) || (input.pressed(VK_ESCAPE) && paused_)) paused_ = false;
    wasGameplay_ = input.gameplay();
  }

  if (gui.capturingKey()) {
    for (int key = 8; key < 256; ++key) {
      // Se ignoran los codigos genericos: llegan ya como izquierda/derecha.
      if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) continue;
      if (input.pressed(key)) {
        gui.captureKey(key);
        break;
      }
    }
  } else if (input.pressed(model_->theme().menuKey.key) &&
             (model_->isOpen() ? !ImGui::GetIO().WantTextInput : input.gameplay() || (paused_ && input.focused()))) {
    // Con el chat o un cuadro de texto abierto la tecla es solo una letra.
    gui.setHudEditing(false);
    model_->toggle();
  } else if (model_->isOpen() && input.pressed(VK_ESCAPE)) {
    if (gui.hudEditing()) gui.setHudEditing(false);
    else if (model_->settingsOpen()) model_->closeSettings();
    else model_->toggle();
  } else if (!model_->isOpen() && input.gameplay()) {
    for (const auto& module : core::module::ModuleManager::instance().modules()) {
      const auto& bind = module->keybind();
      if (bind.key != 0 && input.pressed(bind.key)) module->setEnabled(!module->isEnabled());
    }
  }

  const bool open = model_->isOpen();
  // Al cerrar, el juego no debe ver la tecla hasta que se suelte: si no, Esc
  // cierra LeX y ademas abre la pausa de Minecraft.
  if (wasOpen && !open) {
    closingKey_ = input.down(VK_ESCAPE) ? VK_ESCAPE : model_->theme().menuKey.key;
  }
  if (closingKey_ != 0) {
    const ULONGLONG time = GetTickCount64();
    if (input.down(closingKey_)) guardUntil_ = time + 120;
    else if (time >= guardUntil_) closingKey_ = 0;
  }
  mouse_guard::setMenuOpen(open || closingKey_ != 0);
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDrawCursor = open;
  // El juego puede usar entrada cruda sin mensajes heredados: se alimenta aqui.
  if (open && input.focused()) {
    POINT p;
    if (GetCursorPos(&p) && ScreenToClient(window_, &p)) {
      io.AddMousePosEvent(static_cast<float>(p.x), static_cast<float>(p.y));
    }
  }
  io.AddMouseButtonEvent(0, open && input.down(VK_LBUTTON));
  io.AddMouseButtonEvent(1, open && input.down(VK_RBUTTON));
}

void ImGuiLayer::drawFrame() {
  if (backend_ == Backend::Dx11) ImGui_ImplDX11_NewFrame();
  if (backend_ == Backend::Dx12) ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGuiIO& io = ImGui::GetIO();
  const float scale = std::clamp(io.DisplaySize.y / 860.0f, 0.88f, 2.5f);
  io.FontGlobalScale = 16.0f * scale / hud::fonts().regular->FontSize;
  ImGui::NewFrame();
  ClickGuiRenderer& gui = ClickGuiRenderer::instance();
  Preview::instance().update();
  hud::drawHud(gui.hudEditing(), scale);
  gui.draw(*model_, *platform_, scale);
  ImGui::Render();
}

void ImGuiLayer::renderDx11(IDXGISwapChain* swapChain) {
  TextureStore::instance().flushDx11(d3d11Device_);
  ID3D11Texture2D* buffer = nullptr;
  ID3D11RenderTargetView* view = nullptr;
  if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return;
  if (SUCCEEDED(d3d11Device_->CreateRenderTargetView(buffer, nullptr, &view))) {
    ID3D11RenderTargetView* previous = nullptr;
    ID3D11DepthStencilView* depth = nullptr;
    d3d11Context_->OMGetRenderTargets(1, &previous, &depth);
    d3d11Context_->OMSetRenderTargets(1, &view, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    d3d11Context_->OMSetRenderTargets(1, &previous, depth);
    release(previous);
    release(depth);
  }
  release(view);
  release(buffer);
}

void ImGuiLayer::renderDx12(IDXGISwapChain* swapChain) {
  IDXGISwapChain3* chain = nullptr;
  if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&chain)))) return;
  const UINT index = chain->GetCurrentBackBufferIndex();
  release(chain);
  ID3D12Resource* buffer = nullptr;
  if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&buffer)))) return;

  Frame& frame = frames_[index % frames_.size()];
  if (frame.fence != 0 && fence_->GetCompletedValue() < frame.fence) {
    fence_->SetEventOnCompletion(frame.fence, fenceEvent_);
    WaitForSingleObject(fenceEvent_, 1000);
  }
  if (FAILED(frame.allocator->Reset()) || FAILED(commandList_->Reset(frame.allocator, nullptr))) {
    release(buffer);
    return;
  }

  TextureStore::instance().flushDx12(d3d12Device_, commandList_, srvHeap_, kFirstImage, kHeapSize - kFirstImage);
  game::Cosmetics::instance().record(d3d12Device_, commandList_, index % frames_.size());
  game::LiveTextures::instance().record(d3d12Device_, commandList_, ++frameNumber_);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += static_cast<SIZE_T>(index) * d3d12Device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  d3d12Device_->CreateRenderTargetView(buffer, nullptr, rtv);

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = buffer;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commandList_->ResourceBarrier(1, &barrier);
  commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  commandList_->SetDescriptorHeaps(1, &srvHeap_);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_);
  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  commandList_->ResourceBarrier(1, &barrier);
  commandList_->Close();

  ID3D12CommandList* lists[] = {commandList_};
  d3d12Queue_->ExecuteCommandLists(1, lists);
  frame.fence = ++fenceValue_;
  d3d12Queue_->Signal(fence_, fenceValue_);
  release(buffer);
}

void ImGuiLayer::waitGpu() {
  if (backend_ != Backend::Dx12) return;
  d3d12Queue_->Signal(fence_, ++fenceValue_);
  if (fence_->GetCompletedValue() < fenceValue_) {
    fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
    WaitForSingleObject(fenceEvent_, 2000);
  }
}

void ImGuiLayer::onResizeBuffers(IDXGISwapChain* swapChain, std::uint32_t, std::uint32_t,
                                 std::uint32_t, std::uint32_t, std::uint32_t) {
  // No se guardan referencias al backbuffer; basta con vaciar la cola.
  if (swapChain != target_) return;
  waitGpu();
}

LRESULT CALLBACK ImGuiLayer::wndProc(HWND window, UINT message, WPARAM w, LPARAM l) {
  ImGuiLayer* self = active_;
  if (self == nullptr) return DefWindowProcW(window, message, w, l);
  const bool keyMessage = message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
  int key = keyMessage ? static_cast<int>(w) : 0;
  if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) {
    // Los modificadores llegan genericos: se distingue izquierda de derecha.
    const UINT scan = (static_cast<UINT>(l) >> 16) & 0xFF;
    const UINT extended = (l & (1 << 24)) ? 0xE000 : 0;
    key = static_cast<int>(MapVirtualKeyW(scan | extended, MAPVK_VSC_TO_VK_EX));
  }
  const bool ownKey = key != 0 && self->model_ != nullptr && key == self->model_->theme().menuKey.key &&
                      (mouse_guard::menuOpen() || Input::instance().gameplay() || self->paused_);
  if (mouse_guard::menuOpen()) {
    ImGui_ImplWin32_WndProcHandler(window, message, w, l);
    const bool mouse = (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || (message >= 0x0241 && message <= 0x0257);  // y WM_POINTER*
    const bool typing = message == WM_KEYDOWN || message == WM_CHAR;
    if (mouse || typing || (keyMessage && key == VK_ESCAPE)) return 0;
    if (message == WM_INPUT) return DefWindowProcW(window, message, w, l);
    if (message == WM_SETCURSOR && LOWORD(l) == HTCLIENT) {
      SetCursor(nullptr);  // ImGui dibuja el suyo
      return TRUE;
    }
    // WM_KEYUP pasa siempre: evita teclas atascadas al abrir el menu.
  }
  if (ownKey) return 0;  // la tecla del menu nunca llega al juego
  return CallWindowProcW(self->originalWndProc_, window, message, w, l);
}

void ImGuiLayer::releaseDx12() {
  game::LiveTextures::instance().releaseGpu();
  game::Cosmetics::instance().releaseGpu();
  for (Frame& frame : frames_) release(frame.allocator);
  frames_.clear();
  release(commandList_);
  release(fence_);
  release(srvHeap_);
  release(rtvHeap_);
  release(d3d12Device_);
  if (fenceEvent_ != nullptr) CloseHandle(fenceEvent_);
  fenceEvent_ = nullptr;
}

void ImGuiLayer::shutdown() {
  if (backend_ == Backend::None) return;
  mouse_guard::setMenuOpen(false);
  waitGpu();
  SetWindowLongPtrW(window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
  active_ = nullptr;
  IconAtlas::instance().shutdown();
  TextureStore::instance().shutdown();
  if (backend_ == Backend::Dx11) ImGui_ImplDX11_Shutdown();
  if (backend_ == Backend::Dx12) ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  hud::fonts() = {};
  release(d3d11Context_);
  release(d3d11Device_);
  releaseDx12();
  backend_ = Backend::None;
  target_ = nullptr;
}

}  // namespace lex::platform::windows::gui
