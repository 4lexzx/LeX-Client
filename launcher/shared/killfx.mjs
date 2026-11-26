// Animaciones de baja (sobre quien matas) y de muerte (la tuya), de ~6 s.
//
// Cada una combina tres cosas:
//  - un doble del jugador con SU skin (Texture.default) que hace la coreografia,
//    mientras el cuerpo normal se oculta;
//  - modelos 3D propios en el atlas (espada, rayos, vortice, alas...);
//  - particulas (sangre, humo, cenizas, chispas...).
// Bedrock no deja a la DLL elegir la animacion en vivo: la elegida se fija en el
// pack al pulsar Jugar (selectedKill / selectedDeath).

export const KILL_KINDS = [
  { key: 'execution', slot: 'kill', name: 'Ejecución', rect: [0, 88, 32, 24] },
  { key: 'wrath', slot: 'kill', name: 'Ira divina', rect: [33, 88, 32, 24] },
  { key: 'blackhole', slot: 'kill', name: 'Agujero negro', rect: [66, 88, 32, 24] },
  { key: 'rocket', slot: 'kill', name: 'Cohete', rect: [99, 88, 32, 24] },
  { key: 'ascension', slot: 'death', name: 'Ascensión', rect: [0, 160, 32, 24] },
  { key: 'bleed', slot: 'death', name: 'Desangrado', rect: [33, 160, 32, 24] },
  { key: 'implode', slot: 'death', name: 'Implosión', rect: [66, 160, 32, 24] },
];

export const KILL_SWATCHES = {
  execution: { blade: [0, 0, 4, 16], guard: [4, 0, 8, 3], hilt: [4, 3, 2, 6], pommel: [6, 3, 2, 2], pool: [12, 0, 12, 12], shadow: [24, 0, 8, 8], ring: [0, 16, 8, 8], rune: [8, 12, 4, 4] },
  wrath: { cloud: [0, 0, 8, 8], clouddark: [8, 0, 8, 8], bolt: [16, 0, 4, 16], scorch: [20, 0, 12, 12], glow: [0, 8, 12, 12], ember: [20, 12, 4, 4] },
  blackhole: { vortex: [0, 0, 16, 16], vortex2: [16, 0, 16, 16], ring: [0, 16, 8, 8], star: [8, 16, 4, 4] },
  rocket: { fin: [0, 0, 6, 8], nozzle: [6, 0, 4, 4], flame: [10, 0, 4, 12], gold: [14, 0, 4, 4], cyan: [18, 0, 4, 4], pink: [22, 0, 4, 4], flash: [0, 12, 12, 12], body: [26, 0, 6, 8] },
  ascension: { beam: [0, 0, 8, 16], feather: [8, 0, 12, 10], halo: [20, 0, 8, 8], glow: [8, 10, 14, 14] },
  bleed: { pool: [0, 0, 12, 12], stone: [12, 0, 8, 8], front: [20, 0, 8, 12], flower: [28, 0, 4, 6], drip: [12, 8, 4, 4] },
  implode: { ring: [0, 0, 12, 12], splat: [12, 0, 10, 10], shard: [22, 0, 4, 4], glow: [0, 12, 12, 12] },
};

export const LENGTH = 6.5;
const FACES = ['north', 'south', 'east', 'west', 'up', 'down'];

// --- el doble del jugador: mismas piezas y UV de caja que la skin de 64x64 ---
const SKIN = [
  ['body', null, [0, 24, 0], [[[-4, 12, -2], [8, 12, 4], [16, 16], 0], [[-4, 12, -2], [8, 12, 4], [16, 32], 0.25]]],
  ['head', 'body', [0, 24, 0], [[[-4, 24, -4], [8, 8, 8], [0, 0], 0], [[-4, 24, -4], [8, 8, 8], [32, 0], 0.5]]],
  ['rightArm', 'body', [-5, 22, 0], [[[-8, 12, -2], [4, 12, 4], [40, 16], 0], [[-8, 12, -2], [4, 12, 4], [40, 32], 0.25]]],
  ['leftArm', 'body', [5, 22, 0], [[[4, 12, -2], [4, 12, 4], [32, 48], 0], [[4, 12, -2], [4, 12, 4], [48, 48], 0.25]]],
  ['rightLeg', null, [-1.9, 12, 0], [[[-3.9, 0, -2], [4, 12, 4], [0, 16], 0], [[-3.9, 0, -2], [4, 12, 4], [0, 32], 0.25]]],
  ['leftLeg', null, [1.9, 12, 0], [[[-0.1, 0, -2], [4, 12, 4], [16, 48], 0], [[-0.1, 0, -2], [4, 12, 4], [0, 48], 0.25]]],
];

// dz/flip: al morir tu, el doble aparece delante de la camara y mirandote.
export function cloneGeometry(identifier, dz = 0, flip = false) {
  const at = ([x, y, z]) => [x, y, z + dz];
  const bones = [{ name: 'lexC', pivot: at([0, 0, 0]), ...(flip ? { rotation: [0, 180, 0] } : {}), cubes: [],
    locators: { lexc_feet: at([0, 0.5, 0]), lexc_chest: at([0, 18, -2]), lexc_neck: at([0, 24, 0]), lexc_head: at([0, 30, 0]) } }];
  for (const [name, parent, pivot, cubes] of SKIN) {
    bones.push({
      name: `lexC_${name}`, parent: parent ? `lexC_${parent}` : 'lexC', pivot: at(pivot),
      cubes: cubes.map(([origin, size, uv, inflate]) => ({ origin: at(origin), size, uv, ...(inflate ? { inflate } : {}) })),
    });
  }
  return { description: { identifier, texture_width: 64, texture_height: 64, visible_bounds_width: 8, visible_bounds_height: 10, visible_bounds_offset: [0, 3, 0] }, bones };
}

