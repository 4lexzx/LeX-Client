// Datos que LeX entrega al juego. Se generan a partir de la instalacion del
// usuario y se activan solos: no se distribuye ningun archivo de Mojang.
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { crc32, deflateSync } from 'node:zlib';
import { ATLAS, BONES, COSMETICS, DENSITY, FILE_DATA, TRAIL, CHANNELS_FX, CAPE_PLAIN, bonesOf } from '../shared/cosmetics.mjs';
import { PARTICLE_SIZE } from '../shared/filecosmetics.mjs';
import { dfxBones, hitAnimation } from '../shared/deathfx.mjs';
import { KILL_PARTICLES, choreography, cloneGeometry, killBones, tintOf } from '../shared/killfx.mjs';
import { BASE_H, BASE_W, CHANNELS, COUNTS, COUNT_W, CROSSHAIR, EXTRA_H, GROUPS, HEALTH, HIT_COLOR, HUD_W, KILL_W, PEER_W, REACHES, REACH_W, SLOT, VALUES } from '../shared/telemetry.mjs';

export const PACK_NAME = 'LeX_Cosmetics';
export const HEADER_ID = '5d1f0b7e-6c1a-4f0e-9a55-1e0c11e47a01';
const MODULE_ID = '5d1f0b7e-6c1a-4f0e-9a55-1e0c11e47a02';
const VISIBLE = 'query.is_local_player && (!variable.is_first_person || variable.is_paperdoll) && !variable.map_face_icon';

// Huesos base: mismos nombres que el jugador para heredar sus animaciones.
const SKELETON = [
  { name: 'root', pivot: [0, 0, 0] },
  { name: 'waist', parent: 'root', pivot: [0, 12, 0] },
  { name: 'body', parent: 'waist', pivot: [0, 24, 0] },
  { name: 'head', parent: 'body', pivot: [0, 24, 0] },
];

// --- .brarchive v1: cabecera de 16 bytes, entradas de 256 y luego los datos ---
function* entries(data) {
  if (data.length < 16 || data.readBigUInt64LE(0) !== 0x267052a0b125277dn) throw new Error('brarchive no reconocido');
  const count = data.readUInt32LE(8);
  const base = 16 + count * 256;
  if (base > data.length) throw new Error('brarchive truncado');
  for (let i = 0; i < count; i++) {
    const at = 16 + i * 256;
    const offset = base + data.readUInt32LE(at + 248);
    const size = data.readUInt32LE(at + 252);
    if (offset + size > data.length) throw new Error('entrada fuera de rango');
    yield [data.toString('utf8', at + 1, at + 1 + data[at]), data.subarray(offset, offset + size)];
  }
}

export function findEntry(data, wanted) {
  for (const [name, body] of entries(data)) if (name === wanted) return body;
  return null;
}

const vanillaDir = (gameDir) => join(gameDir, 'data', 'resource_packs', 'vanilla');

export function readPlayerEntity(gameDir) {
  const loose = join(vanillaDir(gameDir), 'entity', 'player.entity.json');
  const raw = existsSync(loose)
    ? readFileSync(loose)
    : findEntry(readFileSync(join(vanillaDir(gameDir), '__brarchive', 'entity.brarchive')), 'player.entity.json');
  if (!raw) throw new Error('esta versión no incluye la entidad del jugador');
  return JSON.parse(raw.toString('utf8'));
}

// Las particulas van compiladas; su identificador es la primera cadena con espacio de nombres.
export function readParticleIds(gameDir) {
  const path = join(vanillaDir(gameDir), '__brarchive', 'particles.brarchive');
  if (!existsSync(path)) return [];
  const ids = new Set();
  for (const [, body] of entries(readFileSync(path))) {
    const match = /minecraft:[a-z0-9_]+/.exec(body.toString('latin1'));
    if (match) ids.add(match[0]);
  }
  return [...ids].sort();
}

function chunk(type, body) {
  const out = Buffer.alloc(12 + body.length);
  out.writeUInt32BE(body.length, 0);
  out.write(type, 4, 'latin1');
  body.copy(out, 8);
  out.writeUInt32BE(crc32(out.subarray(4, 8 + body.length)), 8 + body.length);
  return out;
}

