#include "platform/windows/hooks/QueueHook.h"

#include <windows.h>
#include <d3d12.h>
#include <MinHook.h>

#include "platform/windows/game/Telemetry.h"
#include "platform/windows/gui/ImGuiLayer.h"

namespace lex::platform::windows::hooks {
namespace {

using Execute = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
Execute g_execute = nullptr;

void STDMETHODCALLTYPE executeDetour(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  gui::ImGuiLayer::setGameQueue(queue);
  g_execute(queue, count, lists);
}

}  // namespace

bool installQueueHook() {
  HMODULE module = GetModuleHandleW(L"d3d12.dll");
  if (module == nullptr) return false;
  const auto create = reinterpret_cast<decltype(&D3D12CreateDevice)>(
      reinterpret_cast<void*>(GetProcAddress(module, "D3D12CreateDevice")));
  ID3D12Device* device = nullptr;
  if (create == nullptr || FAILED(create(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return false;
  D3D12_COMMAND_QUEUE_DESC desc{};
  desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  ID3D12CommandQueue* queue = nullptr;
  bool ok = false;
  if (SUCCEEDED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)))) {
    void* target = (*reinterpret_cast<void***>(queue))[10];  // ExecuteCommandLists
    ok = MH_CreateHook(target, reinterpret_cast<void*>(&executeDetour), reinterpret_cast<void**>(&g_execute)) == MH_OK &&
         MH_EnableHook(target) == MH_OK;
    queue->Release();
  }
  game::installTextureHooks(device);
  device->Release();
  return ok;
}

}  // namespace lex::platform::windows::hooks
