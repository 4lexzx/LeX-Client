#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <imgui.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lex::platform::windows::gui {

// Imagenes sueltas para el menu: logo y miniaturas.
class TextureStore {
 public:
  struct Entry {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    ImTextureID texture = 0;
    bool failed = false;
    ID3D11ShaderResourceView* view = nullptr;
    ID3D12Resource* resource = nullptr;
  };

  static TextureStore& instance();
  void add(const std::string& key, int width, int height, const std::uint8_t* rgba);
  const Entry* find(const std::string& key) const;

  void flushDx11(ID3D11Device* device);
  void flushDx12(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* heap, UINT firstSlot, UINT slotCount);
  void shutdown();

 private:
  std::map<std::string, Entry> entries_;
  std::vector<ID3D12Resource*> uploads_;
  int uploadAge_ = 0;
  UINT used_ = 0;
};

}  // namespace lex::platform::windows::gui
