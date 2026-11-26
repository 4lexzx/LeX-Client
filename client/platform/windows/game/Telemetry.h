#pragma once

#include <d3d12.h>

#include <atomic>

namespace lex::platform::windows::game {

// Datos que el propio juego publica (ver launcher/shared/telemetry.mjs).
struct PlayerData {
  bool valid = false;
  int armor[4] = {0, 0, 0, 0};   // casco, peto, pantalon, botas; 0 = vacio
  int damage[4] = {0, 0, 0, 0};  // desgaste de cada pieza
  int health = 0;
  int x = 0;
  int y = 0;
  int z = 0;
  int facing = 4;  // rumbo: 0 sur, 1 oeste, 2 norte, 3 este, 4 quieto
  float speed = 0.0f;
  bool holdingBlock = false;
  int count = 0;  // objetos en la ranura seleccionada
  bool hurt = false;
};

struct Hit {
  unsigned long long time = 0;  // GetTickCount64 del ultimo golpe visto
  float distance = 0.0f;
  unsigned int serial = 0;
};

PlayerData playerData();
Hit lastHit();

// Vida de otros jugadores tal como la publica el servidor.
struct PeerHealth {
  bool seen = false;
  int lowest = -1;
  int highest = -1;
};
PeerHealth peerHealth();

// Cuenta muertes de otros jugadores vistas cerca.
unsigned int nearbyDeaths();
// Bajas tuyas: muertes a tu lado justo despues de atacar. Llamar cada fotograma.
void trackKills();
unsigned int confirmedKills();
unsigned long long lastKillTime();

// Hay un mundo en pantalla. Sin D3D12 no hay telemetria: se asume que si.
bool inWorld();
void setTelemetryAvailable(bool available);
// El HUD del juego (y por tanto su mira) se ha dibujado hace un instante.
bool crosshairVisible();

enum class Live { Atlas, Crosshair, Health, Estimate = Health + 41, Trail = Estimate + 41, HitColor = Trail + 6, FileParticle, Count = FileParticle + 8 };  // Health + vida
constexpr int kHealthSteps = 41;
constexpr unsigned kTrails = 6;  // canales de particulas
// Particulas de cosmeticos por archivos: 512 x (64 + canal), 8 canales.
constexpr int kFileParticleW = 512;
constexpr int kFileParticleH = 64;
constexpr unsigned kFileParticles = 8;
// Textura que el juego esta usando ahora, o nullptr.
ID3D12Resource* liveTexture(Live which);

bool installTextureHooks(ID3D12Device* device);

}  // namespace lex::platform::windows::game