// --- modelos de cada efecto, con UV en su hoja del atlas ---
export function killBones(prefix, keys, dz = 0, flip = false) {
  const bones = [{ name: prefix, pivot: [0, 0, dz], ...(flip ? { rotation: [0, 180, 0] } : {}), cubes: [] }];
  for (const kind of KILL_KINDS.filter((k) => keys.includes(k.key))) {
    const [ox, oy] = kind.rect;
    const sw = KILL_SWATCHES[kind.key];
    const s = (n) => ({ uv: [ox + sw[n][0], oy + sw[n][1]], uv_size: [sw[n][2], sw[n][3]] });
    const six = (n, over = {}) => ({ ...Object.fromEntries(FACES.map((f) => [f, s(n)])), ...Object.fromEntries(Object.entries(over).map(([f, m]) => [f, s(m)])) });
    const only = (face, n) => ({ [face]: s(n) });
    const at = ([x, y, z]) => [x, y, z + dz];
    const cube = (origin, size, uv, extra = {}) => ({ origin: at(origin), size, uv, ...(extra.rotation ? { rotation: extra.rotation, pivot: at(extra.pivot) } : {}) });
    const root = `${prefix}${kind.key}`;
    const bone = (name, pivot, cubes, parent = root) => bones.push({ name: `${root}_${name}`, parent, pivot: at(pivot), cubes });
    bones.push({ name: root, parent: prefix, pivot: at([0, 0, 0]), cubes: [] });
    const ground = (name, r, sw2, y = 0.08) => bone(name, [0, y, 0], [cube([-r, y, -r], [r * 2, 0, r * 2], only('up', sw2))]);

    if (kind.key === 'execution') {
      bone('sword', [0, 0, -7], [
        cube([-1, 2, -7.3], [2, 28, 0.6], six('blade')),
        cube([-0.4, 0, -7.2], [0.8, 2, 0.4], six('blade')),
        cube([-4.5, 30, -7.7], [9, 1.6, 1.4], six('guard')),
        cube([-0.75, 31.6, -7.3], [1.5, 6, 0.6], six('hilt')),
        cube([-1.1, 37.6, -7.6], [2.2, 2.2, 1.2], six('pommel')),
        cube([-0.5, 10, -7.35], [1, 16, 0], only('north', 'rune')),
      ]);
      ground('shadow', 10, 'shadow', 0.06);
      ground('ring', 14, 'ring', 0.12);
      ground('pool', 9, 'pool', 0.09);
    } else if (kind.key === 'wrath') {
      const clouds = [[0, 0, 0], [7, 1, 3], [-7, 0.5, 2], [3, 1.5, -7], [-4, 1, -6], [5, -0.5, -3], [-3, 1.5, 7]];
      bone('clouds', [0, 50, 0], clouds.map(([x, y, z], i) => cube([x - 5, 48 + y, z - 5], [10, 4 + (i % 2), 10], six(i % 2 ? 'clouddark' : 'cloud'))));
      [[-2, 1], [3, -2], [0, 3]].forEach(([bx, bz], b) => {
        const segs = [];
        let x = bx;
        for (let i = 0; i < 6; i++) {
          const y = 2 + i * 8;
          segs.push(cube([x - 0.8, y, bz - 0.8], [1.6, 8.8, 1.6], six('bolt'), { rotation: [0, 0, i % 2 ? 22 : -22], pivot: [x, y + 4, bz] }));
          x += i % 2 ? -3 : 3;
        }
        bone(`bolt${b}`, [bx, 0, bz], segs);
      });
      ground('scorch', 11, 'scorch');
      bone('flash', [0, 16, 0], [cube([-8, 4, 0], [16, 24, 0], only('north', 'glow')), cube([0, 4, -8], [0, 24, 16], only('east', 'glow'))]);
    } else if (kind.key === 'blackhole') {
      ground('disc', 15, 'vortex', 0.12);
      ground('disc2', 10, 'vortex2', 0.16);
      ground('ring', 16, 'ring', 0.2);
      bone('debris', [0, 4, 0], Array.from({ length: 10 }, (_, i) => {
        const a = (i / 10) * Math.PI * 2;
        return cube([Math.sin(a) * 11 - 0.6, 1 + (i % 4) * 2.5, Math.cos(a) * 11 - 0.6], [1.2, 1.2, 1.2], six('star'));
      }));
    } else if (kind.key === 'rocket') {
      bone('engine', [0, 0, 0], [
        cube([-2.5, -3, -2.5], [5, 3, 5], six('nozzle')),
        cube([-4.2, 12, -2.2], [8.4, 4, 4.4], six('body')),
        ...[0, 90, 180, 270].map((a) => cube([-0.3, -2, 2.4], [0.6, 8, 5], six('fin'), { rotation: [0, a, 0], pivot: [0, 0, 0] })),
        cube([-2, -14, 0], [4, 11, 0], only('north', 'flame')),
        cube([0, -14, -2], [0, 11, 4], only('east', 'flame')),
      ]);
      ['gold', 'cyan', 'pink'].forEach((color, i) => {
        const [x, z] = [[0, 0], [6, -4], [-6, 3]][i];
        const top = 64 + i * 4;
        bone(`burst${i}`, [x, top, z], Array.from({ length: 18 }, (_, n) => {
          const yy = 1 - (2 * (n + 0.5)) / 18;
          const r = Math.sqrt(1 - yy * yy);
          const a = n * 2.39996;
          return cube([x + Math.cos(a) * r - 0.5, top + yy - 0.5, z + Math.sin(a) * r - 0.5], [1, 1, 1], six(color));
        }));
      });
      bone('flash', [0, 64, 0], [cube([-10, 54, 0], [20, 20, 0], only('north', 'flash')), cube([0, 54, -10], [0, 20, 20], only('east', 'flash'))]);
    } else if (kind.key === 'ascension') {
      bone('beam', [0, 0, 0], [cube([-6, 0, 0], [12, 80, 0], only('north', 'beam')), cube([0, 0, -6], [0, 80, 12], only('east', 'beam'))]);
      ground('glow', 12, 'glow', 0.15);
      bone('halo', [0, 34, 0], [cube([-4, 34, -4], [8, 0, 8], only('up', 'halo'))]);
      const fe = s('feather');
      bone('wingL', [2, 22, 2.5], [cube([2, 13, 2.5], [16, 14, 0], { north: fe })]);
      bone('wingR', [-2, 22, 2.5], [cube([-18, 13, 2.5], [16, 14, 0], { north: { uv: [fe.uv[0] + fe.uv_size[0], fe.uv[1]], uv_size: [-fe.uv_size[0], fe.uv_size[1]] } })]);
    } else if (kind.key === 'bleed') {
      ground('pool', 10, 'pool', 0.09);
      bone('stone', [0, 0, 10], [
        cube([-6.5, 0, 8.5], [13, 2.2, 5], six('stone')),
        cube([-5, 2.2, 9.5], [10, 12, 2.6], six('stone', { north: 'front', south: 'front' })),
        cube([-4, 14.2, 9.5], [8, 1.4, 2.6], six('stone')),
      ]);
      bone('flowers', [0, 2.2, 7], [
        cube([-5, 2.2, 7.5], [4, 6, 0], only('north', 'flower'), { rotation: [0, 45, 0], pivot: [-3, 2.2, 7.5] }),
        cube([2, 2.2, 7.5], [4, 6, 0], only('north', 'flower'), { rotation: [0, -45, 0], pivot: [4, 2.2, 7.5] }),
      ]);
    } else if (kind.key === 'implode') {
      ground('ring', 14, 'ring', 0.12);
      bone('splats', [0, 0.07, 0], [[-8, -5, 4.5], [6, 4, 3.5], [2, -9, 3], [-5, 8, 3.8]].map(([x, z, r]) => cube([x - r, 0.07, z - r], [r * 2, 0, r * 2], only('up', 'splat'))));
      bone('shards', [0, 18, 0], Array.from({ length: 10 }, (_, i) => {
        const a = (i / 10) * Math.PI * 2;
        return cube([Math.sin(a) * 3 - 0.5, 12 + (i % 4) * 4, Math.cos(a) * 2.5 - 0.5], [1, 1, 1], six('shard'));
      }));
      bone('flash', [0, 18, 0], [cube([-10, 6, 0], [20, 24, 0], only('north', 'glow')), cube([0, 6, -10], [0, 24, 20], only('east', 'glow'))]);
    }
  }
  return bones;
}

