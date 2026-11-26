#include "platform/windows/game/Cosmetics.h"

#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <thread>

#include <nlohmann/json.hpp>

#include "core/util/Logger.h"
#include "platform/windows/game/LiveTextures.h"
#include "platform/windows/game/Telemetry.h"

namespace lex::platform::windows::game {
namespace {

constexpr int kDensity = 12;
constexpr UINT64 kUploadBytes = 12 * 1024 * 1024;

bool readPng(const std::filesystem::path& path, int& width, int& height, std::vector<std::uint8_t>& rgba) {
  const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool ok = false;
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) &&
      SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
      SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
    UINT w = 0;
    UINT h = 0;
    converter->GetSize(&w, &h);
    if (w > 0 && h > 0 && w <= 4096 && h <= 16384) {
      rgba.resize(static_cast<std::size_t>(w) * h * 4);
      ok = SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(rgba.size()), rgba.data()));
      width = static_cast<int>(w);
      height = static_cast<int>(h);
    }
  }
  if (converter != nullptr) converter->Release();
  if (frame != nullptr) frame->Release();
  if (decoder != nullptr) decoder->Release();
  if (factory != nullptr) factory->Release();
  if (SUCCEEDED(com)) CoUninitialize();
  return ok;
}

}  // namespace

Cosmetics& Cosmetics::instance() {
  static Cosmetics* cosmetics = new Cosmetics();
  return *cosmetics;
}

void Cosmetics::load(const std::filesystem::path& dataDirectory) {
  directory_ = dataDirectory;
  try {
    std::ifstream file(directory_ / "cosmetics" / "catalog.json");
    if (!file.is_open()) return;
    nlohmann::json catalog;
    file >> catalog;
    for (const auto& c : catalog) {
      Item item;
      item.id = c.at("id").get<std::string>();
      item.slot = c.at("slot").get<std::string>();
      item.name = c.at("name").get<std::string>();
      item.frames = std::clamp(c.value("frames", 1), 1, 64);
      item.fps = std::clamp(c.value("fps", 12), 1, 60);
      for (int i = 0; i < 4; ++i) item.rect[i] = c.at("rect").at(i).get<int>() * kDensity;
      if (c.contains("alt")) {
        for (int i = 0; i < 4; ++i) item.alt[i] = c["alt"].at(i).get<int>() * kDensity;
      }
      item.trail = c.value("trail", -1);
      if (c.contains("anim") && c["anim"].is_object()) {
        const auto& a = c["anim"];
        item.animFps = a.value("fps", 20.0f);
        item.animFrames = a.value("frames", 0);
        // Copia propia: iterar items() de un temporal deja referencias colgando.
        const nlohmann::json animBones = a.value("bones", nlohmann::json::object());
        for (const auto& [bone, rows] : animBones.items()) {
          if (!rows.is_array()) continue;
          auto& out = item.anim[bone];
          for (const auto& r : rows) {
            std::array<float, 9> v{};
            if (!r.is_array()) continue;
            for (std::size_t k = 0; k < 9 && k < r.size(); ++k) v[k] = r[k].is_number() ? r[k].get<float>() : 0.0f;
            out.push_back(v);
          }
        }
      }
      for (const auto& p : c.value("particles", nlohmann::json::array())) {
        const int channel = p.value("channel", -1);
        const std::string file = p.value("file", std::string{});
        if (channel >= 0 && channel < static_cast<int>(kFileParticles) && file.find_first_of("/\:") == std::string::npos) item.particles.emplace_back(channel, file);
      }
      const auto triple = [](const nlohmann::json& v) {
        return v.is_array() && v.size() == 3 ? std::array<float, 3>{v[0].get<float>(), v[1].get<float>(), v[2].get<float>()} : std::array<float, 3>{};
      };
      static constexpr const char* kFaces[6] = {"north", "south", "east", "west", "up", "down"};
      for (const auto& b : c.value("bones", nlohmann::json::array())) {
        Bone bone;
        bone.name = b.at("name").get<std::string>();
        bone.parent = b.value("parent", std::string{});
        bone.pivot = triple(b.value("pivot", nlohmann::json()));
        const auto wave = [](const nlohmann::json& w, std::array<float, 5>& out) {
          if (w.is_number()) out[0] = w.get<float>();
          else if (w.is_array()) {
            for (std::size_t k = 0; k < 5 && k < w.size(); ++k) out[k] = w[k].get<float>();
          }
        };
        if (b.contains("motion")) {
          const auto& m = b["motion"];
          if (m.contains("rot")) {
            bone.moves = true;
            for (int k = 0; k < 3; ++k) wave(m["rot"][k], bone.motion[k]);
          }
          if (m.contains("pos")) wave(m["pos"][1], bone.lift);
          bone.hidden = m.contains("scale") && m["scale"].is_number() && m["scale"].get<float>() == 0.0f;
        }
        for (const auto& q : b.value("cubes", nlohmann::json::array())) {
          Cube cube;
          cube.origin = triple(q.at("origin"));
          cube.size = triple(q.at("size"));
          cube.pivot = triple(q.value("pivot", nlohmann::json()));
          cube.rotation = triple(q.value("rotation", nlohmann::json()));
          for (int f = 0; f < 6; ++f) {
            if (!q.at("uv").contains(kFaces[f])) continue;
            const auto& uv = q["uv"][kFaces[f]];
            cube.has[f] = true;
            cube.uv[f] = {uv["uv"][0].get<float>(), uv["uv"][1].get<float>(), uv["uv_size"][0].get<float>(), uv["uv_size"][1].get<float>()};
          }
          bone.cubes.push_back(cube);
        }
        item.bones.push_back(std::move(bone));
      }
      if (item.id.find_first_of("./\\:") != std::string::npos) continue;
      items_.push_back(std::move(item));
    }
  } catch (const std::exception& error) {
    LEX_LOG_WARNING(std::string("Catalogo de cosmeticos: ") + error.what());
    items_.clear();
    return;
  }

  for (Item& item : items_) {
    std::error_code error;
    item.valid = std::filesystem::exists(directory_ / "cosmetics" / (item.id + ".png"), error);
  }
  items_.erase(std::remove_if(items_.begin(), items_.end(), [](const Item& i) { return !i.valid; }), items_.end());
  LEX_LOG_INFO("Cosmeticos disponibles: " + std::to_string(items_.size()));
  readLoadout();
}