export function encodePng({ width, height, data }) {
  const head = Buffer.alloc(13);
  head.writeUInt32BE(width, 0);
  head.writeUInt32BE(height, 4);
  head[8] = 8;
  head[9] = 6; // RGBA
  const row = width * 4;
  const raw = Buffer.alloc(height * (row + 1));
  for (let y = 0; y < height; y++) Buffer.from(data.buffer, data.byteOffset + y * row, row).copy(raw, y * (row + 1) + 1);
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', head),
    chunk('IDAT', deflateSync(raw)),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

const blank = (width, height) => encodePng({ width, height, data: new Uint8ClampedArray(width * height * 4) });
const json = (value) => Buffer.from(JSON.stringify(value));

const LOCAL = 'query.is_local_player';
// Memoria de la fisica: velocidad, caida y giro suavizados del jugador local.
const PHYSICS_START = ['run', 'fall', 'turn', 'phase', 'sit', 'air', 'still'].map((v) => `variable.lex_${v} = 0;`);
const PHYSICS = [
  // Segundos sin moverte (las mascotas se duermen).
  `variable.lex_still = ${LOCAL} ? ((query.ground_speed > 0.3 || !query.is_on_ground || query.is_sneaking) ? 0 : variable.lex_still + query.delta_time) : 0;`,
  `variable.lex_air = ${LOCAL} ? variable.lex_air + ((query.is_on_ground ? 0 : 1) - variable.lex_air) * math.min(query.delta_time * 4, 1) : 0;`,
  `variable.lex_run = ${LOCAL} ? variable.lex_run + (math.min(query.ground_speed / 5.6, 1.4) - variable.lex_run) * math.min(query.delta_time * 5, 1) : 0;`,
  `variable.lex_fall = ${LOCAL} ? variable.lex_fall + (math.clamp(query.vertical_speed * -7, -14, 80) - variable.lex_fall) * math.min(query.delta_time * 6, 1) : 0;`,
  `variable.lex_turn = ${LOCAL} ? variable.lex_turn + (math.clamp(query.yaw_speed * 0.1, -20, 20) - variable.lex_turn) * math.min(query.delta_time * 5, 1) : 0;`,
  `variable.lex_phase = ${LOCAL} ? math.mod(variable.lex_phase + query.delta_time * (120 + variable.lex_run * 430), 36000) : 0;`,
  `variable.lex_sit = ${LOCAL} ? variable.lex_sit + ((variable.lex_run < 0.04 && math.mod(query.life_time, 16) > 7 ? 1 : 0) - variable.lex_sit) * math.min(query.delta_time * 3, 1) : 0;`,
];

// Vida de otros jugadores. Si el servidor la publica, se usa tal cual. Si no
// (siempre 20), se estima: cada golpe resta segun la armadura visible y se
// recupera como la regeneracion normal.
// Un golpe se detecta si sube hurt_time o si baja la vida (lo segundo funciona
// siempre en ti y en quien publica su vida). lex_calm: segundos desde el ultimo.
const PEER_START = ['variable.lex_real = 0;', 'variable.lex_est = 20;', 'variable.lex_ht = 0;', 'variable.lex_calm = 99;', 'variable.lex_hp = query.health;', 'variable.lex_hit = 0;'];
const worn = (slot, weight) => `(query.armor_texture_slot(${slot}) >= 0 ? ${weight} * (query.armor_texture_slot(${slot}) == 3 || query.armor_texture_slot(${slot}) == 6 ? 1.33 : 1) : 0)`;
const BLOW = `7 * (1 - (${worn(0, 0.08)} + ${worn(1, 0.24)} + ${worn(2, 0.2)} + ${worn(3, 0.08)}))`;
const PEER = [
  `variable.lex_real = ${LOCAL} ? 1 : (query.health > 0 && query.health < 20 ? 1 : variable.lex_real);`,
  'variable.lex_hit = (query.is_alive && (query.hurt_time > variable.lex_ht || query.health < variable.lex_hp - 0.01)) ? 1 : 0;',
  'variable.lex_calm = variable.lex_hit ? 0 : variable.lex_calm + query.delta_time;',
  `variable.lex_est = !query.is_alive ? 20 : (variable.lex_hit ? math.max(1, variable.lex_est - ${BLOW}) : (variable.lex_calm > 3 ? math.min(20, variable.lex_est + query.delta_time * 0.5) : variable.lex_est));`,
  'variable.lex_ht = query.hurt_time;',
  'variable.lex_hp = query.health;',
];

function animations(selected = {}) {
  const bones = {};
  // Cada tira sigue a la anterior con retraso: la onda baja por la capa.
  for (let i = 0; i < 8; i++) {
    const wave = `math.sin(variable.lex_phase - ${i * 50}) * (${(2 + i * 0.6).toFixed(1)} + variable.lex_run * ${(3 + i * 0.9).toFixed(1)})`;
    bones[`lexCape${i}`] = {
      rotation: i === 0
        ? [`6 + variable.lex_run * 36 + variable.lex_fall + (query.is_sneaking ? 16 : 0) + ${wave}`, 0, 'variable.lex_turn']
        : [`${wave} - variable.lex_run * 3 + variable.lex_fall * 0.05`, 0, 'variable.lex_turn * 0.2'],
    };
  }
  bones.lexPlain0 = { rotation: ['6 + 62 * query.cape_flap_amount + (query.is_sneaking ? 18 : 0)', 0, 0] };
  // Alas vivas: en reposo respiran despacio, al andar baten algo mas y en el
  // aire aletean fuerte. Las frecuencias son fijas (multiplicar el tiempo por
  // una velocidad que cambia hace saltar la fase); solo cambia la amplitud.
  const wave = (lag) => `(math.sin(query.life_time * 95 - ${lag}) * (14 + variable.lex_run * 8) * (1 - variable.lex_air) + math.sin(query.life_time * 330 - ${lag * 1.6}) * 30 * variable.lex_air)`;
  const flap = wave(0);
  const tip = wave(40);
  const end = wave(85);
  for (const kind of ['Wing', 'Dragon']) {
    bones[`lex${kind}L`] = { rotation: [`6 + variable.lex_run * 6`, `-26 - ${flap}`, 8] };
    bones[`lex${kind}L2`] = { rotation: [`math.sin(query.life_time * 95 - 30) * 5`, `-12 - ${tip} * 0.9`, 0] };
    bones[`lex${kind}L3`] = { rotation: [`math.sin(query.life_time * 95 - 70) * 6`, `-10 - ${end} * 0.8`, 0] };
    bones[`lex${kind}R`] = { rotation: [`6 + variable.lex_run * 6`, `26 + ${flap}`, -8] };
    bones[`lex${kind}R2`] = { rotation: [`math.sin(query.life_time * 95 - 30) * 5`, `12 + ${tip} * 0.9`, 0] };
    bones[`lex${kind}R3`] = { rotation: [`math.sin(query.life_time * 95 - 70) * 6`, `10 + ${end} * 0.8`, 0] };
  }
  bones.lexHalo = { position: [0, 'math.sin(query.life_time * 120) * 0.4', 0], rotation: [-8, 'query.life_time * 30', 0] };
  // Por encima del casco cuando se lleva uno.
  const helmet = 'query.armor_texture_slot(0) >= 0 ? 1.13 : 1';
  for (const name of ['lexBand', 'lexGoggles', 'lexCrown']) bones[name] = { scale: [helmet, 1, helmet] };
  bones.lexBandA = { rotation: ['10 + variable.lex_run * 52 + math.sin(variable.lex_phase) * (3 + variable.lex_run * 12)', 0, 'variable.lex_turn'] };
  bones.lexBandB = { rotation: ['8 + variable.lex_run * 46 + math.sin(variable.lex_phase + 90) * (3 + variable.lex_run * 12)', 0, 'variable.lex_turn'] };
  bones.lexAura = { rotation: [0, 'query.life_time * 36', 0] };
  bones.lexAuraWall = { rotation: [0, 'query.life_time * -22', 0] };
  bones.lexAuraOrbs = { position: [0, 'math.sin(query.life_time * 90) * 1.2', 0], rotation: [0, 'query.life_time * 70', 0] };
  // Mascotas voladoras: rondan el hombro, se retrasan al correr y se inclinan.
  const hover = (amp, speed) => ({
    position: [`math.sin(query.life_time * 47) * ${amp} + math.sin(query.life_time * 23) * ${amp}`, `math.sin(query.life_time * ${speed}) * 1.4 + variable.lex_fall * 0.05`, `variable.lex_run * 9 + math.cos(query.life_time * 37) * ${amp}`],
    rotation: ['variable.lex_run * 16', 'math.cos(query.life_time * 47) * 18', 'variable.lex_turn * -1.2 + math.cos(query.life_time * 47) * 6'],
  });
  // Dragon: cada 22 s da una vuelta a tu alrededor (5-9 s) y escupe fuego (13-15.5 s).
  const cyc = 'math.mod(query.life_time, 22)';
  const lap = `math.clamp((${cyc} - 5) / 4, 0, 1)`;
  const ang = `(${lap} * ${lap} * (3 - 2 * ${lap}) * 360)`;
  const breath = `(${cyc} > 13 && ${cyc} < 15.5)`;
  const base = hover(2.2, 90);
  bones.lexPetDragon = {
    position: [
      `${base.position[0]} - 19 * (1 - math.cos(${ang}))`,
      `${base.position[1]} + math.sin(${ang} * 0.5) * 7`,
      `${base.position[2]} + 19 * math.sin(${ang})`,
    ],
    rotation: [`${base.rotation[0]} + (${breath} ? -10 : 0)`, `${base.rotation[1]} - ${ang}`, `${base.rotation[2]} - math.sin(${ang}) * 25`],
  };
  bones.lexPetDragon_breath = { scale: `${breath} ? 0.85 + math.sin(query.life_time * 2200) * 0.12 + math.clamp((${cyc} - 13) * 4, 0, 1) * 0.4 : 0` };
  bones.lexPetParrot = hover(2, 150);
  bones.lexPetBee = hover(2.6, 190);
  bones.lexPetAllay = hover(1.8, 110);
  bones.lexPetSpirit = hover(1.6, 130);
  // Aleteo tranquilo y constante; al correr solo abre un poco mas.
  const beat = 'query.life_time * 210';
  for (const [side, k] of [['R', 1], ['L', -1]]) {
    bones[`lexPetDragon_wing${side}`] = { rotation: [0, `${k * 8}`, `${k} * (-8 + math.sin(${beat}) * (26 + variable.lex_run * 6))`] };
    bones[`lexPetDragon_tip${side}`] = { rotation: [0, 0, `${k} * (6 + math.sin(${beat} - 55) * (34 + variable.lex_run * 6))`] };
    bones[`lexPetDragon_front${side}`] = { rotation: ['58 + math.sin(query.life_time * 120) * 5', 0, 0] };
    bones[`lexPetDragon_fronttip${side}`] = { rotation: [-34, 0, 0] };
    bones[`lexPetDragon_frontfoot${side}`] = { rotation: [36, 0, 0] };
    bones[`lexPetDragon_rear${side}`] = { rotation: ['62 + math.sin(query.life_time * 120 + 60) * 5', 0, 0] };
    bones[`lexPetDragon_reartip${side}`] = { rotation: [28, 0, 0] };
    bones[`lexPetDragon_rearfoot${side}`] = { rotation: [38, 0, 0] };
  }
  for (let i = 0; i < 5; i++) bones[`lexPetDragon_neck${i}`] = { rotation: [`math.sin(query.life_time * 80 - ${i * 30}) * 4 - variable.lex_run * 2`, `math.sin(query.life_time * 53 - ${i * 24}) * 5 + variable.lex_turn * 0.5`, 0] };
  bones.lexPetDragon_head = { rotation: ['math.sin(query.life_time * 70) * 6 + 6', 'math.sin(query.life_time * 53) * 12', 0] };
  bones.lexPetDragon_jaw = { rotation: [`${breath} ? 34 : 8 + math.max(0, math.sin(query.life_time * 60)) * 14`, 0, 0] };
  bones.lexPetDragon_head.rotation[0] = `${breath} ? 14 : ${bones.lexPetDragon_head.rotation[0]}`;
  for (let i = 0; i < 10; i++) bones[`lexPetDragon_tail${i}`] = { rotation: [`math.sin(query.life_time * 100 - ${i * 32}) * 5`, `math.sin(query.life_time * 120 - ${i * 36}) * 8 - variable.lex_turn * 0.6`, 0] };
  bones.lexPetParrot_wing0 = { rotation: [0, 0, 'math.sin(query.life_time * 900) * 46 - 52'] };
  bones.lexPetParrot_wing1 = { rotation: [0, 0, 'math.sin(query.life_time * 900) * -46 + 52'] };
  bones.lexPetParrot_head = { rotation: ['math.sin(query.life_time * 90) * 8', 'math.sin(query.life_time * 60) * 26', 'math.sin(query.life_time * 43) * 10'] };
  bones.lexPetParrot_body = { rotation: [24, 0, 0] };
  bones.lexPetParrot_tail = { rotation: ['30 + math.sin(query.life_time * 140) * 8', 0, 0] };
  bones.lexPetBee_rightwing_bone = { rotation: [0, 0, 'math.sin(query.life_time * 2400) * 34'] };
  bones.lexPetBee_leftwing_bone = { rotation: [0, 0, 'math.sin(query.life_time * 2400) * -34'] };
  bones.lexPetAllay_left_wing = { rotation: [0, 'math.sin(query.life_time * 700) * 28 + 40', 0] };
  bones.lexPetAllay_right_wing = { rotation: [0, 'math.sin(query.life_time * 700) * -28 - 40', 0] };
  bones.lexPetAllay_head = { rotation: ['math.sin(query.life_time * 70) * 6', 'math.sin(query.life_time * 47) * 16', 'math.sin(query.life_time * 90) * 6'] };
  bones.lexPetAllay_body = { rotation: ['10 + variable.lex_run * 24', 0, 0] };
  bones.lexPetAllay_right_arm = { rotation: ['-20 + math.sin(query.life_time * 100) * 14', 0, 8] };
  bones.lexPetAllay_left_arm = { rotation: ['-20 - math.sin(query.life_time * 100) * 14', 0, -8] };
  // Mascotas de suelo: trotan contigo, pasean y se sientan si paras.
  const step = (shift) => `math.sin(variable.lex_phase * 1.6 + ${shift}) * math.min(variable.lex_run, 1) * 48`;
  for (const pet of ['Cat', 'Fox']) {
    const root = `lexPet${pet}`;
    bones[root] = {
      position: ['math.sin(query.life_time * 21) * 2.5 * (1 - variable.lex_sit)', `math.abs(math.sin(variable.lex_phase * 1.6)) * variable.lex_run * 1.2 - variable.lex_sit * 0.4`, 'variable.lex_run * 10 + math.sin(query.life_time * 17) * 4 * (1 - variable.lex_sit)'],
      rotation: [0, 'math.cos(query.life_time * 21) * 14 * (1 - variable.lex_sit) - variable.lex_turn', 0],
    };
    bones[`${root}_body`] = { rotation: ['variable.lex_sit * -26 + math.sin(query.life_time * 90) * 1', 0, 0], position: [0, 'variable.lex_sit * -0.6', 'variable.lex_sit * 1.5'] };
    bones[`${root}_head`] = { rotation: ['variable.lex_sit * 24 + math.sin(query.life_time * 55) * 5', 'math.sin(query.life_time * 37) * 24', 'math.sin(query.life_time * 29) * 6'] };
  }
  const legs = (root, a, b, c, d) => {
    bones[`${root}_${a}`] = { rotation: [`${step(0)} + variable.lex_sit * 26`, 0, 0] };
    bones[`${root}_${b}`] = { rotation: [`${step(180)} + variable.lex_sit * 26`, 0, 0] };
    bones[`${root}_${c}`] = { rotation: [`${step(180)} + variable.lex_sit * -42`, 0, 0] };
    bones[`${root}_${d}`] = { rotation: [`${step(0)} + variable.lex_sit * -42`, 0, 0] };
  };
  legs('lexPetCat', 'frontLegL', 'frontLegR', 'backLegL', 'backLegR');
  legs('lexPetFox', 'leg3', 'leg2', 'leg1', 'leg0');
  bones.lexPetCat_tail1 = { rotation: ['62 - variable.lex_sit * 20 + math.sin(query.life_time * 80) * 8', 'math.sin(query.life_time * 70) * 16', 0] };
  bones.lexPetCat_tail2 = { rotation: ['26 + math.sin(query.life_time * 110) * 18', 'math.sin(query.life_time * 95) * 20', 0] };
  bones.lexPetFox_tail = { rotation: ['78 + math.sin(query.life_time * 90) * 8', 'math.sin(query.life_time * 120) * 18', 0] };
  bones.lexPetSpiritTail = { rotation: ['variable.lex_run * 30 + math.sin(query.life_time * 160) * 10', 0, 'math.sin(query.life_time * 120) * 10'] };
  bones.lexPetSpiritOrbs = { rotation: ['math.sin(query.life_time * 60) * 20', 'query.life_time * 130', 0] };
  const mini = `(math.sin(query.life_time * 160) * (18 + variable.lex_run * 10) * (1 - variable.lex_air) + math.sin(query.life_time * 480) * 34 * variable.lex_air)`;
  bones.lexMiniL = { rotation: [0, `-28 - ${mini}`, 4] };
  bones.lexMiniR = { rotation: [0, `28 + ${mini}`, -4] };
  // Huesos que describen su propio movimiento (auras de objetos).
  const term = (w) => {
    if (!Array.isArray(w)) return w;
    const [base, amp, speed, phase, lin] = w;
    let out = String(base);
    if (amp) out += ` + ${amp} * math.sin(query.life_time * ${speed} + ${phase})`;
    if (lin) out += ` + query.life_time * ${lin}`;
    return out;
  };
  for (const b of BONES) {
    if (bones[b.name] || (!b.motion && !b.molang)) continue;
    const m = b.molang ?? {};
    const out = {};
    if (m.rotation) out.rotation = m.rotation;
    else if (b.motion?.rot) out.rotation = b.motion.rot.map(term);
    if (m.position) out.position = m.position;
    else if (b.motion?.pos) out.position = b.motion.pos.map(term);
    if (Object.keys(out).length) bones[b.name] = out;
  }
  return {
    format_version: '1.8.0',
    animations: {
      'animation.lex.idle': { loop: true, bones },
      // La placa de vida mira siempre a la camara.
      ...FILE_DATA.animations,
      'animation.lex.dfx': choreography(selected.death),
      'animation.lex.kfx': choreography(selected.kill),
      'animation.lex.hfx': hitAnimation('lexDfx'),
      'animation.lex.health': { loop: true, bones: { lexHealth: { rotation: ['-query.camera_rotation(0)', '180 + query.camera_rotation(1) - query.body_y_rotation', 0] } } },
    },
  };
}

// Canales de particulas. La posicion del jugador es la de sus ojos, asi que el
// origen se baja para que salgan de los pies o del cuerpo.
const FX = {
  rise: { at: -1.5, radius: 0.34, speed: 0.45, lift: 0.9, size: 0.17, life: 1.3, rate: 46 },
  fall: { at: -1.0, radius: 0.4, speed: 0.6, lift: -1.1, size: 0.14, life: 1.4, rate: 46 },
  burst: { at: -1.35, radius: 0.3, speed: 1.1, lift: 0.3, size: 0.16, life: 1.2, rate: 46 },
  hit: { at: -0.5, radius: 0.25, speed: 3.2, lift: -3, size: 0.15, life: 0.6, count: 16 },
  death: { at: -0.9, radius: 0.3, speed: 4.2, lift: -2.5, size: 0.22, life: 1.3, count: 46 },
  soul: { at: -0.9, radius: 0.45, speed: 0.7, lift: 1.6, size: 0.22, life: 1.8, count: 34 },
};
function particle(key, index) {
  const f = FX[key];
  return {
    format_version: '1.10.0',
    particle_effect: {
      description: { identifier: `lex:fx_${key}`, basic_render_parameters: { material: 'particles_blend', texture: `textures/lex/p/${index}` } },
      components: {
        ...(f.count
          ? { 'minecraft:emitter_rate_instant': { num_particles: f.count }, 'minecraft:emitter_lifetime_once': { active_time: 0.4 } }
          : { 'minecraft:emitter_rate_steady': { spawn_rate: f.rate, max_particles: 90 }, 'minecraft:emitter_lifetime_looping': { active_time: 1 } }),
        'minecraft:emitter_shape_sphere': { offset: [0, f.at, 0], radius: f.radius, direction: 'outwards' },
        'minecraft:particle_initial_speed': `${f.speed} * math.random(0.4, 1)`,
        'minecraft:particle_lifetime_expression': { max_lifetime: `${f.life} * math.random(0.6, 1.1)` },
        'minecraft:particle_motion_dynamic': { linear_acceleration: ['math.random(-0.5, 0.5)', f.lift, 'math.random(-0.5, 0.5)'], linear_drag_coefficient: 1.3 },
        'minecraft:particle_appearance_billboard': {
          size: [`${f.size} * (0.7 + variable.particle_random_1 * 0.5)`, `${((f.size * 36) / 33).toFixed(3)} * (0.7 + variable.particle_random_1 * 0.5)`], facing_camera_mode: 'rotate_xyz',
          uv: { texture_width: TRAIL.w, texture_height: TRAIL.h + index, flipbook: { base_UV: [0, 0], size_UV: [33, 36], step_UV: [33, 0], frames_per_second: 8, max_frame: 8, stretch_to_lifetime: true, loop: false } },
        },
      },
    },
  };
}

export function buildPack(entity, options) {
  const selected = { kill: options.loadout?.kill?.replace(/^kill-/, '') ?? null, death: options.loadout?.death?.replace(/^death-/, '') ?? null };
  const { version, icon = null, particleIds = [], telemetry = true, disabledParticles = false } = options;
  const out = structuredClone(entity);
  const d = out['minecraft:client_entity'].description;
  const files = new Map();
  const controllers = {};
  const show = (id, controller, when) => {
    controllers[`controller.render.${id}`] = controller;
    d.render_controllers.push({ [`controller.render.${id}`]: when });
  };

  // El cuerpo normal se oculta mientras el doble hace la animacion de muerte.
  d.render_controllers = d.render_controllers.map((rc) => (typeof rc === 'string'
    ? { [rc]: '!variable.lex_fxon' }
    : Object.fromEntries(Object.entries(rc).map(([name, when]) => [name, `(${when}) && !variable.lex_fxon`]))));
  d.materials.lex = 'entity_alphatest';
  d.textures.lex = 'textures/lex/atlas';
  d.geometry.lex = 'geometry.lex.cosmetics';
  // Solo el jugador local paga estas animaciones: en un servidor lleno, hacerlo
  // para todos costaba FPS.
  d.animations.lex = 'animation.lex.idle';
  d.scripts.animate.push({ lex: VISIBLE });
  d.scripts.initialize = [...(d.scripts.initialize ?? []), ...PHYSICS_START];
  d.scripts.pre_animation = [...(d.scripts.pre_animation ?? []), ...PHYSICS];
  // Solo en tercera persona: en primera se colaban delante de la camara.
  const running = `${LOCAL} && query.is_sprinting && query.is_on_ground && !variable.is_first_person && !variable.is_paperdoll`;
  const others = options.healthOnSelf ? '1' : `!${LOCAL}`;
  d.animations.lex_trail = 'controller.animation.lex.trail';
  d.animations.lex_fx = 'controller.animation.lex.fx';
  // Lo de otros jugadores solo cerca: en un servidor lleno ahorra trabajo.
  const nearOthers = options.healthOnSelf ? '1' : `!${LOCAL} && query.distance_from_camera < 28`;
  d.scripts.animate.push({ lex_trail: LOCAL }, { lex_fx: nearOthers });
  d.animations.lex_dfx = 'controller.animation.lex.dfx';
  d.animations.lex_dfx_play = 'animation.lex.dfx';
  d.animations.lex_kfx_play = 'animation.lex.kfx';
  d.animations.lex_hfx_play = 'animation.lex.hfx';
  const near = `(${LOCAL} || query.distance_from_camera < 24)`;
  d.scripts.animate.push({ lex_dfx: near });
  // Cosmeticos por archivos: sus animaciones, un controlador de estados cada
  // uno (solo para ti) y sus particulas, con textura que pinta la DLL.
  Object.assign(d.animations, FILE_DATA.entity.animations);
  for (const ctl of FILE_DATA.entity.animate) d.scripts.animate.push({ [ctl]: VISIBLE });
  d.particle_effects = { ...d.particle_effects, ...FILE_DATA.entity.particles };
  for (const p of FILE_DATA.particles) {
    files.set(`particles/lexf_${p.id}_${p.name}.json`, json(p.json));
    files.set(`textures/lex/fp/${p.channel}.png`, blank(PARTICLE_SIZE.w, PARTICLE_SIZE.h + p.channel));
  }
  d.scripts.initialize.push(...PEER_START, 'variable.lex_dt = 0;', 'variable.lex_mine = 0;', 'variable.lex_fxon = 0;');
  d.scripts.pre_animation.push(...PEER,
    // Segundos muerto; si la baja es tuya (murio a tu lado o justo tras un golpe);
    // si la animacion elegida esta en marcha.
    'variable.lex_dt = query.is_alive ? 0 : variable.lex_dt + query.delta_time;',
    'variable.lex_mine = query.is_alive ? 0 : (variable.lex_dt < 0.1 ? ((query.distance_from_camera < 6 || (variable.lex_calm < 3 && query.distance_from_camera < 10)) ? 1 : variable.lex_mine) : variable.lex_mine);',
    `variable.lex_fxon = (!query.is_alive && variable.lex_dt < 6.6 && (query.is_local_player ? ${selected.death ? 1 : 0} : ${selected.kill ? 1 : 0} * variable.lex_mine)) ? 1 : 0;`);
  d.particle_effects = { ...(d.particle_effects ?? {}), ...Object.fromEntries(CHANNELS_FX.map((k) => [`lex_${k}`, `lex:fx_${k}`])) };
  CHANNELS_FX.forEach((key, i) => {
    files.set(`textures/lex/p/${i}.png`, blank(TRAIL.w, TRAIL.h + i));
    files.set(`particles/lex_fx_${key}.json`, json(particle(key, i)));
  });
  files.set('animation_controllers/lex.animation_controllers.json', json({
    format_version: '1.10.0',
    animation_controllers: {
      ...FILE_DATA.controllers,
      'controller.animation.lex.trail': {
        initial_state: 'still',
        states: {
          still: { transitions: [{ run: running }] },
          run: { particle_effects: ['rise', 'fall', 'burst'].map((k) => ({ effect: `lex_${k}` })), transitions: [{ still: `!(${running})` }] },
        },
      },
      // Animacion 3D de muerte: arranca al morir y se queda en su final.
      'controller.animation.lex.dfx': {
        initial_state: 'alive',
        states: {
          alive: { transitions: [{ dead: '!query.is_alive' }] },
          dead: { animations: [{ lex_dfx_play: 'query.is_local_player' }, { lex_kfx_play: '!query.is_local_player' }], transitions: [{ alive: 'query.is_alive' }] },
        },
      },
      'controller.animation.lex.fx': {
        initial_state: 'calm',
        states: {
          calm: { transitions: [{ dead: '!query.is_alive' }, { hit: 'variable.lex_calm < 0.1 && query.distance_from_camera < 16' }] },
          hit: { animations: ['lex_hfx_play'], transitions: [{ dead: '!query.is_alive' }, { calm: 'variable.lex_calm > 0.4' }] },
          dead: { transitions: [{ calm: 'query.is_alive' }] },
        },
      },
    },
  }));
  files.set('textures/lex/atlas.png', blank(ATLAS.w * DENSITY, ATLAS.h * DENSITY));
  show('lex', {
    rebuild_animation_matrices: true,
    geometry: 'Geometry.lex',
    materials: [{ '*': 'Material.lex' }],
    textures: ['Texture.lex'],
  }, VISIBLE);

  // Al morir tu: delante de ti, para verlo tambien en primera persona.
  // Sobre otro jugador: solo se ve si la DLL confirma que la baja es tuya.
  d.materials.lex_blend = 'entity_alphablend';
  d.geometry.lex_dfx = 'geometry.lex.deathfx';
  d.geometry.lex_kfx = 'geometry.lex.killfx';
  d.geometry.lex_hfx = 'geometry.lex.hitfx';
  // Golpes: sobre otros jugadores (o sobre ti en el modo de prueba).
  const hitOn = options.healthOnSelf ? '1' : `!${LOCAL}`;
  // Doble del jugador con su propia skin para la coreografia de muerte o baja;
  // mientras tanto el cuerpo normal no se dibuja.
  d.materials.lex_skin = 'entity_alphatest';
  d.geometry.lex_clone = 'geometry.lex.clone';
  d.geometry.lex_cloneself = 'geometry.lex.cloneself';
  const tintK = tintOf(selected.kill);
  const tintD = tintOf(selected.death);
  const tint = (ch) => `query.is_local_player ? (${tintD[ch].replace(/\bt\b/g, 'variable.lex_dt')}) : (${tintK[ch].replace(/\bt\b/g, 'variable.lex_dt')})`;
  show('lex_clone', {
    rebuild_animation_matrices: true,
    arrays: { geometries: { 'Array.c': ['Geometry.lex_clone', 'Geometry.lex_cloneself'] } },
    geometry: `Array.c[${LOCAL} ? 1 : 0]`,
    materials: [{ '*': 'Material.lex_skin' }],
    textures: ['Texture.default'],
    overlay_color: { r: tint('r'), g: tint('g'), b: tint('b'), a: tint('a') },
  }, `variable.lex_fxon && (${LOCAL} || query.distance_from_camera < 24)`);
  for (const [name, p] of Object.entries(KILL_PARTICLES)) {
    d.particle_effects[`lex_k_${name}`] = `lex:k_${name}`;
    files.set(`particles/lex_k_${name}.json`, json(p));
    if (options.fxTextures?.[name]) files.set(`textures/lex/kfx/${name}.png`, options.fxTextures[name]);
  }
  show('lex_dfx', {
    rebuild_animation_matrices: true,
    arrays: { geometries: { 'Array.g': ['Geometry.lex_kfx', 'Geometry.lex_dfx', 'Geometry.lex_hfx'] } },
    geometry: `Array.g[query.is_alive ? 2 : (${LOCAL} ? 1 : 0)]`,
    materials: [{ '*': 'Material.lex_blend' }],
    textures: ['Texture.lex'],
  }, `variable.lex_fxon || (query.is_alive && ${hitOn} && variable.lex_calm < 0.4 && query.distance_from_camera < 20)`);
  const fxGeometry = (identifier, bones) => ({
    description: { identifier, texture_width: ATLAS.w, texture_height: ATLAS.h, visible_bounds_width: 10, visible_bounds_height: 12, visible_bounds_offset: [0, 4, 0] },
    bones,
  });
  // Solo la animacion elegida va en el pack (se elige al pulsar Jugar).
  const SELF_DZ = -40;
  const geometry = [
    fxGeometry('geometry.lex.deathfx', killBones('lexDfx', [selected.death].filter(Boolean), SELF_DZ, true)),
    fxGeometry('geometry.lex.killfx', killBones('lexDfx', [selected.kill].filter(Boolean))),
    fxGeometry('geometry.lex.hitfx', dfxBones('lexDfx', 'hit', 0)),
    cloneGeometry('geometry.lex.clone'),
    cloneGeometry('geometry.lex.cloneself', SELF_DZ, true), {
    description: {
      identifier: 'geometry.lex.cosmetics', texture_width: ATLAS.w, texture_height: ATLAS.h,
      visible_bounds_width: 5, visible_bounds_height: 5, visible_bounds_offset: [0, 1.5, 0],
    },
    bones: [...SKELETON, ...BONES, ...(options.extraBones ?? [])],
  }];

  if (telemetry) {
    d.geometry.lex_t = 'geometry.lex.telemetry';
    geometry.push({
      description: { identifier: 'geometry.lex.telemetry', texture_width: 16, texture_height: 16, visible_bounds_width: 5, visible_bounds_height: 5, visible_bounds_offset: [0, 1.5, 0] },
      bones: [{ name: 'lexTelemetry', pivot: [0, 12, 0], cubes: [{ origin: [0, 12, 0], size: [0.02, 0.02, 0.02], uv: [0, 0] }] }],
    });
    const names = CHANNELS.map((_, ch) => Array.from({ length: VALUES }, (_u, v) => {
      const key = `lex_t${ch}_${v}`;
      d.textures[key] = `textures/lex/t/${ch}_${v}`;
      files.set(`textures/lex/t/${ch}_${v}.png`, blank(BASE_W + ch, BASE_H + v));
      return `Texture.${key}`;
    }));
    const telemetryIf = `${LOCAL} && !variable.is_paperdoll && !variable.map_face_icon`;
    const channel = (id, array, index) => show(id, {
      geometry: 'Geometry.lex_t',
      materials: [{ '*': 'Material.lex' }],
      arrays: { textures: { 'Array.v': array } },
      textures: [`Array.v[${index}]`],
    }, telemetryIf);
    // Un control lleva varios canales y entrega uno por turno: menos trabajo
    // por cada jugador del servidor.
    const last = CHANNELS.length - 1; // dano recibido: en cada fotograma
    for (let first = 0; first < last; first += GROUPS) {
      const part = CHANNELS.slice(first, Math.min(first + GROUPS, last));
      const index = part.reduceRight((rest, expr, i) => (rest === null ? `${i * VALUES} + (${expr})` : `(${SLOT} == ${i} ? ${i * VALUES} + (${expr}) : (${rest}))`), null);
      channel(`lex_t${first}`, names.slice(first, first + part.length).flat(), index);
    }
    channel(`lex_t${last}`, names[last], CHANNELS[last]);

    // Que vida publica el servidor de los demas jugadores (para avisar si no la envia).
    const peer = [];
    for (let i = 0; i < HEALTH.rows; i++) {
      d.textures[`lex_v${i}`] = `textures/lex/t/v${i}`;
      files.set(`textures/lex/t/v${i}.png`, blank(PEER_W, EXTRA_H + i));
      peer.push(`Texture.lex_v${i}`);
    }
    show('lex_peer', {
      geometry: 'Geometry.lex_t',
      materials: [{ '*': 'Material.lex' }],
      arrays: { textures: { 'Array.v': peer } },
      textures: [`Array.v[math.clamp(math.ceil(query.health), 0, ${HEALTH.rows - 1})]`],
    }, `!${LOCAL} && query.distance_from_camera < 32 && math.mod(query.life_time, 2) < 0.08`);

    // Golpe a otro jugador: distancia en decimas de bloque.
    const reach = [];
    for (let i = 0; i < REACHES; i++) {
      d.textures[`lex_r${i}`] = `textures/lex/t/r${i}`;
      files.set(`textures/lex/t/r${i}.png`, blank(REACH_W, EXTRA_H + i));
      reach.push(`Texture.lex_r${i}`);
    }
    show('lex_reach', {
      geometry: 'Geometry.lex_t',
      materials: [{ '*': 'Material.lex' }],
      arrays: { textures: { 'Array.v': reach } },
      textures: [`Array.v[math.clamp(math.floor(query.distance_from_camera * 10), 0, ${REACHES - 1})]`],
    }, '!query.is_local_player && variable.lex_calm < 0.1');

    // Vida de otros jugadores (solo minecraft:player, nunca criaturas).
    d.geometry.lex_hp = 'geometry.lex.health';
    const plates = [];
    for (let i = 0; i < HEALTH.rows; i++) {
      d.textures[`lex_h${i}`] = `textures/lex/t/h${i}`;
      files.set(`textures/lex/t/h${i}.png`, blank(HEALTH.w + i, HEALTH.rowH));
      plates.push(`Texture.lex_h${i}`);
    }
    for (let i = 0; i < HEALTH.rows; i++) {
      d.textures[`lex_e${i}`] = `textures/lex/t/e${i}`;
      files.set(`textures/lex/t/e${i}.png`, blank(HEALTH.estimate + i, HEALTH.rowH));
      plates.push(`Texture.lex_e${i}`);
    }
    const face = { uv: [0, 0], uv_size: [HEALTH.w, HEALTH.rowH] };
    geometry.push({
      description: { identifier: 'geometry.lex.health', texture_width: HEALTH.w, texture_height: HEALTH.rowH, visible_bounds_width: 4, visible_bounds_height: 5, visible_bounds_offset: [0, 1.5, 0] },
      bones: [{
        name: 'lexHealth', pivot: [0, 49, 0],
        cubes: [{ origin: [-7, 47, 0], size: [14, 4, 0], uv: { north: face } }],
      }],
    });
    d.animations.lex_hp = 'animation.lex.health';
    const plateIf = options.healthOnSelf ? '!variable.is_first_person && !query.is_in_ui' : '!query.is_local_player && query.is_alive && !query.is_in_ui && !query.is_invisible && query.distance_from_camera < 28';
    d.scripts.animate.push({ lex_hp: nearOthers });
    show('lex_health', {
      rebuild_animation_matrices: true,
      geometry: 'Geometry.lex_hp',
      materials: [{ '*': 'Material.lex' }],
      arrays: { textures: { 'Array.v': plates } },
      textures: [`Array.v[variable.lex_real ? math.clamp(math.ceil(query.health), 0, ${HEALTH.rows - 1}) : ${HEALTH.rows} + math.clamp(math.ceil(variable.lex_est), 0, 20)]`],
    }, plateIf);

    // Color de golpe: la misma piel encima, pintada por la DLL.
    d.textures.lex_hc = 'textures/lex/t/hc';
    files.set('textures/lex/t/hc.png', blank(HIT_COLOR, HIT_COLOR));
    show('lex_hitcolor', {
      geometry: 'Geometry.default',
      materials: [{ '*': 'Material.lex_blend' }],
      textures: ['Texture.lex_hc'],
      is_hurt_color: { r: 0, g: 0, b: 0, a: 0 },
    }, `${options.healthOnSelf ? '!variable.is_first_person' : `!${LOCAL}`} && variable.lex_calm < 0.45 && !query.is_invisible`);

    // Muerte cercana: la DLL decide si fue tuya.
    // El alto lleva la distancia en bloques: la DLL solo cuenta muertes a tu lado.
    const kills = [];
    for (let i = 0; i < 16; i++) {
      d.textures[`lex_k${i}`] = `textures/lex/t/k${i}`;
      files.set(`textures/lex/t/k${i}.png`, blank(KILL_W, EXTRA_H + i));
      kills.push(`Texture.lex_k${i}`);
    }
    show('lex_kill', { geometry: 'Geometry.lex_t', materials: [{ '*': 'Material.lex' }], arrays: { textures: { 'Array.v': kills } }, textures: ['Array.v[math.clamp(math.floor(query.distance_from_camera), 0, 15)]'] },
      `!${LOCAL} && !query.is_alive && query.distance_from_camera < 14`);

    // HUD: marcador de pantalla y cantidad del objeto seleccionado.
    files.set('textures/lex/n/hud.png', blank(HUD_W, EXTRA_H));
    for (let i = 0; i < COUNTS; i++) files.set(`textures/lex/n/${i}.png`, blank(COUNT_W, EXTRA_H + i));
    const insert = (control) => ({ modifications: [{ array_name: 'controls', operation: 'insert_back', value: control }] });
    files.set('ui/hud_screen.json', json({
      namespace: 'hud',
      lex_marker: { type: 'image', texture: 'textures/lex/n/hud', size: [1, 1], layer: 1, alpha: 1 },
      lex_count: {
        type: 'image', size: [1, 1], layer: 1,
        texture: '#texture',
        bindings: [
          { binding_name: '#inventory_stack_count', binding_type: 'collection', binding_collection_name: 'hotbar_items' },
          { binding_name: '#slot_selected', binding_type: 'collection', binding_collection_name: 'hotbar_items' },
          { binding_name: '#item_id_aux', binding_type: 'collection', binding_collection_name: 'hotbar_items' },
          // Los bloques tienen identificador menor que 256 (o negativo); se guarda por 65536.
          { binding_type: 'view', source_property_name: '(#slot_selected and (#item_id_aux < 16777216) and not (#item_id_aux = 0))', target_property_name: '#visible' },
          { binding_type: 'view', source_property_name: "('textures/lex/n/' + #inventory_stack_count)", target_property_name: '#texture' },
        ],
      },
      root_panel: insert({ 'lex_marker@hud.lex_marker': {} }),
      gui_hotbar_grid_item: insert({ 'lex_count@hud.lex_count': {} }),
    }));
    files.set('textures/ui/cross_hair.png', options.crosshair ?? blank(CROSSHAIR, CROSSHAIR));
  }

  if (disabledParticles) {
    particleIds.forEach((id, i) => files.set(`particles/lex_${i}.json`, json({
      format_version: '1.10.0',
      particle_effect: {
        description: { identifier: id, basic_render_parameters: { material: 'particles_alpha', texture: 'textures/particle/particles' } },
        components: {
          'minecraft:emitter_rate_manual': { max_particles: 0 },
          'minecraft:emitter_lifetime_expression': { activation_expression: 0, expiration_expression: 1 },
          'minecraft:emitter_shape_point': {},
          'minecraft:particle_lifetime_expression': { max_lifetime: 0 },
          'minecraft:particle_appearance_billboard': { size: [0, 0], facing_camera_mode: 'lookat_xyz', uv: { texture_width: 128, texture_height: 128, uv: [0, 0], uv_size: [8, 8] } },
        },
      },
    })));
  }

  files.set('manifest.json', json({
    format_version: 2,
    header: { name: 'LeX Client', description: 'Generado por LeX Client en este equipo', uuid: HEADER_ID, version, min_engine_version: [1, 21, 0] },
    modules: [{ type: 'resources', uuid: MODULE_ID, version }],
  }));
  if (icon) files.set('pack_icon.png', icon);
  files.set('entity/player.entity.json', json(out));
  files.set('render_controllers/lex.render_controllers.json', json({ format_version: '1.8.0', render_controllers: controllers }));
  files.set('models/entity/lex.geo.json', json({ format_version: '1.12.0', 'minecraft:geometry': geometry }));
  files.set('animations/lex.animation.json', json(animations(selected)));
  return files;
}

export function writePack(root, files) {
  if (basename(root) !== PACK_NAME) throw new Error('destino inesperado');
  rmSync(root, { recursive: true, force: true });
  for (const [name, body] of files) {
    const target = join(root, name);
    mkdirSync(dirname(target), { recursive: true });
    writeFileSync(target, body);
  }
}

// --- activacion automatica ---
export function withLex(list, version) {
  const rest = (Array.isArray(list) ? list : []).filter((p) => p?.pack_id !== HEADER_ID);
  return version ? [{ pack_id: HEADER_ID, version }, ...rest] : rest;
}

// Minecraft escribe sus JSON con tabuladores y a veces comentarios.
export function parseLoose(text) {
  const clean = text.replace(/^﻿/, '').replace(/^\s*\/\/.*$/gm, '').trim();
  return clean === '' ? [] : JSON.parse(clean);
}

// Activa (o quita, con version = null) LeX como pack global de cada cuenta.
const folder = (dir) => (existsSync(dir) ? readdirSync(dir, { withFileTypes: true }).filter((e) => e.isDirectory()).map((e) => join(dir, e.name)) : []);

// uuid -> carpeta de cada pack instalado (algunos llevan una carpeta intermedia).
export function installedPacks(usersDir) {
  const found = new Map();
  for (const user of folder(usersDir)) {
    for (const kind of ['resource_packs', 'development_resource_packs']) {
      for (const pack of folder(join(user, 'games', 'com.mojang', kind))) {
        for (const dir of [pack, ...folder(pack)]) {
          const path = join(dir, 'manifest.json');
          if (!existsSync(path)) continue;
          const id = /"header"[\s\S]*?"uuid"\s*:\s*"([0-9a-f-]{36})"/i.exec(readFileSync(path, 'utf8')) ?? /"uuid"\s*:\s*"([0-9a-f-]{36})"/i.exec(readFileSync(path, 'utf8'));
          if (id && !found.has(id[1])) found.set(id[1], dir);
        }
      }
    }
  }
  return found;
}

const PIECES = ['helmet', 'chestplate', 'leggings', 'boots'];
const MATERIALS = ['leather', 'chainmail', 'iron', 'diamond', 'gold', 'turtle', 'netherite', 'copper'];

// Iconos de armadura tal como los ve el jugador: primero sus packs, luego el juego.
export function writeItemIcons(gameDir, usersDir, dataDir) {
  const target = join(dataDir, 'items');
  mkdirSync(target, { recursive: true });
  const names = [...MATERIALS.flatMap((m) => PIECES.map((p) => `${m}_${p}`)), 'elytra'];
  const packs = installedPacks(usersDir);
  const order = [];
  for (const user of folder(usersDir)) {
    const path = join(user, 'games', 'com.mojang', 'minecraftpe', 'global_resource_packs.json');
    if (!existsSync(path)) continue;
    try {
      for (const p of parseLoose(readFileSync(path, 'utf8'))) if (packs.has(p.pack_id)) order.push([packs.get(p.pack_id), p.subpack]);
    } catch { /* lista ilegible: se usan los del juego */ }
  }
  const archives = folder(join(gameDir, 'data', 'resource_packs'))
    .map((dir) => join(dir, '__brarchive', 'textures', 'items.brarchive'))
    .filter((p) => /vanilla/.test(p) && existsSync(p))
    .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }))
    .map((p) => readFileSync(p));
  let written = 0;
  for (const name of names) {
    let png = null;
    for (const [dir, sub] of order) {
      const options = [sub && join(dir, 'subpacks', sub, 'textures', 'items', `${name}.png`), join(dir, 'textures', 'items', `${name}.png`)];
      const hit = options.find((p) => p && existsSync(p));
      if (hit) { png = readFileSync(hit); break; }
    }
    for (const archive of png ? [] : archives) {
      try { png = findEntry(archive, `${name}.png`); } catch { png = null; }
      if (png) break;
    }
    if (!png) continue;
    writeFileSync(join(target, `${name}.png`), png);
    written++;
  }
  return written;
}