// --- coreografias ---
const k = (pairs, smooth = true) => Object.fromEntries(pairs.map(([t, v]) => [t.toFixed(2), smooth ? { post: v, lerp_mode: 'catmullrom' } : v]));
const v3 = (n) => [n, n, n];
const P = 'lexDfx';
const C = (b) => `lexC_${b}`;
const shake = (amp, from, to, speed = 2400) => `(query.anim_time > ${from} && query.anim_time < ${to} ? math.sin(query.anim_time * ${speed}) * ${amp} : 0)`;

// tint: color sobre la skin del doble, en funcion de t (segundos muerto).
export const CHOREO = {
  execution: () => ({
    bones: {
      [C('rightLeg')]: { rotation: k([[0, [0, 0, 0]], [0.6, [0, 0, 0]], [1.4, [-90, 0, 6]]]), position: k([[0, [0, 0, 0]], [0.6, [0, 0, 0]], [1.4, [0, 0, 2]]]) },
      [C('leftLeg')]: { rotation: k([[0, [0, 0, 0]], [0.6, [0, 0, 0]], [1.4, [-90, 0, -6]]]), position: k([[0, [0, 0, 0]], [0.6, [0, 0, 0]], [1.4, [0, 0, 2]]]) },
      [C('body')]: {
        position: k([[0, [0, 0, 0]], [0.6, [0, 0, 0]], [1.4, [0, -10, 0]], [3.3, [0, -10, 0]], [4.6, [0, -18, -6]]]),
        rotation: k([[0, [0, 0, 0]], [1.4, [8, 0, 0]], [2.3, [8, 0, 0]], [2.45, [-6, 0, 0]], [3.3, [10, 0, 0]], [4.6, [88, 0, 0]]]),
      },
      [C('head')]: {
        rotation: k([[0, [0, 0, 0]], [1.4, [30, 0, 0]], [2.4, [30, 0, 0]], [3.4, [420, 160, 40]], [4.2, [520, 200, 90]]]),
        position: k([[0, [0, 0, 0]], [2.4, [0, 0, 0]], [2.9, [5, 14, -4]], [3.6, [9, -18, -9]], [3.8, [9, -16, -9]], [4.2, [10, -20, -10]]]),
      },
      [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [1.4, [-12, 0, 6]], [2.45, [-40, 0, 30]], [3.3, [-10, 0, 10]]]) },
      [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [1.4, [-12, 0, -6]], [2.45, [-40, 0, -30]], [3.3, [-10, 0, -10]]]) },
      lexC: { scale: k([[0, v3(0.2)], [0.25, v3(1.05)], [0.35, v3(1)], [5.6, v3(1)], [6.4, v3(0)]]) },
      [`${P}execution_sword`]: {
        position: k([[0, [0, 70, 0]], [1.5, [0, 70, 0]], [2.2, [0, 8, 0]], [2.4, [0, 0, 0]]], false),
        rotation: k([[2.4, [0, 0, 0]], [2.5, [3, 0, 2]], [2.6, [-2, 0, -1]], [2.7, [0, 0, 0]]]),
        scale: k([[0, v3(1)], [5.2, v3(1)], [6.2, [1, 0, 1]]]),
      },
      [`${P}execution_shadow`]: { scale: k([[0, v3(0)], [1.5, v3(0.3)], [2.35, v3(1.3)], [2.45, v3(0)]]) },
      [`${P}execution_ring`]: { scale: k([[0, v3(0)], [2.4, v3(0.1)], [2.9, v3(1.6)], [3.1, v3(0)]]) },
      [`${P}execution_pool`]: { scale: k([[0, v3(0)], [2.6, v3(0.1)], [5.0, v3(1)], [5.9, v3(1)], [6.5, v3(0)]]) },
    },
    particles: { '2.40': [{ effect: 'lex_k_blood', locator: 'lexc_neck' }, { effect: 'lex_k_spray', locator: 'lexc_neck' }, { effect: 'lex_k_smoke', locator: 'lexc_feet' }], '3.70': [{ effect: 'lex_k_blood', locator: 'lexc_feet' }] },
    tint: { r: '0.08', g: '0.0', b: '0.0', a: 'math.clamp((t - 3.3) * 0.35, 0, 0.55)' },
  }),
  wrath: () => ({
    bones: {
      lexC: {
        position: [shake(0.6, 1.1, 3.0), `math.clamp((query.anim_time - 0.4) * 9, 0, 9) - math.clamp((query.anim_time - 3.6) * 12, 0, 9)`, shake(0.6, 1.1, 3.0, 2100)],
        scale: k([[0, v3(1)], [3.6, v3(1)], [4.6, [1.1, 0.15, 1.1]], [5.2, v3(0)]]),
      },
      [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [0.9, [0, 0, 85]], [3.4, [0, 0, 80]], [3.8, [0, 0, 0]]]) },
      [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [0.9, [0, 0, -85]], [3.4, [0, 0, -80]], [3.8, [0, 0, 0]]]) },
      [C('head')]: { rotation: k([[0, [0, 0, 0]], [0.9, [-35, 0, 0]], [3.4, [-35, 0, 0]], [3.8, [20, 0, 0]]]) },
      [C('body')]: { rotation: [`${shake(4, 1.1, 3.0, 1900)}`, 0, 0] },
      [`${P}wrath_clouds`]: { scale: k([[0, v3(0)], [0.8, v3(1.1)], [1.0, v3(1)], [5.2, v3(1)], [6.2, v3(0)]]), rotation: [0, 'query.anim_time * 25', 0] },
      ...Object.fromEntries([1.2, 1.8, 2.4].map((t0, i) => [`${P}wrath_bolt${i}`, { scale: k([[0, v3(0)], [t0, v3(0)], [t0 + 0.03, v3(1)], [t0 + 0.12, v3(0)], [t0 + 0.18, v3(1)], [t0 + 0.3, v3(0)]], false) }])),
      [`${P}wrath_flash`]: { scale: k([[0, v3(0)], [1.2, v3(0)], [1.23, v3(1.4)], [1.4, v3(0)], [1.8, v3(0)], [1.83, v3(1.4)], [2.0, v3(0)], [2.4, v3(0)], [2.43, v3(1.6)], [2.7, v3(0)]], false) },
      [`${P}wrath_scorch`]: { scale: k([[0, v3(0)], [1.2, v3(0.3)], [2.6, v3(1.1)], [5.8, v3(1)], [6.5, v3(0)]]) },
    },
    particles: { '1.20': [{ effect: 'lex_k_ember', locator: 'lexc_chest' }], '1.80': [{ effect: 'lex_k_ember', locator: 'lexc_chest' }], '2.40': [{ effect: 'lex_k_ember', locator: 'lexc_chest' }, { effect: 'lex_k_smoke', locator: 'lexc_chest' }], '3.70': [{ effect: 'lex_k_ash', locator: 'lexc_chest' }], '4.30': [{ effect: 'lex_k_ash', locator: 'lexc_feet' }] },
    // Destellos blancos con cada rayo y luego carbonizado.
    tint: {
      r: '(math.abs(t - 1.25) < 0.08 || math.abs(t - 1.85) < 0.08 || math.abs(t - 2.45) < 0.08) ? 1 : 0.05',
      g: '(math.abs(t - 1.25) < 0.08 || math.abs(t - 1.85) < 0.08 || math.abs(t - 2.45) < 0.08) ? 1 : 0.04',
      b: '(math.abs(t - 1.25) < 0.08 || math.abs(t - 1.85) < 0.08 || math.abs(t - 2.45) < 0.08) ? 1 : 0.03',
      a: '(math.abs(t - 1.25) < 0.08 || math.abs(t - 1.85) < 0.08 || math.abs(t - 2.45) < 0.08) ? 0.9 : math.clamp((t - 2.4) * 0.8, 0, 0.88)',
    },
  }),
  blackhole: () => ({
    bones: {
      lexC: {
        position: k([[0, [0, 0, 0]], [1.0, [0, 3, 0]], [2.8, [0, 4, 0]], [3.6, [0, -26, 0]]]),
        rotation: [0, 'math.pow(math.clamp(query.anim_time - 0.8, 0, 3), 2) * 160', 0],
        scale: k([[0, v3(1)], [1.0, v3(1)], [2.8, [0.35, 2.2, 0.35]], [3.6, [0.05, 3, 0.05]], [3.7, v3(0)]]),
      },
      [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [1.2, [-160, 0, 20]]]) },
      [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [1.2, [-160, 0, -20]]]) },
      [`${P}blackhole_disc`]: { rotation: [0, 'query.anim_time * -220', 0], scale: k([[0, v3(0)], [0.9, v3(1.1)], [1.1, v3(1)], [3.7, v3(1.2)], [4.3, v3(0)]]) },
      [`${P}blackhole_disc2`]: { rotation: [0, 'query.anim_time * 340', 0], scale: k([[0, v3(0)], [0.7, v3(1)], [3.7, v3(1.3)], [4.3, v3(0)]]) },
      [`${P}blackhole_ring`]: { scale: k([[0, v3(0)], [3.7, v3(0)], [3.8, v3(0.4)], [4.6, v3(2.2)], [4.9, v3(0)]]) },
      [`${P}blackhole_debris`]: {
        rotation: [0, 'query.anim_time * 260', 0],
        scale: k([[0, v3(0)], [0.6, v3(1.2)], [3.6, v3(0.2)], [3.7, v3(0)]]),
        position: k([[0, [0, 0, 0]], [3.6, [0, -4, 0]]]),
      },
    },
    particles: { '0.20': [{ effect: 'lex_k_void', locator: 'lexc_feet' }], '1.60': [{ effect: 'lex_k_void', locator: 'lexc_feet' }], '3.70': [{ effect: 'lex_k_spark', locator: 'lexc_feet' }, { effect: 'lex_k_smoke', locator: 'lexc_feet' }] },
    tint: { r: '0.35', g: '0.05', b: '0.6', a: 'math.clamp((t - 0.8) * 0.3, 0, 0.7)' },
  }),
  rocket: () => {
    const lift = `math.pow(math.clamp(query.anim_time - 1.4, 0, 2), 2) * 16`;
    return {
      bones: {
        lexC: {
          position: [shake(0.5, 0.4, 1.4), lift, shake(0.5, 0.4, 1.4, 2000)],
          rotation: [0, 'math.pow(math.clamp(query.anim_time - 1.4, 0, 2), 2) * 300', 0],
          scale: k([[0, v3(1)], [2.95, v3(1)], [3.0, v3(0)]], false),
        },
        [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [0.5, [0, 0, 10]], [1.4, [-175, 0, 0]]]) },
        [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [0.5, [0, 0, -10]], [1.4, [-175, 0, 0]]]) },
        [`${P}rocket_engine`]: {
          position: [shake(0.5, 0.4, 1.4), lift, 0],
          rotation: [0, 'math.pow(math.clamp(query.anim_time - 1.4, 0, 2), 2) * 300', 0],
          scale: k([[0, v3(0)], [0.3, v3(1.15)], [0.45, v3(1)], [2.95, v3(1)], [3.0, v3(0)]], false),
        },
        ...Object.fromEntries([0, 1, 2].map((i) => [`${P}rocket_burst${i}`, {
          scale: k([[0, v3(0)], [3.0 + i * 0.25, v3(0.2)], [3.35 + i * 0.25, v3(8)], [3.9 + i * 0.25, v3(10)], [5.2 + i * 0.25, v3(0)]]),
          position: k([[0, [0, 0, 0]], [3.4 + i * 0.25, [0, 0, 0]], [5.2 + i * 0.25, [0, -10, 0]]]),
          rotation: [0, `query.anim_time * ${50 + i * 30}`, 0],
        }])),
        [`${P}rocket_flash`]: { scale: k([[0, v3(0)], [3.0, v3(0)], [3.05, v3(1.6)], [3.4, v3(0)]], false) },
      },
      particles: { '0.40': [{ effect: 'lex_k_smoke', locator: 'lexc_feet' }], '1.40': [{ effect: 'lex_k_flame', locator: 'lexc_feet' }], '3.00': [{ effect: 'lex_k_confetti', locator: 'lexc_head' }, { effect: 'lex_k_blood', locator: 'lexc_head' }] },
      tint: { r: '0', g: '0', b: '0', a: '0' },
    };
  },
  ascension: () => {
    const rise = k([[0, [0, 0, 0]], [1.4, [0, 2, 0]], [6.0, [0, 50, 0]]]);
    return {
      bones: {
        lexC: { position: rise, scale: k([[0, v3(1)], [5.4, v3(1)], [6.2, v3(0)]]) },
        [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [1.2, [0, 0, 50]], [6, [0, 0, 60]]]) },
        [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [1.2, [0, 0, -50]], [6, [0, 0, -60]]]) },
        [C('head')]: { rotation: k([[0, [0, 0, 0]], [1.4, [-25, 0, 0]]]) },
        [`${P}ascension_beam`]: { scale: k([[0, [0, 1, 0]], [0.6, [1.2, 1, 1.2]], [0.9, v3(1)], [5.4, v3(1)], [6.3, [0, 1, 0]]]) },
        [`${P}ascension_glow`]: { scale: k([[0, v3(0)], [0.6, v3(1.1)], [5.6, v3(1)], [6.3, v3(0)]]), rotation: [0, 'query.anim_time * 50', 0] },
        [`${P}ascension_halo`]: { position: rise, scale: k([[0, v3(0)], [1.8, v3(0)], [2.2, v3(1.2)], [2.4, v3(1)], [5.6, v3(1)], [6.2, v3(0)]]) },
        [`${P}ascension_wingL`]: { position: rise, scale: k([[0, v3(0)], [1.0, v3(0)], [1.8, v3(1)], [5.6, v3(1)], [6.2, v3(0)]]), rotation: [0, `-10 - math.sin(query.anim_time * 200) * (query.anim_time > 1.8 ? 24 : 0)`, 0] },
        [`${P}ascension_wingR`]: { position: rise, scale: k([[0, v3(0)], [1.0, v3(0)], [1.8, v3(1)], [5.6, v3(1)], [6.2, v3(0)]]), rotation: [0, `10 + math.sin(query.anim_time * 200) * (query.anim_time > 1.8 ? 24 : 0)`, 0] },
      },
      particles: { '1.80': [{ effect: 'lex_k_feather', locator: 'lexc_chest' }], '3.50': [{ effect: 'lex_k_feather', locator: 'lexc_chest' }] },
      tint: { r: '1', g: '0.97', b: '0.85', a: 'math.clamp((t - 1.0) * 0.18, 0, 0.75)' },
    };
  },
  bleed: () => ({
    bones: {
      lexC: { position: [`${shake(0.4, 0, 1.5, 300)}`, 0, 0], scale: k([[0, v3(1)], [5.8, v3(1)], [6.4, v3(0)]]) },
      [C('rightArm')]: { rotation: k([[0, [0, 0, 0]], [0.6, [-70, 0, -30]], [2.6, [-70, 0, -30]], [3.6, [-170, 0, 10]]]) },
      [C('leftArm')]: { rotation: k([[0, [0, 0, 0]], [0.6, [-70, 0, 30]], [2.6, [-70, 0, 30]], [3.6, [-170, 0, -10]]]) },
      [C('rightLeg')]: { rotation: k([[0, [0, 0, 0]], [1.4, [0, 0, 0]], [2.2, [-90, 0, 4]]]), position: k([[0, [0, 0, 0]], [2.2, [0, 0, 2]]]) },
      [C('leftLeg')]: { rotation: k([[0, [0, 0, 0]], [1.4, [0, 0, 0]], [2.2, [-90, 0, -4]]]), position: k([[0, [0, 0, 0]], [2.2, [0, 0, 2]]]) },
      [C('body')]: {
        position: k([[0, [0, 0, 0]], [1.4, [0, 0, 0]], [2.2, [0, -10, 0]], [2.8, [0, -10, 0]], [3.8, [0, -20, -4]]]),
        rotation: k([[0, [0, 0, 0]], [0.6, [18, 0, 0]], [1.4, [10, 0, 6]], [2.2, [14, 0, 0]], [2.8, [14, 0, 0]], [3.8, [86, 0, 0]]]),
      },
      [C('head')]: { rotation: k([[0, [0, 0, 0]], [0.6, [26, 0, 0]], [2.8, [30, 10, 0]], [3.8, [10, 30, 0]]]) },
      [`${P}bleed_pool`]: { scale: k([[0, v3(0)], [0.8, v3(0.1)], [5.0, v3(1.15)], [6.0, v3(1.15)], [6.5, v3(0)]]) },
      [`${P}bleed_stone`]: { position: k([[0, [0, -20, 0]], [3.6, [0, -20, 0]], [4.3, [0, 1, 0]], [4.5, [0, 0, 0]]]), scale: k([[0, v3(0)], [3.59, v3(0)], [3.6, v3(1)], [6.0, v3(1)], [6.5, v3(0)]], false) },
      [`${P}bleed_flowers`]: { scale: k([[0, v3(0)], [4.5, v3(0)], [4.9, v3(1.2)], [5.0, v3(1)], [6.0, v3(1)], [6.5, v3(0)]]) },
    },
    particles: { '0.10': [{ effect: 'lex_k_drip', locator: 'lexc_chest' }], '1.40': [{ effect: 'lex_k_drip', locator: 'lexc_chest' }], '2.20': [{ effect: 'lex_k_blood', locator: 'lexc_chest' }], '3.80': [{ effect: 'lex_k_blood', locator: 'lexc_feet' }] },
    tint: { r: '0.25', g: '0.26', b: '0.3', a: 'math.clamp((t - 2.5) * 0.25, 0, 0.6)' },
  }),
  implode: () => ({
    bones: {
      lexC: {
        position: [shake(0.7, 0.2, 2.5), 0, shake(0.7, 0.2, 2.5, 2600)],
        scale: k([[0, v3(1)], [2.3, v3(0.92)], [2.5, v3(1.12)], [2.55, v3(1)], [5.6, v3(1)], [6.3, v3(0)]]),
      },
      [C('head')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.2, [3, 22, -2]], [4.2, [6, -16, -6]]]), rotation: k([[2.5, [0, 0, 0]], [4.2, [540, 220, 30]]]) },
      [C('rightArm')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.1, [-14, 8, 3]], [4.0, [-20, -10, 5]]]), rotation: k([[2.5, [0, 0, 0]], [4.0, [200, 0, 300]]]) },
      [C('leftArm')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.1, [14, 8, -3]], [4.0, [20, -10, -5]]]), rotation: k([[2.5, [0, 0, 0]], [4.0, [-200, 0, -300]]]) },
      [C('rightLeg')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.1, [-6, 4, 10]], [4.0, [-9, 0, 14]]]), rotation: k([[2.5, [0, 0, 0]], [4.0, [270, 0, 40]]]) },
      [C('leftLeg')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.1, [6, 4, -10]], [4.0, [9, 0, -14]]]), rotation: k([[2.5, [0, 0, 0]], [4.0, [-270, 0, -40]]]) },
      [C('body')]: { position: k([[0, [0, 0, 0]], [2.5, [0, 0, 0]], [3.0, [0, 6, 0]], [3.9, [0, -16, 0]]]), rotation: k([[2.5, [0, 0, 0]], [3.9, [90, 60, 0]]]) },
      [`${P}implode_ring`]: { scale: k([[0, v3(0)], [2.5, v3(0.2)], [3.1, v3(1.8)], [3.4, v3(0)]]) },
      [`${P}implode_splats`]: { scale: k([[0, v3(0)], [3.0, v3(0)], [3.4, v3(1.1)], [3.6, v3(1)], [6.0, v3(1)], [6.5, v3(0)]]) },
      [`${P}implode_shards`]: { scale: k([[0, v3(0)], [2.5, v3(0.4)], [3.2, v3(4)], [3.9, v3(0)]]), rotation: [0, 'query.anim_time * 200', 0] },
      [`${P}implode_flash`]: { scale: k([[0, v3(0)], [2.5, v3(0)], [2.55, v3(1.5)], [2.9, v3(0)]], false) },
    },
    particles: { '2.50': [{ effect: 'lex_k_blood', locator: 'lexc_chest' }, { effect: 'lex_k_spray', locator: 'lexc_chest' }, { effect: 'lex_k_smoke', locator: 'lexc_chest' }], '3.60': [{ effect: 'lex_k_blood', locator: 'lexc_feet' }] },
    // Grietas rojas que laten cada vez mas rapido.
    tint: { r: '1', g: '0.1', b: '0.05', a: 't < 2.5 ? math.abs(math.sin(t * t * 160)) * (0.2 + t * 0.22) : 0' },
  }),
};

