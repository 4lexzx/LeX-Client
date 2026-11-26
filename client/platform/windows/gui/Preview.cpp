#include "platform/windows/gui/Preview.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>

#include "platform/windows/gui/TextureStore.h"

namespace lex::platform::windows::gui {
namespace {

struct V3 {
  float x, y, z;
};

struct Quad {
  std::array<V3, 4> p;  // sup. izq., sup. der., inf. der., inf. izq. vistos desde fuera
  std::array<ImVec2, 4> uv;
  ImTextureID texture;
  float shade;
  bool twoSided = false;
};

constexpr float kRad = 3.14159265f / 180.0f;

V3 rotate(V3 v, const std::array<float, 3>& pivot, const std::array<float, 3>& degrees) {
  V3 p{v.x - pivot[0], v.y - pivot[1], v.z - pivot[2]};
  if (degrees[0] != 0) {
    const float c = std::cos(degrees[0] * kRad), s = std::sin(degrees[0] * kRad);
    p = {p.x, p.y * c + p.z * s, p.z * c - p.y * s};
  }
  if (degrees[2] != 0) {
    const float c = std::cos(degrees[2] * kRad), s = std::sin(degrees[2] * kRad);
    p = {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
  }
  if (degrees[1] != 0) {
    const float c = std::cos(degrees[1] * kRad), s = std::sin(degrees[1] * kRad);
    p = {p.x * c - p.z * s, p.y, p.z * c + p.x * s};
  }
  return {p.x + pivot[0], p.y + pivot[1], p.z + pivot[2]};
}

// 0 norte, 1 sur, 2 este, 3 oeste, 4 arriba, 5 abajo
std::array<V3, 4> face(int f, V3 a, V3 b) {
  switch (f) {
    case 0: return {V3{a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, a.y, a.z}, {a.x, a.y, a.z}};
    case 1: return {V3{b.x, b.y, b.z}, {a.x, b.y, b.z}, {a.x, a.y, b.z}, {b.x, a.y, b.z}};
    case 2: return {V3{b.x, b.y, a.z}, {b.x, b.y, b.z}, {b.x, a.y, b.z}, {b.x, a.y, a.z}};
    case 3: return {V3{a.x, b.y, b.z}, {a.x, b.y, a.z}, {a.x, a.y, a.z}, {a.x, a.y, b.z}};
    case 4: return {V3{a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z}};
    default: return {V3{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}};
  }
}
constexpr float kShade[6] = {1.0f, 0.92f, 0.78f, 0.78f, 1.0f, 0.6f};

void addBox(std::vector<Quad>& out, ImTextureID skin, V3 o, V3 s, float u, float v, float inflate) {
  const V3 a{o.x - inflate, o.y - inflate, o.z - inflate};
  const V3 b{o.x + s.x + inflate, o.y + s.y + inflate, o.z + s.z + inflate};
  const float w = s.x, h = s.y, d = s.z;
  const float rect[6][4] = {{u + d, v + d, w, h},         {u + 2 * d + w, v + d, w, h}, {u + d + w, v + d, d, h},
                            {u, v + d, d, h},             {u + d, v, w, d},             {u + d + w, v, w, d}};
  for (int f = 0; f < 6; ++f) {
    const float x0 = rect[f][0] / 64, y0 = rect[f][1] / 64, x1 = x0 + rect[f][2] / 64, y1 = y0 + rect[f][3] / 64;
    out.push_back({face(f, a, b), {ImVec2(x0, y0), ImVec2(x1, y0), ImVec2(x1, y1), ImVec2(x0, y1)}, skin, kShade[f]});
  }
}

std::array<float, 3> pose(const std::string& bone, float t) {
  if (bone.rfind("lexCape", 0) == 0) {
    const int i = bone.back() - '0';
    const float wave = std::sin(t * 2.6f - i * 0.73f) * 2.2f;
    return {i == 0 ? 9 + wave : wave, 0, 0};
  }
  const float flap = std::sin(t * 2.0f) * 8;
  if (bone == "lexAuraWall") return {0, -t * 22, 0};
  if (bone == "lexAuraOrbs") return {0, t * 60, 0};
  if (bone == "lexBandA" || bone == "lexBandB") return {14 + std::sin(t * 2.4f) * 6, 0, 0};
  // Mascotas: mismas poses que en el juego, sin correr.
  if (bone.rfind("lexPet", 0) == 0) {
    const auto is = [&](const char* name) { return bone == name; };
    if (bone.rfind("lexPetDragon_", 0) == 0) {
      const std::string part = bone.substr(13);
      const float beat = t * 5.2f;
      if (part == "wingR" || part == "wingL") return {0, 0, (part == "wingR" ? -1.0f : 1.0f) * (-8 + std::sin(beat) * 34)};
      if (part == "tipR" || part == "tipL") return {0, 0, (part == "tipR" ? -1.0f : 1.0f) * (6 + std::sin(beat - 0.96f) * 46)};
      if (part.rfind("fronttip", 0) == 0) return {-34, 0, 0};
      if (part.rfind("frontfoot", 0) == 0) return {36, 0, 0};
      if (part.rfind("front", 0) == 0) return {58, 0, 0};
      if (part.rfind("reartip", 0) == 0) return {28, 0, 0};
      if (part.rfind("rearfoot", 0) == 0) return {38, 0, 0};
      if (part.rfind("rear", 0) == 0) return {62, 0, 0};
      if (part.rfind("neck", 0) == 0) return {std::sin(t * 1.4f - (part[4] - '0') * 0.5f) * 4, std::sin(t * 0.9f - (part[4] - '0') * 0.4f) * 5, 0};
      if (part == "head") return {6 + std::sin(t * 1.2f) * 6, std::sin(t * 0.9f) * 12, 0};
      if (part == "jaw") return {8 + std::max(0.0f, std::sin(t)) * 14, 0, 0};
      if (part.rfind("tail", 0) == 0) return {std::sin(t * 1.7f - (part[4] - '0') * 0.55f) * 5, std::sin(t * 2.1f - (part[4] - '0') * 0.62f) * 8, 0};
    }
    const float sit = std::clamp(std::sin(t * 0.55f) * 3.0f, 0.0f, 1.0f);
    const float walk = std::sin(t * 6.0f) * 30.0f * (1.0f - sit);
    if (is("lexPetCat_body") || is("lexPetFox_body")) return {-26 * sit, 0, 0};
    if (is("lexPetCat_head") || is("lexPetFox_head")) return {24 * sit + std::sin(t) * 5, std::sin(t * 0.65f) * 22, 0};
    if (is("lexPetCat_frontLegL") || is("lexPetFox_leg3")) return {walk + 26 * sit, 0, 0};
    if (is("lexPetCat_frontLegR") || is("lexPetFox_leg2")) return {-walk + 26 * sit, 0, 0};
    if (is("lexPetCat_backLegL") || is("lexPetFox_leg1")) return {-walk - 42 * sit, 0, 0};
    if (is("lexPetCat_backLegR") || is("lexPetFox_leg0")) return {walk - 42 * sit, 0, 0};
    if (is("lexPetCat_tail1")) return {62 - 20 * sit + std::sin(t * 1.4f) * 8, std::sin(t * 1.2f) * 16, 0};
    if (is("lexPetCat_tail2")) return {26 + std::sin(t * 1.9f) * 18, std::sin(t * 1.6f) * 20, 0};
    if (is("lexPetFox_tail")) return {78 + std::sin(t * 1.6f) * 8, std::sin(t * 2.1f) * 18, 0};
    if (is("lexPetParrot_wing0")) return {0, 0, 52 - std::sin(t * 15) * 46};
    if (is("lexPetParrot_wing1")) return {0, 0, -52 + std::sin(t * 15) * 46};
    if (is("lexPetParrot_body")) return {24, 0, 0};
    if (is("lexPetParrot_head")) return {std::sin(t * 1.6f) * 8, std::sin(t) * 26, 0};
    if (is("lexPetParrot_tail")) return {30, 0, 0};
    if (is("lexPetBee_rightwing_bone")) return {15, 15, -std::sin(t * 40) * 34};
    if (is("lexPetBee_leftwing_bone")) return {15, -15, std::sin(t * 40) * 34};
    if (is("lexPetAllay_left_wing")) return {0, -40 - std::sin(t * 12) * 28, 0};
    if (is("lexPetAllay_right_wing")) return {0, 40 + std::sin(t * 12) * 28, 0};
    if (is("lexPetAllay_body")) return {10, 0, 0};
  }
  if (bone == "lexMiniL") return {0, 30 + std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexMiniR") return {0, -30 - std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexPetSpiritOrbs") return {0, t * 130, 0};
  if (bone == "lexDragonL") return {0, 24 + std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexDragonL2") return {0, 14 + std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexDragonR") return {0, -24 - std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexDragonR2") return {0, -14 - std::sin(t * 2.0f) * 8, 0};
  if (bone == "lexWingL") return {0, 26 + flap, 0};
  if (bone == "lexWingL2") return {0, 14 + flap, 0};
  if (bone == "lexWingR") return {0, -26 - flap, 0};
  if (bone == "lexWingR2") return {0, -14 - flap, 0};
  if (bone == "lexHalo") return {8, t * 30, 0};
  if (bone == "lexAura") return {0, t * 36, 0};
  return {0, 0, 0};
}

}  // namespace

Preview& Preview::instance() {
  static Preview* preview = new Preview();
  return *preview;
}

void Preview::request(const std::string& id) {
  // Una carga a la vez: abrir la pestana no debe dar tirones.
  if (requested_.contains(id)) return;
  if (!requested_.empty() && TextureStore::instance().find("strip." + *requested_.rbegin()) == nullptr && requested_.size() > 200) return;
  requested_.insert(id);
  std::thread([this, id]() {
    auto loaded = std::make_shared<Loaded>();
    loaded->key = "strip." + id;
    if (!game::Cosmetics::instance().image(id, loaded->w, loaded->h, loaded->rgba)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    ready_.push_back(loaded);
  }).detach();
}

void Preview::update() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ready_.empty()) return;
  const auto loaded = ready_.front();
  ready_.erase(ready_.begin());
  TextureStore::instance().add(loaded->key, loaded->w, loaded->h, loaded->rgba.data());
}

void Preview::draw(ImDrawList& list, ImVec2 min, ImVec2 size, const std::vector<const game::Cosmetics::Item*>& worn, float yaw, bool zoomHead, bool wide) {
  const float t = static_cast<float>(ImGui::GetTime());
  const auto* skin = TextureStore::instance().find("skin");
  std::vector<Quad> quads;
  quads.reserve(160);
  // Animaciones de muerte: se ensenan solas, sin el personaje encima.
  const bool effect = !worn.empty() && std::all_of(worn.begin(), worn.end(), [](const auto* i) { return i->slot == "kill" || i->slot == "death" || i->slot == "hit" || (i->slot == "pet" && !i->anim.empty()); });
  if (skin != nullptr && !effect) {
    const ImTextureID s = skin->texture;
    addBox(quads, s, {-4, 24, -4}, {8, 8, 8}, 0, 0, 0);
    addBox(quads, s, {-4, 24, -4}, {8, 8, 8}, 32, 0, 0.5f);
    addBox(quads, s, {-4, 12, -2}, {8, 12, 4}, 16, 16, 0);
    addBox(quads, s, {-4, 12, -2}, {8, 12, 4}, 16, 32, 0.25f);
    addBox(quads, s, {-8, 12, -2}, {4, 12, 4}, 40, 16, 0);
    addBox(quads, s, {-8, 12, -2}, {4, 12, 4}, 40, 32, 0.25f);
    addBox(quads, s, {4, 12, -2}, {4, 12, 4}, 32, 48, 0);
    addBox(quads, s, {4, 12, -2}, {4, 12, 4}, 48, 48, 0.25f);
    addBox(quads, s, {-4, 0, -2}, {4, 12, 4}, 0, 16, 0);
    addBox(quads, s, {-4, 0, -2}, {4, 12, 4}, 0, 32, 0.25f);
    addBox(quads, s, {0, 0, -2}, {4, 12, 4}, 16, 48, 0);
    addBox(quads, s, {0, 0, -2}, {4, 12, 4}, 0, 48, 0.25f);
  }

  for (const auto* item : worn) {
    request(item->id);
    const auto* strip = TextureStore::instance().find("strip." + item->id);
    if (strip == nullptr) continue;
    const int frame = item->frames > 1 ? static_cast<int>(t * item->fps) % item->frames : 0;
    const float rx = item->rect[0] / 12.0f, ry = item->rect[1] / 12.0f, rw = item->rect[2] / 12.0f, rh = item->rect[3] / 12.0f;
    // Muestra de la animacion de un hueso en este instante (o nada).
    const int animFrame = item->animFrames > 0 ? static_cast<int>(t * item->animFps) % item->animFrames : 0;
    const auto sample = [&](const game::Cosmetics::Bone& b) -> const std::array<float, 9>* {
      const auto it = item->anim.find(b.name);
      return it != item->anim.end() && animFrame < static_cast<int>(it->second.size()) ? &it->second[animFrame] : nullptr;
    };
    for (const auto& bone : item->bones) {
      // Oculto si el hueso o alguno de sus padres tiene escala 0 ahora mismo.
      bool hiddenNow = false;
      for (const game::Cosmetics::Bone* link = &bone; link != nullptr && !hiddenNow;) {
        const auto* s = sample(*link);
        hiddenNow = s != nullptr ? ((*s)[6] == 0.0f || (*s)[7] == 0.0f) : link->hidden;
        const std::string& parent = link->parent;
        link = nullptr;
        for (const auto& other : item->bones) {
          if (other.name == parent) link = &other;
        }
      }
      if (hiddenNow) continue;
      for (const auto& cube : bone.cubes) {
        const V3 a{cube.origin[0], cube.origin[1], cube.origin[2]};
        const V3 b{a.x + cube.size[0], a.y + cube.size[1], a.z + cube.size[2]};
        for (int f = 0; f < 6; ++f) {
          if (!cube.has[f]) continue;
          Quad q{face(f, a, b), {}, strip->texture, kShade[f], cube.size[0] == 0 || cube.size[1] == 0 || cube.size[2] == 0};
          const float x0 = (cube.uv[f][0] - rx) / rw, x1 = (cube.uv[f][0] + cube.uv[f][2] - rx) / rw;
          const float y0 = ((cube.uv[f][1] - ry) / rh + frame) / item->frames, y1 = ((cube.uv[f][1] + cube.uv[f][3] - ry) / rh + frame) / item->frames;
          q.uv = {ImVec2(x0, y0), ImVec2(x1, y0), ImVec2(x1, y1), ImVec2(x0, y1)};
          for (V3& p : q.p) {
            if (cube.rotation != std::array<float, 3>{0, 0, 0}) p = rotate(p, cube.pivot, {cube.rotation[0], -cube.rotation[1], -cube.rotation[2]});
            // Cadena de huesos: propio, padre, abuelo...
            const game::Cosmetics::Bone* link = &bone;
            for (int depth = 0; link != nullptr && depth < 24; ++depth) {
              const auto at = [&](const std::array<float, 5>& w) { return w[0] + w[1] * std::sin((w[2] * t + w[3]) * kRad) + w[4] * t; };
              if (const auto* s = sample(*link)) {
                const auto& v = *s;
                p = {link->pivot[0] + (p.x - link->pivot[0]) * v[6], link->pivot[1] + (p.y - link->pivot[1]) * v[7], link->pivot[2] + (p.z - link->pivot[2]) * v[8]};
                p = rotate(p, link->pivot, {v[0], -v[1], -v[2]});
                p.x += v[3];
                p.y += v[4];
                p.z += v[5];
              } else {
                p = rotate(p, link->pivot, link->moves ? std::array<float, 3>{at(link->motion[0]), -at(link->motion[1]), -at(link->motion[2])} : pose(link->name, t));
                p.y += at(link->lift);
              }
              const std::string& parent = link->parent;
              link = nullptr;
              for (const auto& other : item->bones) {
                if (other.name == parent) link = &other;
              }
            }
          }
          quads.push_back(q);
        }
      }
    }
  }

  // Camara
  const float c = std::cos(yaw * kRad), s = std::sin(yaw * kRad);
  const float pitch = zoomHead ? 10.0f : 12.0f;
  const float pc = std::cos(pitch * kRad), ps = std::sin(pitch * kRad);
  float centerY = zoomHead ? 30.0f : 18.0f;
  float scale = std::min(size.x / (zoomHead ? 17.0f : wide ? 40.0f : 19.0f), size.y / (zoomHead ? 18.0f : 38.0f));
  if (effect && !quads.empty()) {
    // Encuadre segun el propio modelo (el rayo es muy alto, la lapida baja).
    float low = 1e9f, high = -1e9f, radius = 1.0f;
    for (const Quad& q : quads) {
      for (const V3& p : q.p) {
        low = std::min(low, p.y);
        high = std::max(high, p.y);
        radius = std::max(radius, std::sqrt(p.x * p.x + p.z * p.z));
      }
    }
    centerY = (low + high) * 0.5f;
    scale = std::min(size.x / (radius * 2.0f + 3.0f), size.y / (high - low + 4.0f));
  }
  const ImVec2 middle(min.x + size.x * 0.5f, min.y + size.y * 0.5f);
  struct Face {
    ImVec2 p[4];
    float depth;
    const Quad* quad;
  };
  std::vector<Face> faces;
  faces.reserve(quads.size());
  for (const Quad& q : quads) {
    Face out{};
    out.quad = &q;
    for (int i = 0; i < 4; ++i) {
      const float x = q.p[i].x * c + q.p[i].z * s;
      const float z0 = -q.p[i].x * s + q.p[i].z * c;
      const float y0 = q.p[i].y - centerY;
      const float y = y0 * pc + z0 * ps;
      const float z = z0 * pc - y0 * ps;
      const float k = 90.0f / (90.0f + z) * scale;
      out.p[i] = ImVec2(middle.x + x * k, middle.y - y * k);
      out.depth += z * 0.25f;
    }
    const float cross = (out.p[1].x - out.p[0].x) * (out.p[3].y - out.p[0].y) - (out.p[1].y - out.p[0].y) * (out.p[3].x - out.p[0].x);
    if (cross > 0.0f || q.twoSided) faces.push_back(out);
  }
  std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth > b.depth; });

  list.PushClipRect(min, ImVec2(min.x + size.x, min.y + size.y), true);
  const float alpha = ImGui::GetStyle().Alpha;
  for (const Face& f : faces) {
    const int v = static_cast<int>(255 * f.quad->shade);
    list.AddImageQuad(f.quad->texture, f.p[0], f.p[1], f.p[2], f.p[3], f.quad->uv[0], f.quad->uv[1], f.quad->uv[2], f.quad->uv[3], IM_COL32(v, v, v, static_cast<int>(255 * alpha)));
  }
  list.PopClipRect();
}

}  // namespace lex::platform::windows::gui