bool Cosmetics::readImage(const std::filesystem::path& path, int& width, int& height, std::vector<std::uint8_t>& rgba) {
  return readPng(path, width, height, rgba);
}

bool Cosmetics::image(const std::string& name, int& width, int& height, std::vector<std::uint8_t>& rgba) const {
  if (name.find_first_of("./\\:") != std::string::npos) return false;
  return readPng(directory_ / "cosmetics" / (name + ".png"), width, height, rgba);
}

std::vector<const Cosmetics::Item*> Cosmetics::worn() const {
  std::vector<const Item*> out;
  for (const Item& item : items_) {
    if (equipped(item)) out.push_back(&item);
  }
  return out;
}

const Cosmetics::Item* Cosmetics::find(std::string_view id) const {
  for (const Item& item : items_) {
    if (item.id == id) return &item;
  }
  return nullptr;
}

bool Cosmetics::equipped(const Item& item) const {
  const auto it = loadout_.find(item.slot);
  return it != loadout_.end() && it->second == item.id;
}

void Cosmetics::toggle(const Item& item) {
  equip(item.slot, equipped(item) ? std::string() : item.id);
  saveLoadout();
}

void Cosmetics::clear() {
  for (const auto& [slot, id] : std::map<std::string, std::string>(loadout_)) equip(slot, {});
  saveLoadout();
}

void Cosmetics::equip(const std::string& slot, const std::string& id) {
  if (loadout_[slot] == id) return;
  loadout_[slot] = id;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    active_.erase(slot);
  }
  dirty_.insert(slot);
  animated_.erase(slot);
  particlesDirty_ = true;
  const Item* item = find(id);
  if (item == nullptr) return;

  // La decodificacion va aparte para no parar el fotograma.
  const std::filesystem::path path = directory_ / "cosmetics" / (item->id + ".png");
  const int expectedW = item->rect[2];
  const int expectedH = item->rect[3] * item->frames;
  std::thread([this, path, slot, id, expectedW, expectedH]() {
    auto image = std::make_shared<Image>();
    int w = 0;
    int h = 0;
    if (!readPng(path, w, h, image->rgba) || w != expectedW || h != expectedH) return;
    image->id = id;
    std::lock_guard<std::mutex> lock(mutex_);
    ready_[slot] = image;
  }).detach();
}

void Cosmetics::setCapePhysics(bool on) {
  if (capePhysics_ == on) return;
  capePhysics_ = on;
  dirty_.insert("cape");
  animated_.erase("cape");
}