// Animacion de una ranura con su coreografia (o vacia si no hay nada elegido).
export function choreography(key) {
  if (!key || !CHOREO[key]) return { animation_length: 0.1, loop: 'hold_on_last_frame', bones: {} };
  const c = CHOREO[key]();
  const bones = {
    // El cuerpo cae de lado: el doble y los efectos se mantienen rectos.
    lexDfx: { rotation: [0, 0, '-math.min(1, math.sqrt(math.max(0, query.death_ticks) / 20 * 1.6)) * 90'] },
    ...c.bones,
  };
  const rot = '-math.min(1, math.sqrt(math.max(0, query.death_ticks) / 20 * 1.6)) * 90';
  bones.lexC = { ...(bones.lexC ?? {}), rotation: bones.lexC?.rotation ?? [0, 0, rot] };
  return { animation_length: LENGTH, loop: 'hold_on_last_frame', bones, particle_effects: c.particles };
}

export const tintOf = (key) => (key && CHOREO[key] ? CHOREO[key]().tint : { r: '0', g: '0', b: '0', a: '0' });

// --- particulas (texturas en assets/fx, 8 fotogramas de 64x64) ---
const part = (name, tint, { rate = 0, count = 0, active = 0.5, speed = '2', life = '1', size = '0.1', accel = [0, -9, 0], drag = 0.5, shape = { radius: 0.3 }, dir = 'outwards', noTint = false } = {}) => ({
  format_version: '1.10.0',
  particle_effect: {
    description: { identifier: `lex:k_${name}`, basic_render_parameters: { material: 'particles_blend', texture: `textures/lex/kfx/${name}` } },
    components: {
      'minecraft:emitter_local_space': { position: true, rotation: false },
      ...(count ? { 'minecraft:emitter_rate_instant': { num_particles: count } } : { 'minecraft:emitter_rate_steady': { spawn_rate: rate, max_particles: 200 } }),
      'minecraft:emitter_lifetime_once': { active_time: active },
      'minecraft:emitter_shape_sphere': { radius: shape.radius, direction: dir, ...(shape.offset ? { offset: shape.offset } : {}) },
      'minecraft:particle_initial_speed': speed,
      'minecraft:particle_lifetime_expression': { max_lifetime: life },
      'minecraft:particle_motion_dynamic': { linear_acceleration: accel, linear_drag_coefficient: drag },
      'minecraft:particle_initial_spin': { rotation: 'math.random(0, 360)', rotation_rate: 'math.random(-120, 120)' },
      'minecraft:particle_appearance_billboard': {
        size: [size, size], facing_camera_mode: 'rotate_xyz',
        uv: { texture_width: 512, texture_height: 64, flipbook: { base_UV: [0, 0], size_UV: [64, 64], step_UV: [64, 0], max_frame: 8, stretch_to_lifetime: true } },
      },
      ...(noTint ? {} : { 'minecraft:particle_appearance_tinting': { color: { interpolant: 'variable.particle_age / variable.particle_lifetime', gradient: tint } } }),
      'minecraft:particle_motion_collision': { collision_radius: 0.05, coefficient_of_restitution: 0.1, collision_drag: 4 },
    },
  },
});

