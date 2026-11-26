#pragma once

#include <d3d12.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace lex::platform::windows::game {

// Cosmeticos en vivo: repinta en la GPU la region del atlas que usa cada uno.
class Cosmetics {
 public:
  struct Cube {
    std::array<float, 3> origin{};
    std::array<float, 3> size{};
    std::array<float, 3> pivot{};
    std::array<float, 3> rotation{};
    std::array<bool, 6> has{};
    std::array<std::array<float, 4>, 6> uv{};  // u, v, ancho, alto
  };
  struct Bone {
    std::string name;
    std::string parent;
    std::array<float, 3> pivot{};
    // Movimiento ciclico: por eje, base + amp * sin(t * vel + fase) + lin * t.
    std::array<std::array<float, 5>, 3> motion{};
    std::array<float, 5> lift{};
    bool moves = false;
    bool hidden = false;  // escala 0 en reposo (p. ej. el fuego del dragon)
    std::vector<Cube> cubes;
  };
  struct Item {
    std::string id;
    std::string slot;
    std::string name;
    std::array<int, 4> rect{};  // pixeles dentro del atlas
    std::array<int, 4> alt{};   // segunda region (capa sin fisica); vacia si no hay
    int trail = -1;             // indice de particula
    // Particulas propias (cosmeticos por archivos): canal y PNG a pintar.
    std::vector<std::pair<int, std::string>> particles;
    // Vista previa animada (cosmeticos por archivos): por hueso y fotograma,
    // giro xyz, posicion xyz y escala xyz ya calculados por el launcher.
    float animFps = 0.0f;
    int animFrames = 0;
    std::map<std::string, std::vector<std::array<float, 9>>> anim;
    int frames = 1;
    int fps = 12;
    bool valid = false;
    std::vector<Bone> bones;
  };

  static Cosmetics& instance();

  void load(const std::filesystem::path& dataDirectory);
  void update(float delta);
  void record(ID3D12Device* device, ID3D12GraphicsCommandList* list, std::size_t frameSlot);
  void releaseGpu();
  void invalidate() { painted_ = nullptr; }

  const std::vector<Item>& items() const noexcept { return items_; }
  const std::filesystem::path& directory() const noexcept { return directory_; }
  // PNG de la carpeta de cosmeticos, por nombre sin extension.
  bool image(const std::string& name, int& width, int& height, std::vector<std::uint8_t>& rgba) const;
  static bool readImage(const std::filesystem::path& path, int& width, int& height, std::vector<std::uint8_t>& rgba);
  std::vector<const Item*> worn() const;
  bool equipped(const Item& item) const;
  void toggle(const Item& item);
  void clear();
  void setCapePhysics(bool on);

 private:
  struct Image {
    std::string id;
    int frame = 0;
    std::vector<std::uint8_t> rgba;
  };
  struct Upload {
    ID3D12Resource* buffer = nullptr;
    std::uint8_t* data = nullptr;
  };

  Cosmetics() = default;
  const Item* find(std::string_view id) const;
  void equip(const std::string& slot, const std::string& id);
  void readLoadout();
  void saveLoadout();
  void publishTrail();
  void publishParticles();

  std::filesystem::path directory_;
  std::filesystem::file_time_type loadoutTime_{};
  std::vector<Item> items_;
  std::map<std::string, std::string> loadout_;
  std::mutex mutex_;  // protege active_ y ready_
  std::map<std::string, std::shared_ptr<Image>> active_;
  std::map<std::string, std::shared_ptr<Image>> ready_;
  std::set<std::string> dirty_;
  std::set<std::string> animated_;  // de dirty_, solo cambio de fotograma
  std::vector<Upload> uploads_;
  ID3D12Resource* painted_ = nullptr;
  double time_ = 0.0;
  float clock_ = 0.0f;
  bool capePhysics_ = true;
  bool killShown_ = false;
  bool particlesDirty_ = true;
  std::map<std::string, std::vector<std::uint8_t>> particleCache_;
};

}  // namespace lex::platform::windows::game