// Canales de particulas de los cosmeticos por archivos: el flipbook del que
// llevas puesto, o vacio (asi no se ve el fuego de un dragon que no llevas).
void Cosmetics::publishParticles() {
  particlesDirty_ = false;
  for (unsigned k = 0; k < kFileParticles; ++k) {
    const int width = kFileParticleW;
    const int height = kFileParticleH + static_cast<int>(k);
    std::vector<std::uint8_t> sheet;
    for (const Item& item : items_) {
      if (!equipped(item)) continue;
      for (const auto& [channel, file] : item.particles) {
        if (channel != static_cast<int>(k)) continue;
        auto cached = particleCache_.find(file);
        if (cached == particleCache_.end()) {
          int w = 0, h = 0;
          std::vector<std::uint8_t> rgba;
          if (!readPng(directory_ / "cosmetics" / (file + ".png"), w, h, rgba) || w != width || h != height) rgba.clear();
          cached = particleCache_.emplace(file, std::move(rgba)).first;
        }
        if (!cached->second.empty()) sheet = cached->second;
      }
    }
    if (sheet.empty()) sheet.assign(static_cast<std::size_t>(width) * height * 4, 0);
    LiveTextures::instance().set(static_cast<Live>(static_cast<unsigned>(Live::FileParticle) + k), width, height, std::move(sheet));
  }
}

// Cada canal de particulas lleva el dibujo del cosmetico que lo usa, o nada.
void Cosmetics::publishTrail() {
  for (unsigned i = 0; i < kTrails; ++i) {
    const int width = 264;
    const int height = 40 + static_cast<int>(i);
    std::vector<std::uint8_t> sheet(static_cast<std::size_t>(width) * height * 4, 0);
    for (const auto& [slot, image] : active_) {
      const Item* worn = find(image->id);
      if (worn == nullptr || worn->trail != static_cast<int>(i)) continue;
      std::memcpy(sheet.data(), image->rgba.data(), std::min(sheet.size(), image->rgba.size()));
    }
    LiveTextures::instance().set(static_cast<Live>(static_cast<unsigned>(Live::Trail) + i), width, height, std::move(sheet));
  }
}

void Cosmetics::readLoadout() {
  std::error_code error;
  const auto path = directory_ / "loadout.json";
  loadoutTime_ = std::filesystem::last_write_time(path, error);
  if (error) return;
  try {
    std::ifstream file(path);
    nlohmann::json json;
    file >> json;
    std::set<std::string> slots;
    for (const Item& item : items_) slots.insert(item.slot);
    for (const std::string& slot : slots) {
      std::string id;
      if (json.contains(slot) && json[slot].is_string()) id = json[slot].get<std::string>();
      const Item* item = find(id);
      equip(slot, item != nullptr && item->slot == slot ? id : std::string());
    }
  } catch (const std::exception&) {
    // El launcher puede estar escribiendo: se reintenta en el siguiente cambio.
  }
}

void Cosmetics::saveLoadout() {
  nlohmann::json json = nlohmann::json::object();
  for (const auto& [slot, id] : loadout_) json[slot] = id.empty() ? nlohmann::json(nullptr) : nlohmann::json(id);
  std::ofstream file(directory_ / "loadout.json", std::ios::trunc);
  file << json.dump(2);
  file.close();
  std::error_code error;
  loadoutTime_ = std::filesystem::last_write_time(directory_ / "loadout.json", error);
}

void Cosmetics::update(float delta) {
  trackKills();
  // La animacion de baja solo se pinta unos segundos tras una baja tuya.
  const bool kill = lastKillTime() != 0 && GetTickCount64() - lastKillTime() < 4500;
  if (kill != killShown_) {
    killShown_ = kill;
    dirty_.insert("kill");
    animated_.erase("kill");
  }
  time_ += delta;
  clock_ += delta;
  if (clock_ > 1.0f) {
    clock_ = 0.0f;
    std::error_code error;
    const auto time = std::filesystem::last_write_time(directory_ / "loadout.json", error);
    if (!error && time != loadoutTime_) readLoadout();
  }

  if (particlesDirty_) publishParticles();
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [slot, image] : ready_) {
    if (loadout_[slot] == image->id) {
      active_[slot] = image;
      dirty_.insert(slot);
      animated_.erase(slot);
    }
  }
  ready_.clear();
  for (const auto& [slot, image] : active_) {
    const Item* item = find(image->id);
    if (item == nullptr || item->frames <= 1) continue;
    const int frame = static_cast<int>(time_ * item->fps) % item->frames;
    if (frame != image->frame) {
      image->frame = frame;
      if (dirty_.insert(slot).second) animated_.insert(slot);
    }
  }
}

