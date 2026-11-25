#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <iostream>

#include <imgui.h>

#include "core/gui/ClickGuiModel.h"
#include "core/module/ModuleManager.h"
#include "platform/windows/gui/ClickGuiRenderer.h"
#include "platform/windows/hooks/DxgiHook.h"
#include "tests/NullPlatform.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    ++g_checks;                                                                 \
    if (!(condition)) {                                                         \
      std::cerr << "FAIL linea " << __LINE__ << ": " #condition << '\n';       \
      ++g_failures;                                                             \
    }                                                                           \
  } while (false)

using lex::platform::windows::hooks::DxgiHook;
using lex::platform::windows::hooks::HookSink;

void testClickGuiRendering() {
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int fontWidth = 0;
  int fontHeight = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &fontWidth, &fontHeight);
  CHECK(fontWidth > 0 && fontHeight > 0);

  auto& manager = lex::core::module::ModuleManager::instance();
  lex::core::gui::ClickGuiModel model(manager);
  model.setOpen(true);
  model.update(1.0f);
  lex::tests::NullPlatform platform;

  ImGui::NewFrame();
  lex::platform::windows::gui::ClickGuiRenderer::instance().draw(model, platform);
  ImGui::Render();

  const ImDrawData* drawData = ImGui::GetDrawData();
  CHECK(drawData != nullptr);
  CHECK(drawData != nullptr && drawData->TotalIdxCount > 0);
  CHECK(drawData != nullptr && drawData->CmdListsCount > 0);

  ImGui::DestroyContext();
}

class CountingSink final : public HookSink {
 public:
  int presents = 0;
  int resizes = 0;

  void onPresent(IDXGISwapChain*) override { ++presents; }
  void onResizeBuffers(IDXGISwapChain*, std::uint32_t, std::uint32_t,
                       std::uint32_t, std::uint32_t, std::uint32_t) override {
    ++resizes;
  }
};

bool createSwapChain(IDXGISwapChain** swapChain, ID3D11Device** device,
                     ID3D11DeviceContext** context, HWND window) {
  DXGI_SWAP_CHAIN_DESC description{};
  description.BufferCount = 2;
  description.BufferDesc.Width = 64;
  description.BufferDesc.Height = 64;
  description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  description.OutputWindow = window;
  description.SampleDesc.Count = 1;
  description.Windowed = TRUE;
  description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
  return SUCCEEDED(D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1, D3D11_SDK_VERSION,
      &description, swapChain, device, nullptr, context));
}

}  // namespace

int main() {
  testClickGuiRendering();

  static constexpr wchar_t kWindowClass[] = L"LexHookTestWindow";
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = kWindowClass;
  CHECK(RegisterClassW(&windowClass) != 0);

  HWND window = CreateWindowW(kWindowClass, L"LeX hook test", WS_OVERLAPPEDWINDOW,
                              0, 0, 64, 64, nullptr, nullptr,
                              windowClass.hInstance, nullptr);
  CHECK(window != nullptr);
  if (window == nullptr) return 1;

  IDXGISwapChain* swapChainA = nullptr;
  IDXGISwapChain* swapChainB = nullptr;
  ID3D11Device* deviceA = nullptr;
  ID3D11Device* deviceB = nullptr;
  ID3D11DeviceContext* contextA = nullptr;
  ID3D11DeviceContext* contextB = nullptr;

  const bool createdA = createSwapChain(&swapChainA, &deviceA, &contextA, window);
  const bool createdB = createSwapChain(&swapChainB, &deviceB, &contextB, window);
  CHECK(createdA);
  CHECK(createdB);

  if (createdA && createdB) {
    DxgiHook& hook = DxgiHook::instance();
    CHECK(!hook.installed());
    CHECK(DxgiHook::kPresentIndex == 8);
    CHECK(DxgiHook::kResizeBuffersIndex == 13);

    CountingSink sink;
    CHECK(hook.install(&sink));
    CHECK(hook.installed());

    // Dos swaps chains distintos: si la vtable compartida se engancho bien,
    // ambas llamadas deben pasar por nuestro detour.
    swapChainA->Present(0, 0);
    swapChainB->Present(0, 0);
    CHECK(sink.presents == 2);
    CHECK(hook.presentCallCount() >= 2);

    swapChainB->ResizeBuffers(2, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    CHECK(sink.resizes == 1);

    hook.shutdown();
    CHECK(!hook.installed());

    const int presentsBefore = sink.presents;
    swapChainA->Present(0, 0);
    CHECK(sink.presents == presentsBefore);
  }

  if (contextA) contextA->Release();
  if (deviceA) deviceA->Release();
  if (swapChainA) swapChainA->Release();
  if (contextB) contextB->Release();
  if (deviceB) deviceB->Release();
  if (swapChainB) swapChainB->Release();

  DestroyWindow(window);
  UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));

  std::cout << "\nFase 2 hooks: " << g_checks << " comprobaciones, "
            << g_failures << " fallos.\n";
  return g_failures == 0 ? 0 : 1;
}
