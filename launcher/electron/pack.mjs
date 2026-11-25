// Datos que LeX entrega al juego. Se generan a partir de la instalacion del
// usuario y se activan solos: no se distribuye ningun archivo de Mojang.
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { crc32, deflateSync } from 'node:zlib';
import { ATLAS, BONES, COSMETICS, DENSITY, TRAIL, TRAILS, CAPE_PLAIN, bonesOf } from '../shared/cosmetics.mjs';
import { BASE_H, BASE_W, CHANNELS, COUNTS, COUNT_W, CROSSHAIR, EXTRA_H, GROUPS, HEALTH, HUD_W, PEER_W, REACHES, REACH_W, SLOT, VALUES } from '../shared/telemetry.mjs';

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
const PHYSICS_START = ['run', 'fall', 'turn', 'phase'].map((v) => `variable.lex_${v} = 0;`);
const PHYSICS = [
  `variable.lex_run = ${LOCAL} ? variable.lex_run + (math.min(query.ground_speed / 5.6, 1.4) - variable.lex_run) * math.min(query.delta_time * 5, 1) : 0;`,
  `variable.lex_fall = ${LOCAL} ? variable.lex_fall + (math.clamp(query.vertical_speed * -7, -14, 80) - variable.lex_fall) * math.min(query.delta_time * 6, 1) : 0;`,
  `variable.lex_turn = ${LOCAL} ? variable.lex_turn + (math.clamp(query.yaw_speed * 0.1, -20, 20) - variable.lex_turn) * math.min(query.delta_time * 5, 1) : 0;`,
  `variable.lex_phase = ${LOCAL} ? math.mod(variable.lex_phase + query.delta_time * (120 + variable.lex_run * 430), 36000) : 0;`,
];