void Cosmetics::record(ID3D12Device* device, ID3D12GraphicsCommandList* list, std::size_t frameSlot) {
  // Solo los rastros van en particulas; golpes, bajas y muertes son modelos
  // del atlas y se pintan como cualquier otro cosmetico.
  if (dirty_.erase("trail") > 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    publishTrail();
  }
  ID3D12Resource* atlas = liveTexture(Live::Atlas);
  if (atlas == nullptr || items_.empty()) return;
  if (atlas != painted_) {
    painted_ = atlas;
    for (const Item& item : items_) {
      if (item.trail < 0) dirty_.insert(item.slot);
      animated_.clear();
    }
    LEX_LOG_INFO("Atlas de cosmeticos enlazado");
  }
  if (dirty_.empty()) return;

  if (uploads_.size() <= frameSlot) uploads_.resize(frameSlot + 1);
  Upload& upload = uploads_[frameSlot];
  if (upload.buffer == nullptr) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = kUploadBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload.buffer)))) return;
    if (FAILED(upload.buffer->Map(0, nullptr, reinterpret_cast<void**>(&upload.data)))) {
      upload.buffer->Release();
      upload.buffer = nullptr;
      return;
    }
  }

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = atlas;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
  list->ResourceBarrier(1, &barrier);

  UINT64 offset = 0;
  std::set<std::string> done;
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::array<int, 4>> painted;
  // Lo que no cabe en el bufer de este fotograma se pinta en el siguiente.
  std::set<std::string> later;
  for (const std::string& slot : dirty_) {
    bool full = false;
    const auto active = active_.find(slot);
    // La baja la decide el juego (variable del pack): aqui solo se pinta lo elegido.
    const Item* worn = active != active_.end() ? find(active->second->id) : nullptr;
    painted.clear();
    const auto paint = [&](const std::array<int, 4>& rect, const std::uint8_t* source) {
      if (rect[2] == 0 || std::find(painted.begin(), painted.end(), rect) != painted.end()) return;
      painted.push_back(rect);
      const UINT w = static_cast<UINT>(rect[2]);
      const UINT h = static_cast<UINT>(rect[3]);
      const UINT pitch = (w * 4 + 255) & ~255u;
      offset = (offset + 511) & ~511ull;
      if (offset + static_cast<UINT64>(pitch) * h > kUploadBytes) {
        full = true;
        return;
      }
      for (UINT y = 0; y < h; ++y) {
        std::uint8_t* row = upload.data + offset + static_cast<std::size_t>(y) * pitch;
        if (source != nullptr) std::memcpy(row, source + static_cast<std::size_t>(y) * w * 4, w * 4);
        else std::memset(row, 0, w * 4);
      }
      D3D12_TEXTURE_COPY_LOCATION from{};
      from.pResource = upload.buffer;
      from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      from.PlacedFootprint.Offset = offset;
      from.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, w, h, 1, pitch};
      D3D12_TEXTURE_COPY_LOCATION to{};
      to.pResource = atlas;
      to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      list->CopyTextureRegion(&to, static_cast<UINT>(rect[0]), static_cast<UINT>(rect[1]), 0, &from, nullptr);
      offset += static_cast<UINT64>(pitch) * h;
    };
    // Solo cambia el fotograma: las regiones vacias ya estan limpias.
    const bool frameOnly = animated_.contains(slot);
    const bool plain = slot == "cape" && !capePhysics_;
    for (const Item& item : items_) {
      if (item.slot != slot || item.trail >= 0) continue;
      const std::uint8_t* source = nullptr;
      if (worn != nullptr && worn->rect == item.rect) {
        source = active->second->rgba.data() + static_cast<std::size_t>(active->second->frame) * item.rect[2] * item.rect[3] * 4;
      }
      if (frameOnly && source == nullptr) continue;
      paint(item.rect, plain ? nullptr : source);
      paint(item.alt, plain ? source : nullptr);
    }
    if (full) later.insert(slot);
  }
  animated_.clear();
  dirty_ = std::move(later);
  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  list->ResourceBarrier(1, &barrier);
}

void Cosmetics::releaseGpu() {
  for (Upload& upload : uploads_) {
    if (upload.buffer != nullptr) upload.buffer->Release();
  }
  uploads_.clear();
  painted_ = nullptr;
}

}  // namespace lex::platform::windows::game
