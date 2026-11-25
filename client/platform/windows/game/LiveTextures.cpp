#include "platform/windows/game/LiveTextures.h"

#include <cstring>

namespace lex::platform::windows::game {

LiveTextures& LiveTextures::instance() {
  static LiveTextures* textures = new LiveTextures();
  return *textures;
}

void LiveTextures::set(Live which, int width, int height, std::vector<std::uint8_t> rgba) {
  Slot& slot = slots_[static_cast<std::size_t>(which)];
  if (slot.rgba == rgba) return;
  slot.width = width;
  slot.height = height;
  slot.rgba = std::move(rgba);
  slot.dirty = true;
}

void LiveTextures::record(ID3D12Device* device, ID3D12GraphicsCommandList* list, std::uint64_t frame) {
  while (!uploads_.empty() && frame - uploads_.front().frame > 12) {
    uploads_.front().buffer->Release();
    uploads_.erase(uploads_.begin());
  }
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    Slot& slot = slots_[i];
    ID3D12Resource* target = slot.rgba.empty() ? nullptr : liveTexture(static_cast<Live>(i));
    if (target == nullptr || (target == slot.painted && !slot.dirty)) continue;

    const UINT w = static_cast<UINT>(slot.width);
    const UINT h = static_cast<UINT>(slot.height);
    const UINT pitch = (w * 4 + 255) & ~255u;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = static_cast<UINT64>(pitch) * h;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* buffer = nullptr;
    std::uint8_t* data = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)))) continue;
    uploads_.push_back({buffer, frame});
    if (FAILED(buffer->Map(0, nullptr, reinterpret_cast<void**>(&data)))) continue;
    for (UINT y = 0; y < h; ++y) std::memcpy(data + static_cast<std::size_t>(y) * pitch, slot.rgba.data() + static_cast<std::size_t>(y) * w * 4, w * 4);
    buffer->Unmap(0, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Transition = {target, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    list->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource = buffer;
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, w, h, 1, pitch};
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource = target;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    list->ResourceBarrier(1, &barrier);
    slot.painted = target;
    slot.dirty = false;
  }
}

void LiveTextures::releaseGpu() {
  for (Upload& upload : uploads_) upload.buffer->Release();
  uploads_.clear();
  for (Slot& slot : slots_) slot.painted = nullptr;
}

}  // namespace lex::platform::windows::game