function animations() {
  const bones = {};
  // Cada tira sigue a la anterior con retraso: la onda baja por la capa.
  for (let i = 0; i < 8; i++) {
    const wave = `math.sin(variable.lex_phase - ${i * 50}) * (${(0.8 + i * 0.2).toFixed(1)} + variable.lex_run * ${(3 + i * 0.9).toFixed(1)})`;
    bones[`lexCape${i}`] = {
      rotation: i === 0
        ? [`6 + variable.lex_run * 36 + variable.lex_fall + (query.is_sneaking ? 16 : 0) + ${wave}`, 0, 'variable.lex_turn']
        : [`${wave} - variable.lex_run * 3 + variable.lex_fall * 0.05`, 0, 'variable.lex_turn * 0.2'],
    };
  }
  bones.lexPlain0 = { rotation: ['6 + 62 * query.cape_flap_amount + (query.is_sneaking ? 18 : 0)', 0, 0] };
  const flap = '(query.is_on_ground ? math.sin(query.life_time * 110) * 6 : math.sin(query.life_time * 420) * 24)';
  for (const kind of ['Wing', 'Dragon']) {
    bones[`lex${kind}L`] = { rotation: [6, `-22 - ${flap}`, 8] };
    bones[`lex${kind}L2`] = { rotation: [0, `-14 - ${flap} * 0.8`, 0] };
    bones[`lex${kind}R`] = { rotation: [6, `22 + ${flap}`, -8] };
    bones[`lex${kind}R2`] = { rotation: [0, `14 + ${flap} * 0.8`, 0] };
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
  // Mascotas: flotan, se retrasan al correr y se inclinan al girar.
  for (const pet of ['Dragon', 'Lynx', 'Spirit']) {
    bones[`lexPet${pet}`] = {
      position: ['math.sin(query.life_time * 50) * 1.2', 'math.sin(query.life_time * 130) * 1.3', 'variable.lex_run * 7 + math.cos(query.life_time * 40)'],
      rotation: ['variable.lex_run * 14', 'math.sin(query.life_time * 35) * 8', 'variable.lex_turn * -0.8'],
    };
  }
  bones.lexPetDragonHead = { rotation: ['math.sin(query.life_time * 90) * 5', 'math.sin(query.life_time * 47) * 16', 0] };
  bones.lexPetDragonTail = { rotation: ['math.sin(query.life_time * 100) * 6', 'math.sin(query.life_time * 150) * 16', 0] };
  bones.lexPetDragonTail2 = { rotation: ['math.sin(query.life_time * 100 - 60) * 10', 'math.sin(query.life_time * 150 - 70) * 22', 0] };
  bones.lexPetDragonL = { rotation: [0, 0, '-12 + math.sin(query.life_time * (560)) * 34'] };
  bones.lexPetDragonR = { rotation: [0, 0, '12 - math.sin(query.life_time * (560)) * 34'] };
  bones.lexPetLynxHead = { rotation: ['math.sin(query.life_time * 70) * 5', 'math.sin(query.life_time * 45) * 18', 'math.sin(query.life_time * 30) * 6'] };
  bones.lexPetLynxTail = { rotation: ['34 + math.sin(query.life_time * 120) * 12', 'math.sin(query.life_time * 170) * 22', 0] };
  bones.lexPetSpiritTail = { rotation: ['variable.lex_run * 30 + math.sin(query.life_time * 160) * 10', 0, 'math.sin(query.life_time * 120) * 10'] };
  bones.lexPetSpiritOrbs = { rotation: ['math.sin(query.life_time * 60) * 20', 'query.life_time * 130', 0] };
  return {
    format_version: '1.8.0',
    animations: {
      'animation.lex.idle': { loop: true, bones },
      // La placa de vida mira siempre a la camara.
      'animation.lex.health': { loop: true, bones: { lexHealth: { rotation: ['-query.camera_rotation(0)', '180 + query.camera_rotation(1) - query.body_y_rotation', 0] } } },
    },
  };
}

// Como se mueve cada estela: velocidad inicial, aceleracion vertical, tamano, vida.
const MOTION = {
  fire: [0.5, 0.9, 0.15, 0.6], bolt: [0.9, 0.5, 0.2, 0.35], water: [1.2, -5, 0.11, 0.6], frost: [0.5, -0.5, 0.1, 1.0], hearts: [0.5, 0.8, 0.13, 0.8],
  stars: [0.8, 0.2, 0.12, 0.9], petals: [0.7, -0.8, 0.12, 1.2], smoke: [0.35, 0.8, 0.24, 0.9], ender: [1.1, 0.6, 0.1, 0.8], rgb: [0.6, 0.5, 0.12, 0.9],
};
function trailParticle(key, index) {
  const [speed, lift, size, life] = MOTION[key];
  const half = (size * 36) / 33;
  return {
    format_version: '1.10.0',
    particle_effect: {
      description: { identifier: `lex:trail_${key}`, basic_render_parameters: { material: 'particles_blend', texture: `textures/lex/p/${index}` } },
      components: {
        'minecraft:emitter_rate_steady': { spawn_rate: 30, max_particles: 50 },
        'minecraft:emitter_lifetime_looping': { active_time: 1 },
        'minecraft:emitter_shape_disc': { offset: [0, 0.12, 0], radius: 0.32, direction: 'outwards' },
        'minecraft:particle_initial_speed': `${speed} * math.random(0.4, 1)`,
        'minecraft:particle_lifetime_expression': { max_lifetime: `${life} * math.random(0.7, 1.1)` },
        'minecraft:particle_motion_dynamic': { linear_acceleration: ['math.random(-0.6, 0.6)', lift, 'math.random(-0.6, 0.6)'], linear_drag_coefficient: 1.2 },
        'minecraft:particle_appearance_billboard': {
          size: [size, half], facing_camera_mode: 'rotate_xyz',
          uv: { texture_width: TRAIL.w, texture_height: TRAIL.h + index, flipbook: { base_UV: [0, 0], size_UV: [33, 36], step_UV: [33, 0], frames_per_second: 8, max_frame: 8, stretch_to_lifetime: true, loop: false } },
        },
      },
    },
  };
}

export function buildPack(entity, options) {
  const { version, icon = null, particleIds = [], telemetry = true, disabledParticles = false } = options;
  const out = structuredClone(entity);
  const d = out['minecraft:client_entity'].description;
  const files = new Map();
  const controllers = {};
  const show = (id, controller, when) => {
    controllers[`controller.render.${id}`] = controller;
    d.render_controllers.push({ [`controller.render.${id}`]: when });
  };

  d.materials.lex = 'entity_alphatest';
  d.textures.lex = 'textures/lex/atlas';
  d.geometry.lex = 'geometry.lex.cosmetics';
  // Solo el jugador local paga estas animaciones: en un servidor lleno, hacerlo
  // para todos costaba FPS.
  d.animations.lex = 'animation.lex.idle';
  d.scripts.animate.push({ lex: VISIBLE });
  d.scripts.initialize = [...(d.scripts.initialize ?? []), ...PHYSICS_START];
  d.scripts.pre_animation = [...(d.scripts.pre_animation ?? []), ...PHYSICS];
  const running = `${LOCAL} && query.is_sprinting && query.is_on_ground && !variable.is_paperdoll`;
  d.animations.lex_trail = 'controller.animation.lex.trail';
  d.scripts.animate.push({ lex_trail: LOCAL });
  d.particle_effects = { ...(d.particle_effects ?? {}), ...Object.fromEntries(TRAILS.map((k) => [`lex_${k}`, `lex:trail_${k}`])) };
  TRAILS.forEach((key, i) => {
    files.set(`textures/lex/p/${i}.png`, blank(TRAIL.w, TRAIL.h + i));
    files.set(`particles/lex_trail_${key}.json`, json(trailParticle(key, i)));
  });
  files.set('animation_controllers/lex.animation_controllers.json', json({
    format_version: '1.10.0',
    animation_controllers: { 'controller.animation.lex.trail': {
      initial_state: 'still',
      states: {
        still: { transitions: [{ run: running }] },
        run: { particle_effects: TRAILS.map((k) => ({ effect: `lex_${k}` })), transitions: [{ still: `!(${running})` }] },
      },
    } },
  }));
  files.set('textures/lex/atlas.png', blank(ATLAS.w * DENSITY, ATLAS.h * DENSITY));
  show('lex', {
    rebuild_animation_matrices: true,
    geometry: 'Geometry.lex',
    materials: [{ '*': 'Material.lex' }],
    textures: ['Texture.lex'],
  }, VISIBLE);

  const geometry = [{
    description: {
      identifier: 'geometry.lex.cosmetics', texture_width: ATLAS.w, texture_height: ATLAS.h,
      visible_bounds_width: 5, visible_bounds_height: 5, visible_bounds_offset: [0, 1.5, 0],
    },
    bones: [...SKELETON, ...BONES],
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
    }, `!${LOCAL} && math.mod(math.floor(query.life_time * 4), 4) == 0`);

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
    }, '!query.is_local_player && query.hurt_time > 7');

    // Vida de otros jugadores (solo minecraft:player, nunca criaturas).
    d.geometry.lex_hp = 'geometry.lex.health';
    const plates = [];
    for (let i = 0; i < HEALTH.rows; i++) {
      d.textures[`lex_h${i}`] = `textures/lex/t/h${i}`;
      files.set(`textures/lex/t/h${i}.png`, blank(HEALTH.w + i, HEALTH.rowH));
      plates.push(`Texture.lex_h${i}`);
    }
    const face = { uv: [0, 0], uv_size: [HEALTH.w, HEALTH.rowH] };
    geometry.push({
      description: { identifier: 'geometry.lex.health', texture_width: HEALTH.w, texture_height: HEALTH.rowH, visible_bounds_width: 4, visible_bounds_height: 5, visible_bounds_offset: [0, 1.5, 0] },
      bones: [{
        name: 'lexHealth', pivot: [0, 41, 0],
        cubes: [{ origin: [-7, 38, 0], size: [14, 4, 0], uv: { north: face } }],
      }],
    });
    d.animations.lex_hp = 'animation.lex.health';
    const others = options.healthOnSelf ? '!variable.is_first_person && !query.is_in_ui' : '!query.is_local_player && query.health > 0 && !query.is_in_ui && !query.is_invisible';
    d.scripts.animate.push({ lex_hp: options.healthOnSelf ? '1' : `!${LOCAL}` });
    show('lex_health', {
      rebuild_animation_matrices: true,
      geometry: 'Geometry.lex_hp',
      materials: [{ '*': 'Material.lex' }],
      arrays: { textures: { 'Array.v': plates } },
      textures: [`Array.v[math.clamp(math.ceil(query.health), 0, ${HEALTH.rows - 1})]`],
    }, others);

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
  files.set('animations/lex.animation.json', json(animations()));
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

// Archivos que lee la DLL: catalogo y PNG de cada cosmetico.
export function writeClientAssets(assetsDir, dataDir) {
  const target = join(dataDir, 'cosmetics');
  mkdirSync(target, { recursive: true });
  for (const c of COSMETICS) copyFileSync(join(assetsDir, `${c.id}.png`), join(target, `${c.id}.png`));
  writeFileSync(join(target, 'catalog.json'), JSON.stringify(COSMETICS.map((c) => ({ ...c, ...(c.slot === 'cape' ? { alt: CAPE_PLAIN } : {}), bones: bonesOf(c.id) }))));
}
