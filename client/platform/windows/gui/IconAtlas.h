#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <imgui.h>

#include "platform/windows/resources/IconResource.h"

namespace lex::platform::windows::gui {

// Iconos lineales propios embebidos como RCDATA en LeXClient.dll (64x64 RGBA).
// Se suben a GPU segun el backend activo y se exponen como ImTextureID.
class IconAtlas {
 public:
  static constexpr int kIconCount = 28;
  static constexpr std::uint32_t kIconSize = 64;

  static IconAtlas& instance();

  void setModule(HMODULE module) noexcept { module_ = module; }
  bool loadFromResources();
  int loadedCount() const noexcept { return loadedCount_; }

  bool initializeDX11(ID3D11Device* device);
  bool initializeDX12(ID3D12Device* device, ID3D12DescriptorHeap* heap,
                      UINT firstDescriptor, ID3D12CommandQueue* queue);
  void shutdown();

  // Devuelve 0 si el icono no existe o el backend aun no esta listo.
  ImTextureID texture(std::string_view iconId) const;

 private:
  struct Entry {
    const char* id = "";
    WORD resourceId = 0;
    std::vector<std::uint8_t> rgba;
    ImTextureID texture = 0;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D12Resource* gpuTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
  };

  IconAtlas() = default;
  ~IconAtlas() { shutdown(); }

  HMODULE module_ = nullptr;
  int loadedCount_ = 0;
  bool uploaded_ = false;
  std::array<Entry, kIconCount> entries_;

  ID3D11Device* d3d11Device_ = nullptr;
  ID3D12Device* d3d12Device_ = nullptr;
  ID3D12DescriptorHeap* descriptorHeap_ = nullptr;
  UINT firstDescriptor_ = 0;
  ID3D12CommandQueue* queue_ = nullptr;
  ID3D12CommandAllocator* allocator_ = nullptr;
  ID3D12GraphicsCommandList* commandList_ = nullptr;
  ID3D12Fence* fence_ = nullptr;
  HANDLE fenceEvent_ = nullptr;
  std::uint64_t fenceValue_ = 0;
};

}  // namespace lex::platform::windows::gui