export const KILL_PARTICLES = {
  blood: part('blood', { '0.0': '#FFC81020', '0.6': '#FF8A0814', '1.0': '#00500408' }, { count: 70, speed: 'math.random(2, 5.5)', life: 'math.random(0.7, 1.4)', size: '0.07 + variable.particle_random_1 * 0.06', accel: [0, -12, 0], drag: 0.4 }),
  spray: part('spray', { '0.0': '#FFD01828', '1.0': '#00600408' }, { rate: 90, active: 1.6, speed: 'math.random(3, 5)', life: 'math.random(0.5, 0.9)', size: '0.06', accel: [0, -10, 0], drag: 0.3, shape: { radius: 0.1 }, dir: [0, 1, 0] }),
  drip: part('drip', { '0.0': '#FFB81020', '1.0': '#00600408' }, { rate: 14, active: 1.2, speed: '0.2', life: '0.9', size: '0.05', accel: [0, -9, 0], drag: 0.2, shape: { radius: 0.25 } }),
  smoke: part('smoke', { '0.0': '#C0606060', '0.5': '#80404040', '1.0': '#00202020' }, { count: 40, speed: 'math.random(0.5, 1.6)', life: 'math.random(1.4, 2.4)', size: '0.25 + variable.particle_age * 0.4', accel: [0, 0.8, 0], drag: 1.5, shape: { radius: 0.6 } }),
  ember: part('ember', { '0.0': '#FFFFF0A0', '0.4': '#FFFF8A20', '1.0': '#00802010' }, { count: 50, speed: 'math.random(2, 5)', life: 'math.random(0.6, 1.2)', size: '0.05', accel: [0, 2, 0], drag: 1.2, shape: { radius: 0.5 } }),
  ash: part('ash', { '0.0': '#FF303030', '1.0': '#00505050' }, { rate: 60, active: 1.2, speed: 'math.random(0.3, 1.2)', life: 'math.random(1.5, 2.5)', size: '0.06', accel: [0, 0.6, 0], drag: 1, shape: { radius: 0.7 } }),
  void: part('void', { '0.0': '#00B060FF', '0.3': '#FFB060FF', '1.0': '#00400080' }, { rate: 70, active: 2.2, speed: '2.5', life: '1.1', size: '0.08', accel: [0, 0.5, 0], drag: 0.2, shape: { radius: 4 }, dir: 'inwards' }),
  spark: part('spark', { '0.0': '#FFFFFFFF', '0.5': '#FFD080FF', '1.0': '#008040C0' }, { count: 80, speed: 'math.random(4, 8)', life: 'math.random(0.4, 0.9)', size: '0.06', accel: [0, -3, 0], drag: 1.4 }),
  flame: part('flame', { '0.0': '#FFFFF6C8', '0.3': '#FFFFA030', '0.7': '#C0FF4A10', '1.0': '#00302826' }, { rate: 80, active: 1.6, speed: '3', life: '0.5', size: '0.15 + variable.particle_age * 0.5', accel: [0, -6, 0], drag: 1, shape: { radius: 0.2 }, dir: [0, -1, 0] }),
  confetti: part('confetti', {}, { count: 120, speed: 'math.random(3, 7)', life: 'math.random(1.5, 2.6)', size: '0.07', accel: [0, -3, 0], drag: 1.6, noTint: true }),
  feather: part('feather', { '0.0': '#00FFFFFF', '0.1': '#FFFFFFFF', '1.0': '#00FFFFFF' }, { count: 30, speed: 'math.random(0.5, 1.5)', life: 'math.random(2, 3)', size: '0.12', accel: [0, -0.6, 0], drag: 1.8, shape: { radius: 0.8 } }),
};
