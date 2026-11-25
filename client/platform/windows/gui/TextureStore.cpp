#include "platform/windows/gui/TextureStore.h"

#include <cstring>

namespace lex::platform::windows::gui {

TextureStore& TextureStore::instance() {
  static TextureStore* store = new TextureStore();
  return *store;
}

void TextureStore::add(const std::string& key, int width, int height, const std::uint8_t* rgba) {
  if (width <= 0 || height <= 0 || rgba == nullptr || entries_.contains(key)) return;
  Entry entry;
  entry.width = width;
  entry.height = height;
  entry.rgba.assign(rgba, rgba + static_cast<std::size_t>(width) * height * 4);
  entries_.emplace(key, std::move(entry));
}

const TextureStore::Entry* TextureStore::find(const std::string& key) const {
  const auto it = entries_.find(key);
  return it != entries_.end() && it->second.texture != 0 ? &it->second : nullptr;
}

void TextureStore::flushDx11(ID3D11Device* device) {
  for (auto& [key, entry] : entries_) {
    if (entry.texture != 0) continue;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(entry.width);
    desc.Height = static_cast<UINT>(entry.height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{entry.rgba.data(), desc.Width * 4, 0};
    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture))) continue;
    if (SUCCEEDED(device->CreateShaderResourceView(texture, nullptr, &entry.view))) {
      entry.texture = reinterpret_cast<ImTextureID>(entry.view);
    }
    texture->Release();
  }
}

void TextureStore::flushDx12(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* heap, UINT firstSlot, UINT slotCount) {
  // Las subidas anteriores ya salieron de la GPU tras varios fotogramas.
  if (!uploads_.empty() && ++uploadAge_ > 8) {
    for (ID3D12Resource* r : uploads_) r->Release();
    uploads_.clear();
  }
  const UINT step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  for (auto& [key, entry] : entries_) {
    if (entry.texture != 0 || entry.failed) continue;
    entry.failed = true;
    if (used_ >= slotCount) continue;

    D3D12_HEAP_PROPERTIES gpu{};
    gpu.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = static_cast<UINT64>(entry.width);
    desc.Height = static_cast<UINT>(entry.height);
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    if (FAILED(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&entry.resource)))) continue;

    const UINT pitch = (static_cast<UINT>(entry.width) * 4 + 255) & ~255u;
    D3D12_HEAP_PROPERTIES cpuHeap{};
    cpuHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = static_cast<UINT64>(pitch) * entry.height;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* upload = nullptr;
    std::uint8_t* mapped = nullptr;
    if (FAILED(device->CreateCommittedResource(&cpuHeap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) continue;
    uploads_.push_back(upload);
    uploadAge_ = 0;
    if (FAILED(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) continue;
    for (int y = 0; y < entry.height; ++y) {
      std::memcpy(mapped + static_cast<std::size_t>(y) * pitch, entry.rgba.data() + static_cast<std::size_t>(y) * entry.width * 4, static_cast<std::size_t>(entry.width) * 4);
    }
    upload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource = upload;
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT>(entry.width), static_cast<UINT>(entry.height), 1, pitch};
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource = entry.resource;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Transition.pResource = entry.resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    list->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE handle = heap->GetGPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(firstSlot + used_) * step;
    handle.ptr += static_cast<UINT64>(firstSlot + used_) * step;
    ++used_;
    device->CreateShaderResourceView(entry.resource, nullptr, cpu);
    entry.texture = static_cast<ImTextureID>(handle.ptr);
    entry.failed = false;
    entry.rgba.clear();
    entry.rgba.shrink_to_fit();
  }
}

void TextureStore::shutdown() {
  for (auto& [key, entry] : entries_) {
    if (entry.view != nullptr) entry.view->Release();
    if (entry.resource != nullptr) entry.resource->Release();
  }
  for (ID3D12Resource* r : uploads_) r->Release();
  uploads_.clear();
  entries_.clear();
  used_ = 0;
}

}  // namespace lex::platform::windows::gui