// Skin clasica mas reciente del usuario, para las vistas previas.
export function findSkin(usersDir) {
  let best = null;
  for (const user of folder(usersDir)) {
    const dir = join(user, 'games', 'com.mojang', 'custom_skins');
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir).filter((n) => /\.png$/i.test(n))) {
      const path = join(dir, name);
      const time = statSync(path).mtimeMs;
      const png = readFileSync(path);
      if (png.length > 24 && png.readUInt32BE(16) === png.readUInt32BE(20) && (!best || time > best.time)) best = { path, time };
    }
  }
  return best?.path ?? null;
}

export function setGlobalPack(usersDir, version) {
  let changed = 0;
  if (!existsSync(usersDir)) return changed;
  for (const user of readdirSync(usersDir)) {
    const dir = join(usersDir, user, 'games', 'com.mojang', 'minecraftpe');
    if (!existsSync(dir)) continue;
    const path = join(dir, 'global_resource_packs.json');
    const before = existsSync(path) ? readFileSync(path, 'utf8') : '[]';
    let list;
    try {
      list = parseLoose(before);
    } catch {
      continue; // formato desconocido: no se toca
    }
    const next = JSON.stringify(withLex(list, version), null, 2);
    if (existsSync(path) && !existsSync(`${path}.lex-backup`)) copyFileSync(path, `${path}.lex-backup`);
    writeFileSync(`${path}.tmp`, next);
    renameSync(`${path}.tmp`, path);
    changed++;
  }
  return changed;
}

