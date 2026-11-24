#include "platform/windows/gui/IconAtlas.h"

#include <algorithm>
#include <cstring>

#include "core/util/Logger.h"

namespace lex::platform::windows::gui {

using lex::core::util::Logger;

namespace {

struct IconEntryDefinition {
  const char* id;
  WORD resourceId;
};

// El orden coincide con IconResource.h.
constexpr IconEntryDefinition kDefinitions[] = {
    {"icon.category.hud", IDR_ICON_CATEGORY_HUD},
    {"icon.category.visual", IDR_ICON_CATEGORY_VISUAL},
    {"icon.category.utility", IDR_ICON_CATEGORY_UTILITY},
    {"icon.category.cosmetics", IDR_ICON_CATEGORY_COSMETICS},
    {"icon.category.settings", IDR_ICON_CATEGORY_SETTINGS},
    {"icon.star", IDR_ICON_FAVORITES},
    {"icon.search", IDR_ICON_SEARCH},
    {"icon.gear", IDR_ICON_GEAR},
    {"icon.reset", IDR_ICON_RESET},
    {"icon.close", IDR_ICON_CLOSE},
    {"icon.module.fps", IDR_ICON_FPS},
    {"icon.module.cps", IDR_ICON_CPS},
    {"icon.module.coordinates", IDR_ICON_COORDINATES},
    {"icon.module.keystrokes", IDR_ICON_KEYSTROKES},
    {"icon.module.ping", IDR_ICON_PING},
    {"icon.module.clock", IDR_ICON_CLOCK},
    {"icon.module.armor", IDR_ICON_ARMOR},
    {"icon.module.speedometer", IDR_ICON_SPEEDOMETER},
    {"icon.module.zoom", IDR_ICON_ZOOM},
    {"icon.module.fullbright", IDR_ICON_FULLBRIGHT},
    {"icon.module.crosshair", IDR_ICON_CROSSHAIR},
    {"icon.module.hurtcam", IDR_ICON_HURTCAM},
    {"icon.module.nametag", IDR_ICON_NAMETAG},
    {"icon.module.toggle_sprint", IDR_ICON_TOGGLE_SPRINT},
    {"icon.module.screenshot", IDR_ICON_SCREENSHOT},
    {"icon.module.chat", IDR_ICON_CHAT},
    {"icon.module.fps_limit", IDR_ICON_FPS_LIMIT},
    {"icon.module.test", IDR_ICON_MODULE_TEST},
};

static_assert(sizeof(kDefinitions) / sizeof(kDefinitions[0]) ==
                  IconAtlas::kIconCount,
              "El numero de entradas debe coincidir con kIconCount");

}  // namespace

IconAtlas& IconAtlas::instance() {
  static IconAtlas atlas;
  return atlas;
}

bool IconAtlas::loadFromResources() {
  if (module_ == nullptr || loadedCount_ > 0) return loadedCount_ > 0;

  loadedCount_ = 0;
  for (int index = 0; index < kIconCount; ++index) {
    Entry& entry = entries_[static_cast<std::size_t>(index)];
    const IconEntryDefinition& definition = kDefinitions[index];

    HRSRC resource = FindResourceW(
        module_, MAKEINTRESOURCEW(definition.resourceId),
        MAKEINTRESOURCEW(10));  // RT_RCDATA (10) en su variante W.
    if (resource == nullptr) continue;

    HGLOBAL loaded = LoadResource(module_, resource);
    if (loaded == nullptr) continue;

    const void* bytes = LockResource(loaded);
    const DWORD size = SizeofResource(module_, resource);
    if (bytes == nullptr || size != kIconSize * kIconSize * 4) continue;

    const std::uint8_t* first = static_cast<const std::uint8_t*>(bytes);
    entry.rgba.assign(first, first + size);
    entry.id = definition.id;
    entry.resourceId = definition.resourceId;
    ++loadedCount_;
  }

  Logger::instance().info("Iconos embebidos cargados: " +
                          std::to_string(loadedCount_));
  return loadedCount_ == kIconCount;
}

ImTextureID IconAtlas::texture(std::string_view iconId) const {
  if (!uploaded_) return 0;
  for (const Entry& entry : entries_) {
    if (iconId == entry.id) return entry.texture;
  }
  return 0;
}

bool IconAtlas::initializeDX11(ID3D11Device* device) {
  if (device == nullptr || uploaded_) return false;
  d3d11Device_ = device;

  for (Entry& entry : entries_) {
    if (entry.rgba.empty()) continue;

    D3D11_TEXTURE2D_DESC description{};
    description.Width = kIconSize;
    description.Height = kIconSize;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = entry.rgba.data();
    data.SysMemPitch = kIconSize * 4;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&description, &data, &texture)) ||
        texture == nullptr) {
      continue;
    }

    ID3D11ShaderResourceView* view = nullptr;
    if (SUCCEEDED(device->CreateShaderResourceView(texture, nullptr, &view)) &&
        view != nullptr) {
      entry.srv = view;
      entry.texture = reinterpret_cast<ImTextureID>(view);
    }
    texture->Release();
  }

  uploaded_ = true;
  Logger::instance().info("Iconos subidos a GPU (D3D11)");
  return true;
}

