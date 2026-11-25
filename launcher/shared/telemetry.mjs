// Canal de datos juego -> cliente. Cada canal es un control de render que elige
// una textura segun una consulta Molang; el tamano de la textura codifica
// (canal, valor) y la DLL ve cual se enlaza. No depende de offsets de memoria.
// Debe coincidir con client/platform/windows/game/Telemetry.cpp.
export const BASE_W = 37;
export const BASE_H = 211;
export const VALUES = 10;

const digit = (expr, power) => `math.mod(math.floor((${expr}) / ${power}), 10)`;
const digits = (expr, n) => Array.from({ length: n }, (_, i) => digit(expr, 10 ** i));
const X = 'math.abs(math.floor(query.position(0)))';
const Y = 'math.clamp(math.floor(query.position(1)) + 100, 0, 999)';
const Z = 'math.abs(math.floor(query.position(2)))';
const HP = 'math.clamp(math.ceil(query.health), 0, 99)';
const SPEED = 'math.clamp(math.floor(query.ground_speed * 10), 0, 999)';
// Rumbo del movimiento: 0 sur, 1 oeste, 2 norte, 3 este, 4 quieto. Los giros no
// sirven: en primera persona el juego los entrega a cero.
const HEADING = '(math.abs(query.position_delta(0)) + math.abs(query.position_delta(2))) < 0.02 ? 4 : math.clamp(math.mod(math.floor((math.atan2(-query.position_delta(0), query.position_delta(2)) + 765) / 90), 4), 0, 3)';
const armor = (slot) => `math.clamp(query.armor_texture_slot(${slot}) + 1, 0, 9)`;
const damage = (slot) => digits(`math.clamp(query.armor_damage_slot(${slot}), 0, 999)`, 3);

export const CHANNELS = [
  armor(0), armor(1), armor(2), armor(3), // 0-3
  ...damage(0), ...damage(1), ...damage(2), ...damage(3), // 4-15
  ...digits(HP, 2), // 16-17
  'query.position(0) < 0 ? 1 : 0', // 18
  ...digits(X, 5), // 19-23
  ...digits(Y, 3), // 24-26
  'query.position(2) < 0 ? 1 : 0', // 27
  ...digits(Z, 5), // 28-32
  HEADING, // 33
  ...digits(SPEED, 3), // 34-36
  'query.is_carrying_block ? 1 : 0', // 37
  'query.hurt_time > 0 ? 1 : 0', // 38
];

// Cada control de render lleva GROUPS canales y publica uno por turno.
export const GROUPS = 6;
export const SLOT = `math.mod(math.floor(query.life_time * 40), ${GROUPS})`;

// Otros tamanos reservados (ancho fijo, alto = 300 + valor).
export const EXTRA_H = 300;
export const COUNT_W = 103; // objetos en la mano, 0-64
export const HUD_W = 104; // marcador: el HUD del juego esta en pantalla
export const REACH_W = 105; // jugador golpeado: distancia * 10, 0-79
export const PEER_W = 106; // vida que el juego conoce de otro jugador, 0-40
export const COUNTS = 65;
export const REACHES = 80;

// Texturas que la DLL repinta en vivo.
export const CROSSHAIR = 67; // cuadrada
export const HEALTH = { w: 131, rows: 41, rowH: 36 }; // una textura por valor: ancho 131 + vida