// Archivos que lee la DLL: catalogo, PNG de cada cosmetico e iconos.
export function writeClientAssets(root, dataDir, vanilla = { bones: [], images: new Map() }, fileAssets = null) {
  const assets = join(root, 'assets', 'cosmetics');
  const target = join(dataDir, 'cosmetics');
  mkdirSync(target, { recursive: true });
  for (const c of COSMETICS) {
    const own = join(assets, `${c.geo === 'trail' ? `trail-${c.art}` : c.id}.png`);
    if (c.file && fileAssets) writeFileSync(join(target, `${c.id}.png`), fileAssets.texture(c));
    else if (vanilla.images.has(c.id)) writeFileSync(join(target, `${c.id}.png`), vanilla.images.get(c.id));
    else if (existsSync(own)) copyFileSync(own, join(target, `${c.id}.png`));
  }
  if (fileAssets) for (const p of FILE_DATA.particles) writeFileSync(join(target, `${p.id}.fp${p.channel}.png`), fileAssets.particle(p));
  const round = (key, v) => (typeof v === 'number' ? Math.round(v * 10000) / 10000 : v);
  writeFileSync(join(target, 'catalog.json'), JSON.stringify(COSMETICS.map(({ source, ...c }) => ({
    ...c, ...(c.slot === 'cape' ? { alt: CAPE_PLAIN } : {}), bones: bonesOf(c.id, vanilla.bones),
  })), round));
  const icons = join(dataDir, 'icons');
  mkdirSync(icons, { recursive: true });
  const from = join(root, 'assets', 'icons');
  if (existsSync(from)) for (const name of readdirSync(from)) copyFileSync(join(from, name), join(icons, name));
}