bool IconAtlas::initializeDX12(ID3D12Device* device,
                               ID3D12DescriptorHeap* heap,
                               UINT firstDescriptor,
                               ID3D12CommandQueue* commandQueue) {
  if (device == nullptr || heap == nullptr || commandQueue == nullptr ||
      uploaded_) {
    return false;
  }

  d3d12Device_ = device;
  descriptorHeap_ = heap;
  firstDescriptor_ = firstDescriptor;
  queue_ = commandQueue;

  if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(&allocator_))) ||
      FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator_, nullptr,
                                       IID_PPV_ARGS(&commandList_))) ||
      FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                 IID_PPV_ARGS(&fence_)))) {
    Logger::instance().error("No se pudieron crear recursos de iconos D3D12");
    return false;
  }
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  D3D12_HEAP_PROPERTIES defaultHeap{};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_HEAP_PROPERTIES uploadHeap{};
  uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC textureDescription{};
  textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  textureDescription.Width = kIconSize;
  textureDescription.Height = kIconSize;
  textureDescription.DepthOrArraySize = 1;
  textureDescription.MipLevels = 1;
  textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  textureDescription.SampleDesc.Count = 1;

  std::vector<ID3D12Resource*> uploads;

  for (Entry& entry : entries_) {
    if (entry.rgba.empty()) continue;

    ID3D12Resource* texture = nullptr;
    if (FAILED(device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)))) {
      continue;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0;
    UINT64 rowSize = 0;
    UINT64 requiredSize = 0;
    device->GetCopyableFootprints(&textureDescription, 0, 1, 0, &footprint,
                                  &rows, &rowSize, &requiredSize);

    D3D12_RESOURCE_DESC uploadDescription{};
    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Width = requiredSize;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.Format = DXGI_FORMAT_UNKNOWN;
    uploadDescription.SampleDesc.Count = 1;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* upload = nullptr;
    if (FAILED(device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
      texture->Release();
      continue;
    }

    std::uint8_t* mapped = nullptr;
    if (SUCCEEDED(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) {
      for (UINT row = 0; row < rows; ++row) {
        std::memcpy(mapped + footprint.Offset +
                        static_cast<std::size_t>(row) *
                            footprint.Footprint.RowPitch,
                    entry.rgba.data() +
                        static_cast<std::size_t>(row) * kIconSize * 4,
                    static_cast<std::size_t>(rowSize));
      }
      upload->Unmap(0, nullptr);
    }

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = upload;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    commandList_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    entry.gpuTexture = texture;
    uploads.push_back(upload);
  }

  commandList_->Close();
  ID3D12CommandList* commands[] = {commandList_};
  queue_->ExecuteCommandLists(1, commands);
  ++fenceValue_;
  queue_->Signal(fence_, fenceValue_);
  if (fence_->GetCompletedValue() < fenceValue_) {
    fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
  }

  for (ID3D12Resource* upload : uploads) upload->Release();

  const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE cpu =
      heap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE gpu =
      heap->GetGPUDescriptorHandleForHeapStart();
  cpu.ptr += static_cast<SIZE_T>(firstDescriptor) * descriptorSize;
  gpu.ptr += static_cast<SIZE_T>(firstDescriptor) * descriptorSize;

  for (Entry& entry : entries_) {
    if (entry.gpuTexture == nullptr) continue;
    device->CreateShaderResourceView(entry.gpuTexture, nullptr, cpu);
    entry.gpuHandle = gpu;
    entry.texture = reinterpret_cast<ImTextureID>(gpu.ptr);
    cpu.ptr += descriptorSize;
    gpu.ptr += descriptorSize;
  }

  uploaded_ = true;
  Logger::instance().info("Iconos subidos a GPU (D3D12)");
  return true;
}

void IconAtlas::shutdown() {
  for (Entry& entry : entries_) {
    if (entry.srv != nullptr) {
      entry.srv->Release();
      entry.srv = nullptr;
    }
    if (entry.gpuTexture != nullptr) {
      entry.gpuTexture->Release();
      entry.gpuTexture = nullptr;
    }
    entry.texture = 0;
    entry.rgba.clear();
    entry.gpuHandle = D3D12_GPU_DESCRIPTOR_HANDLE{};
  }

  if (fenceEvent_ != nullptr) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
  if (fence_ != nullptr) {
    fence_->Release();
    fence_ = nullptr;
  }
  if (commandList_ != nullptr) {
    commandList_->Release();
    commandList_ = nullptr;
  }
  if (allocator_ != nullptr) {
    allocator_->Release();
    allocator_ = nullptr;
  }

  d3d11Device_ = nullptr;
  d3d12Device_ = nullptr;
  descriptorHeap_ = nullptr;
  queue_ = nullptr;
  uploaded_ = false;
  loadedCount_ = 0;
}

}  // namespace lex::platform::windows::gui
