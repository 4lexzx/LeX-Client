// Animaciones 3D de muerte: modelos que aparecen cuando matas a alguien
// (sobre su cuerpo) o cuando mueres tu. Cada una tiene su hoja de 32x24 en el
// atlas, con muestras de material (piedra, rayo, hueso...) que se estiran
// sobre las caras. tools/make_cosmetics.py pinta las hojas con este mismo mapa.

// Cada animacion pertenece a una sola ranura: Kills (sobre quien matas),
// Muertes (cuando mueres tu) o Golpes (sobre quien golpeas). rect: atlas.
export const DFX_KINDS = [
  { key: 'daze', slot: 'hit', name: 'Mareo', rect: [120, 24, 16, 12] },
  { key: 'slash', slot: 'hit', name: 'Corte', rect: [136, 24, 16, 12] },
  { key: 'impact', slot: 'hit', name: 'Impacto', rect: [120, 36, 16, 12] },
];

// Muestras de cada hoja: [x, y, ancho, alto] en unidades del atlas.
export const SWATCHES = {
  bolt: { core: [0, 0, 4, 16], scorch: [4, 0, 16, 16], glow: [20, 0, 12, 12], spark: [20, 12, 4, 4], branch: [24, 12, 4, 8] },
  grave: { stone: [0, 0, 8, 8], front: [8, 0, 8, 12], dirt: [16, 0, 8, 8], poppy: [24, 0, 4, 6], orchid: [28, 0, 4, 6], top: [16, 8, 8, 2], grass: [0, 12, 8, 8] },
  soul: { ghost: [0, 0, 8, 8], face: [8, 0, 8, 8], tail: [16, 0, 8, 8], wing: [0, 8, 10, 8], halo: [10, 8, 8, 8], glow: [18, 8, 12, 12] },
  skull: { bone: [0, 0, 8, 8], face: [8, 0, 10, 10], jaw: [18, 0, 10, 4], dark: [18, 4, 4, 4], flame: [0, 10, 6, 10], ring: [10, 10, 14, 14] },
  firework: { rocket: [0, 0, 4, 8], stick: [4, 0, 2, 8], gold: [8, 0, 4, 4], cyan: [12, 0, 4, 4], pink: [16, 0, 4, 4], trail: [20, 0, 4, 12], flash: [0, 8, 12, 12] },
  crown: { gold: [0, 0, 8, 8], gem: [8, 0, 4, 4], gemb: [12, 0, 4, 4], band: [16, 0, 12, 6], spark: [28, 0, 4, 4], ring: [0, 8, 16, 16] },
  coffin: { wood: [0, 0, 8, 8], lid: [8, 0, 8, 16], velvet: [16, 0, 8, 8], gold: [24, 0, 4, 4], dirt: [0, 16, 8, 8], glow: [16, 8, 12, 12] },
  angel: { beam: [0, 0, 8, 16], feather: [8, 0, 12, 10], halo: [20, 0, 8, 8], glow: [8, 10, 14, 14] },
  daze: { star: [0, 0, 6, 6], spark: [6, 0, 6, 6], ring: [0, 6, 16, 6] },
  slash: { slash: [0, 0, 16, 5], glow: [0, 6, 16, 6] },
  impact: { ring: [0, 0, 12, 12], shard: [12, 0, 4, 4], spark: [12, 4, 4, 4] },
};

const FACES = ['north', 'south', 'east', 'west', 'up', 'down'];

// Geometria de las animaciones de una ranura. dz desplaza todo (delante de
// ti al morir tu, para verlo tambien en primera persona).
export function dfxBones(prefix, slot, dz = 0) {
  const bones = [{ name: prefix, pivot: [0, 0, dz], cubes: [] }];
  for (const kind of DFX_KINDS.filter((k) => k.slot === slot)) {
    const [ox, oy] = kind.rect;
    const sw = SWATCHES[kind.key];
    const s = (name) => ({ uv: [ox + sw[name][0], oy + sw[name][1]], uv_size: [sw[name][2], sw[name][3]] });
    const six = (name, over = {}) => ({ ...Object.fromEntries(FACES.map((f) => [f, s(name)])), ...Object.fromEntries(Object.entries(over).map(([f, n]) => [f, s(n)])) });
    const only = (face, name) => ({ [face]: s(name) });
    const at = ([x, y, z]) => [x, y, z + dz];
    const cube = (origin, size, uv, extra = {}) => ({
      origin: at(origin), size, uv,
      ...(extra.rotation ? { rotation: extra.rotation, pivot: at(extra.pivot) } : {}),
    });
    const bone = (name, pivot, cubes, parent = `${prefix}${kind.key}`) => ({ name: `${prefix}${kind.key}_${name}`, parent, pivot: at(pivot), cubes });
    const root = { name: `${prefix}${kind.key}`, parent: prefix, pivot: at([0, 0, 0]), cubes: [] };
    bones.push(root);
    if (kind.key === 'bolt') {
      // Rayo en zigzag desde el cielo hasta el suelo, con resplandor y marca quemada.
      const core = [];
      let x = 0;
      for (let i = 0; i < 8; i++) {
        const y = 2 + i * 8;
        const lean = i % 2 ? 22 : -22;
        core.push(cube([x - 0.9, y, -0.9], [1.8, 8.8, 1.8], six('core'), { rotation: [0, 0, lean], pivot: [x, y + 4, 0] }));
        x += i % 2 ? -3 : 3;
      }
      bones.push(bone('core', [0, 0, 0], core));
      bones.push(bone('branch', [0, 0, 0], [
        cube([1.5, 30, -0.5], [1, 8, 1], six('branch'), { rotation: [0, 0, 48], pivot: [2, 34, 0] }),
        cube([-3, 46, -0.5], [1, 7, 1], six('branch'), { rotation: [0, 0, -52], pivot: [-2.5, 49, 0] }),
        cube([0, 14, 0.5], [1, 6, 1], six('branch'), { rotation: [38, 0, 0], pivot: [0.5, 17, 1] }),
      ]));
      bones.push(bone('glow', [0, 0, 0], [
        cube([-7, 0, 0], [14, 66, 0], only('north', 'glow')),
        cube([0, 0, -7], [0, 66, 14], only('east', 'glow')),
      ]));
      bones.push(bone('scorch', [0, 0.06, 0], [cube([-11, 0.06, -11], [22, 0, 22], only('up', 'scorch'))]));
      bones.push(bone('sparks', [0, 1, 0], [[-5, 1, -3], [4, 2, -4], [5, 1, 3], [-4, 2, 4], [0, 3, -6], [1, 1, 6], [-7, 2, 0], [7, 3, 1]].map(([px, py, pz]) => cube([px - 0.6, py, pz - 0.6], [1.2, 1.2, 1.2], six('spark')))));
    } else if (kind.key === 'grave') {
      // Lapida que sale de la tierra, con su monticulo y flores.
      bones.push(bone('dirt', [0, 0, 0], [cube([-6, 0, -1.5], [12, 1.2, 11], six('dirt', { up: 'grass' }))]));
      bones.push(bone('stone', [0, 0, -1], [
        cube([-6.5, 0, -3.5], [13, 2.2, 5], six('stone')),
        cube([-5, 2.2, -2.5], [10, 12, 2.6], six('stone', { north: 'front', south: 'front' })),
        cube([-4, 14.2, -2.5], [8, 1.4, 2.6], six('top')),
        cube([-2.5, 15.6, -2.5], [5, 0.9, 2.6], six('top')),
      ]));
      bones.push(bone('flowers', [0, 1.2, 4], [
        cube([-5, 1.2, 4], [4, 6, 0], only('north', 'poppy'), { rotation: [0, 45, 0], pivot: [-3, 1.2, 4] }),
        cube([-5, 1.2, 4], [4, 6, 0], only('north', 'poppy'), { rotation: [0, -45, 0], pivot: [-3, 1.2, 4] }),
        cube([1.5, 1.2, 6], [4, 6, 0], only('north', 'orchid'), { rotation: [0, 45, 0], pivot: [3.5, 1.2, 6] }),
        cube([1.5, 1.2, 6], [4, 6, 0], only('north', 'orchid'), { rotation: [0, -45, 0], pivot: [3.5, 1.2, 6] }),
      ]));
    } else if (kind.key === 'soul') {
      // Fantasma con alas y aureola que sube girando.
      bones.push(bone('body', [0, 10, 0], [
        cube([-3.5, 12, -3.5], [7, 7, 7], six('ghost', { north: 'face' })),
        cube([-3, 5, -2.5], [6, 7, 5], six('ghost')),
        cube([-2.2, 1.5, -1.8], [4.4, 3.5, 3.6], six('tail')),
        cube([-1.2, -0.5, -0.8], [2.4, 2, 2], six('tail')),
      ]));
      bones.push(bone('wingL', [2.5, 11, 2.5], [cube([2.5, 6, 2.5], [10, 8, 0], { south: s('wing'), north: s('wing') })], `${prefix}${kind.key}_body`));
      bones.push(bone('wingR', [-2.5, 11, 2.5], [cube([-12.5, 6, 2.5], [10, 8, 0], { south: { ...s('wing'), uv_size: [-10, 8], uv: [s('wing').uv[0] + 10, s('wing').uv[1]] }, north: { ...s('wing'), uv_size: [-10, 8], uv: [s('wing').uv[0] + 10, s('wing').uv[1]] } })], `${prefix}${kind.key}_body`));
      bones.push(bone('halo', [0, 21, 0], [cube([-4, 21, -4], [8, 0, 8], only('up', 'halo'))], `${prefix}${kind.key}_body`));
      bones.push(bone('aura', [0, 10, 0], [
        cube([-8, 2, 0], [16, 16, 0], only('north', 'glow')),
        cube([0, 2, -8], [0, 16, 16], only('east', 'glow')),
      ], `${prefix}${kind.key}_body`));
    } else if (kind.key === 'skull') {
      // Calavera que brota del suelo, castañea y se va girando.
      bones.push(bone('ring', [0, 0.08, 0], [cube([-12, 0.08, -12], [24, 0, 24], only('up', 'ring'))]));
      bones.push(bone('head', [0, 9, 0], [
        cube([-5, 8, -5], [10, 9, 10], six('bone', { north: 'face' })),
        cube([-3, 17, 0], [6, 10, 0], only('north', 'flame')),
        cube([0, 17, -3], [0, 10, 6], only('east', 'flame')),
      ]));
      bones.push(bone('jaw', [0, 8, 4], [cube([-4.5, 5, -4.8], [9, 3, 9], six('bone', { north: 'jaw', up: 'dark' }))], `${prefix}${kind.key}_head`));
    } else if (kind.key === 'firework') {
      // Tres cohetes que suben y estallan en esferas de chispas de colores.
      [[-4, 2, 'gold'], [3.5, -3, 'cyan'], [0.5, 5, 'pink']].forEach(([x, z, color], i) => {
        bones.push(bone(`r${i}`, [x, 0, z], [
          cube([x - 0.6, 2, z - 0.6], [1.2, 3.2, 1.2], six('rocket')),
          cube([x - 0.15, 0, z - 0.15], [0.3, 2, 0.3], six('stick')),
          cube([x - 0.8, -3, z], [1.6, 5, 0], only('north', 'trail')),
          cube([x, -3, z - 0.8], [0, 5, 1.6], only('east', 'trail')),
        ]));
        const top = 38 + i * 5;
        const sparks = [];
        for (let n = 0; n < 18; n++) {
          const yy = 1 - (2 * (n + 0.5)) / 18;
          const r = Math.sqrt(1 - yy * yy);
          const a = n * 2.39996;
          sparks.push(cube([x + Math.cos(a) * r - 0.45, top + yy - 0.45, z + Math.sin(a) * r - 0.45], [0.9, 0.9, 0.9], six(color)));
        }
        bones.push(bone(`b${i}`, [x, top, z], sparks));
        bones.push(bone(`f${i}`, [x, top, z], [cube([x - 6, top - 6, z], [12, 12, 0], only('north', 'flash')), cube([x, top - 6, z - 6], [0, 12, 12], only('east', 'flash'))]));
      });
    } else if (kind.key === 'crown') {
      // Corona dorada de 12 lados con puntas y gemas, y anillo de destellos.
      const band = [];
      for (let i = 0; i < 12; i++) {
        const a = i * 30;
        band.push(cube([-1.25, 16, -4.9], [2.5, 3.2, 0.6], { north: s('band'), south: s('gold'), up: s('gold'), down: s('gold'), east: s('gold'), west: s('gold') }, { rotation: [0, a, 0], pivot: [0, 17, 0] }));
        if (i % 2 === 0) {
          band.push(cube([-0.6, 19.2, -4.8], [1.2, 2.4, 0.5], six('gold'), { rotation: [0, a, 0], pivot: [0, 17, 0] }));
          band.push(cube([-0.45, 21.4, -4.85], [0.9, 0.9, 0.6], six(i % 4 === 0 ? 'gem' : 'gemb'), { rotation: [0, a, 0], pivot: [0, 17, 0] }));
        }
      }
      bones.push(bone('band', [0, 17, 0], band));
      bones.push(bone('ring', [0, 0.1, 0], [cube([-12, 0.1, -12], [24, 0, 24], only('up', 'ring'))]));
      bones.push(bone('sparks', [0, 17, 0], [0, 60, 120, 180, 240, 300].map((a, i) => cube([-0.5, 14 + (i % 3) * 3, -8], [1, 1, 1], six('spark'), { rotation: [0, a, 0], pivot: [0, 17, 0] }))));
    } else if (kind.key === 'coffin') {
      // Ataud de pie que sale de la tierra, se abre y vuelve a hundirse.
      bones.push(bone('dirt', [0, 0, 0], [cube([-6, 0, -5], [12, 1, 10], six('dirt'))]));
      bones.push(bone('glow', [0, 0.2, 0], [cube([-8, 0.15, -8], [16, 0, 16], only('up', 'glow'))]));
      bones.push(bone('body', [0, 0, 0], [
        cube([-2.6, 0, -1.6], [5.2, 9, 3.2], six('wood', { north: 'velvet' })),
        cube([-3.2, 9, -1.6], [6.4, 4.4, 3.2], six('wood', { north: 'velvet' })),
        cube([-2.6, 13.4, -1.6], [5.2, 1.6, 3.2], six('wood', { north: 'velvet' })),
        cube([3.2, 8, -0.5], [0.6, 0.6, 1], six('gold')),
        cube([-3.8, 8, -0.5], [0.6, 0.6, 1], six('gold')),
      ]));
      bones.push(bone('lid', [-3.2, 7, -1.9], [
        cube([-3.2, 0, -2.0], [6.4, 15, 0.4], six('wood', { north: 'lid' })),
      ], `${prefix}${kind.key}_body`));
    } else if (kind.key === 'angel') {
      // Columna de luz, alas que se abren y suben con su aureola.
      bones.push(bone('beam', [0, 0, 0], [cube([-5, 0, 0], [10, 64, 0], only('north', 'beam')), cube([0, 0, -5], [0, 64, 10], only('east', 'beam'))]));
      bones.push(bone('glow', [0, 0.2, 0], [cube([-10, 0.15, -10], [20, 0, 20], only('up', 'glow'))]));
      bones.push(bone('rise', [0, 18, 0], [cube([-3.5, 30, -3.5], [7, 0, 7], only('up', 'halo'))]));
      const fe = s('feather');
      bones.push(bone('wingL', [1, 22, 2], [cube([1, 15, 2], [14, 12, 0], { north: fe })], `${prefix}${kind.key}_rise`));
      bones.push(bone('wingR', [-1, 22, 2], [cube([-15, 15, 2], [14, 12, 0], { north: { uv: [fe.uv[0] + fe.uv_size[0], fe.uv[1]], uv_size: [-fe.uv_size[0], fe.uv_size[1]] } })], `${prefix}${kind.key}_rise`));
    } else if (kind.key === 'daze') {
      // Estrellas que giran sobre la cabeza del golpeado.
      const stars = [];
      for (let i = 0; i < 5; i++) {
        const a = (i / 5) * 360;
        const r = (a * Math.PI) / 180;
        const [x, z] = [Math.sin(r) * 6.5, Math.cos(r) * 6.5];
        stars.push(cube([x - 1.6, 33 + (i % 2) * 1.2, z], [3.2, 3.2, 0], only('north', 'star'), { rotation: [0, a, 0], pivot: [x, 34.6, z] }));
        stars.push(cube([x, 33 + (i % 2) * 1.2, z - 1.6], [0, 3.2, 3.2], only('east', 'star'), { rotation: [0, a, 0], pivot: [x, 34.6, z] }));
      }
      bones.push(bone('stars', [0, 34, 0], stars));
      bones.push(bone('ring', [0, 33.5, 0], [cube([-8, 33.5, -8], [16, 0, 16], only('up', 'ring'))]));
    } else if (kind.key === 'slash') {
      // Dos tajos cruzados delante y detras del pecho.
      const cut = [];
      for (const z of [-3.2, 3.2]) {
        for (const angle of [32, -32]) cut.push(cube([-8, 17, z], [16, 4, 0], { north: s('slash'), south: s('slash') }, { rotation: [0, 0, angle], pivot: [0, 19, z] }));
      }
      bones.push(bone('cut', [0, 19, 0], cut));
      bones.push(bone('glow', [0, 19, 0], [cube([-8, 13, -3.6], [16, 12, 0], { north: s('glow'), south: s('glow') }), cube([-8, 13, 3.6], [16, 12, 0], { north: s('glow'), south: s('glow') })]));
    } else if (kind.key === 'impact') {
      // Onda de choque a la altura del pecho y esquirlas que salen.
      bones.push(bone('ring', [0, 17, 0], [cube([-10, 17, -10], [20, 0, 20], { up: s('ring'), down: s('ring') })]));
      bones.push(bone('shards', [0, 18, 0], Array.from({ length: 8 }, (_, i) => {
        const r = (i / 8) * Math.PI * 2;
        const [x, z] = [Math.sin(r) * 5, Math.cos(r) * 5];
        return cube([x - 0.7, 16 + (i % 3), z - 0.7], [1.4, 1.4, 1.4], six(i % 2 ? 'shard' : 'spark'));
      })));
    }
  }
  return bones;
}

// Animacion de 3 s desde el instante de la muerte; se queda en el ultimo
// fotograma (lo que debe desaparecer acaba con escala 0).
// Golpe: dura lo mismo que el parpadeo rojo (unos 0.35 s).
export function hitAnimation(prefix) {
  const k = (pairs) => Object.fromEntries(pairs.map(([t, v]) => [t.toFixed(2), v]));
  const v3 = (n) => [n, n, n];
  const P = prefix;
  return {
    animation_length: 0.4,
    loop: 'hold_on_last_frame',
    bones: {
      [`${P}daze_stars`]: { rotation: [0, 'query.anim_time * 540', 0], scale: k([[0, v3(0.2)], [0.08, v3(1.2)], [0.14, v3(1)], [0.3, v3(1)], [0.4, v3(0)]]) },
      [`${P}daze_ring`]: { scale: k([[0, v3(0)], [0.1, v3(1)], [0.3, v3(1.1)], [0.4, v3(0)]]) },
      [`${P}slash_cut`]: { scale: k([[0, [0.1, 1, 1]], [0.06, [1.25, 1, 1]], [0.2, [1, 0.8, 1]], [0.36, [1.1, 0, 1]]]) },
      [`${P}slash_glow`]: { scale: k([[0, v3(0)], [0.05, v3(1)], [0.25, v3(0.6)], [0.36, v3(0)]]) },
      [`${P}impact_ring`]: { scale: k([[0, v3(0.2)], [0.25, v3(1.5)], [0.38, v3(0)]]) },
      [`${P}impact_shards`]: { scale: k([[0, v3(0.4)], [0.3, v3(2.6)], [0.4, v3(0)]]), position: k([[0, [0, 0, 0]], [0.3, [0, 3, 0]]]) },
    },
  };
}

// Animacion de unos 4.5 s desde el instante de la muerte; se queda en el
// ultimo fotograma (lo que debe desaparecer acaba con escala 0). Los
// movimientos usan curvas suaves (catmull-rom); los destellos, cortes secos.
export function dfxAnimation(prefix) {
  const k = (pairs, smooth = true) => Object.fromEntries(pairs.map(([t, v]) => [t.toFixed(2), smooth ? { post: v, lerp_mode: 'catmullrom' } : v]));
  const v3 = (n) => [n, n, n];
  const P = prefix;
  const bones = {
    // El cuerpo cae de lado: el efecto se mantiene recto.
    [P]: { rotation: [0, 0, '-math.min(1, math.sqrt(math.max(0, query.death_ticks) / 20 * 1.6)) * 90'] },
    // Rayo: dos descargas, chispas y marca quemada que se apaga al final.
    [`${P}bolt_core`]: { scale: k([[0, v3(0)], [0.04, v3(1)], [0.14, v3(0)], [0.22, v3(1)], [0.36, v3(1)], [0.42, v3(0)], [1.3, v3(0)], [1.34, v3(1)], [1.5, v3(1)], [1.56, v3(0)]], false) },
    [`${P}bolt_branch`]: { scale: k([[0, v3(0)], [0.04, v3(1)], [0.14, v3(0)], [0.22, v3(1)], [0.36, v3(0)], [1.34, v3(1)], [1.5, v3(0)]], false) },
    [`${P}bolt_glow`]: { scale: k([[0, [0, 1, 0]], [0.04, [1.3, 1, 1.3]], [0.5, [0.6, 1, 0.6]], [1.3, [0.2, 1, 0.2]], [1.36, [1.2, 1, 1.2]], [1.8, [0, 1, 0]]], false) },
    [`${P}bolt_scorch`]: { scale: k([[0, v3(0)], [0.06, v3(0.4)], [0.4, v3(1.12)], [0.6, v3(1)], [3.8, v3(1)], [4.5, v3(0)]]) },
    [`${P}bolt_sparks`]: {
      scale: k([[0, v3(0)], [0.06, v3(0.6)], [1.0, v3(2.6)], [1.6, v3(0)]]),
      position: k([[0, [0, 0, 0]], [0.06, [0, 2, 0]], [0.8, [0, 7, 0]], [1.6, [0, 0, 0]]]),
    },
    // Lapida: sale de la tierra con rebote y se queda.
    [`${P}grave_dirt`]: { scale: k([[0, v3(0)], [0.2, v3(0)], [0.6, v3(1.1)], [0.75, v3(1)]]) },
    [`${P}grave_stone`]: {
      position: k([[0, [0, -18, 0]], [0.3, [0, -18, 0]], [0.85, [0, 1.4, 0]], [1.05, [0, 0, 0]]]),
      rotation: k([[0.85, [0, 0, 0]], [0.95, [0, 0, 4]], [1.05, [0, 0, -3]], [1.2, [0, 0, 1.5]], [1.35, [0, 0, 0]]]),
      scale: k([[0, v3(0)], [0.29, v3(0)], [0.3, v3(1)]], false),
    },
    [`${P}grave_flowers`]: { scale: k([[0, v3(0)], [1.1, v3(0)], [1.45, v3(1.2)], [1.6, v3(1)]]) },
    // Alma: sube girando 4.5 s y se desvanece.
    [`${P}soul_body`]: {
      position: k([[0, [0, 0, 0]], [1.1, [1.5, 12, 0]], [2.2, [-1.5, 28, 0]], [3.3, [1.5, 46, 0]], [4.5, [0, 64, 0]]]),
      rotation: k([[0, [0, 0, 0]], [4.5, [0, 720, 0]]]),
      scale: k([[0, v3(0)], [0.45, v3(1.15)], [0.65, v3(1)], [3.7, v3(1)], [4.5, v3(0)]]),
    },
    [`${P}soul_wingL`]: { rotation: [0, '-20 - math.sin(query.anim_time * 700) * 34', 0] },
    [`${P}soul_wingR`]: { rotation: [0, '20 + math.sin(query.anim_time * 700) * 34', 0] },
    [`${P}soul_halo`]: { position: [0, 'math.sin(query.anim_time * 300) * 0.6', 0] },
    [`${P}soul_aura`]: { rotation: [0, 'query.anim_time * -200', 0] },
    // Calavera: brota, castanea, flota y se va girando.
    [`${P}skull_ring`]: {
      scale: k([[0, v3(0)], [0.5, v3(1.1)], [0.65, v3(1)], [3.7, v3(1)], [4.3, v3(0)]]),
      rotation: [0, 'query.anim_time * 60', 0],
    },
    [`${P}skull_head`]: {
      position: k([[0, [0, -10, 0]], [0.5, [0, 3, 0]], [0.7, [0, 1.5, 0]], [1.7, [0, 2.5, 0]], [2.8, [0, 1.5, 0]], [3.3, [0, 3, 0]], [4.3, [0, 28, 0]]]),
      rotation: k([[0, [0, 0, 0]], [3.1, [0, 0, 0]], [4.3, [0, 720, 0]]]),
      scale: k([[0, v3(0)], [0.42, v3(1.35)], [0.62, v3(1)], [3.3, v3(1.1)], [4.3, v3(0)]]),
    },
    [`${P}skull_jaw`]: { rotation: k([[0.7, [0, 0, 0]], [0.85, [28, 0, 0]], [1.0, [0, 0, 0]], [1.15, [28, 0, 0]], [1.3, [0, 0, 0]], [1.45, [28, 0, 0]], [1.6, [0, 0, 0]], [1.75, [28, 0, 0]], [1.95, [0, 0, 0]]], false) },
    // Corona de victoria: cae, rebota, gira y se eleva.
    [`${P}crown_band`]: {
      position: k([[0, [0, 32, 0]], [0.55, [0, -2, 0]], [0.75, [0, 1.2, 0]], [0.95, [0, 0, 0]], [3.6, [0, 0.8, 0]], [4.5, [0, 26, 0]]]),
      rotation: [0, 'query.anim_time * 70', 0],
      scale: k([[0, v3(0.6)], [0.55, v3(1.1)], [0.75, v3(1)], [3.9, v3(1)], [4.5, v3(0)]]),
    },
    [`${P}crown_ring`]: { scale: k([[0, v3(0)], [0.55, v3(0)], [0.9, v3(1.2)], [1.1, v3(1)], [3.8, v3(1)], [4.4, v3(0)]]), rotation: [0, 'query.anim_time * -40', 0] },
    [`${P}crown_sparks`]: { rotation: [0, 'query.anim_time * 160', 0], scale: k([[0, v3(0)], [0.8, v3(0)], [1.1, v3(1)], [3.9, v3(1)], [4.4, v3(0)]]) },
    // Ataud: sale, se abre (terciopelo), se cierra y se hunde.
    [`${P}coffin_dirt`]: { scale: k([[0, v3(0)], [0.3, v3(1.1)], [0.45, v3(1)], [4.0, v3(1)], [4.5, v3(0)]]) },
    [`${P}coffin_glow`]: { scale: k([[0, v3(0)], [1.6, v3(0)], [2.0, v3(1)], [3.2, v3(1)], [3.5, v3(0)]]) },
    [`${P}coffin_body`]: {
      position: k([[0, [0, -18, 0]], [0.9, [0, 0.8, 0]], [1.1, [0, 0, 0]], [3.7, [0, 0, 0]], [4.5, [0, -18, 0]]]),
      rotation: k([[0, [0, 0, 0]], [1.1, [0, 0, 0]], [1.2, [0, 0, 3]], [1.3, [0, 0, -2]], [1.4, [0, 0, 0]]]),
    },
    [`${P}coffin_lid`]: { rotation: k([[0, [0, 0, 0]], [1.5, [0, 0, 0]], [2.1, [0, -110, 0]], [3.1, [0, -110, 0]], [3.5, [0, 0, 0]]]) },
    // Ascension: columna de luz, alas que se abren y suben.
    [`${P}angel_beam`]: { scale: k([[0, [0, 0, 0]], [0.5, [1.2, 1, 1.2]], [0.8, [1, 1, 1]], [3.6, [0.8, 1, 0.8]], [4.5, [0, 1, 0]]]) },
    [`${P}angel_glow`]: { scale: k([[0, v3(0)], [0.5, v3(1.1)], [0.8, v3(1)], [3.8, v3(1)], [4.5, v3(0)]]), rotation: [0, 'query.anim_time * 50', 0] },
    [`${P}angel_rise`]: {
      position: k([[0, [0, 0, 0]], [1.5, [0, 0, 0]], [4.5, [0, 44, 0]]]),
      scale: k([[0, v3(0)], [0.6, v3(0)], [1.0, v3(1.1)], [1.2, v3(1)], [3.9, v3(1)], [4.5, v3(0)]]),
    },
    [`${P}angel_wingL`]: { rotation: [0, '(query.anim_time < 1.4 ? 80 - math.clamp((query.anim_time - 0.6) / 0.8, 0, 1) * 70 : 10) + math.sin(query.anim_time * 220) * (query.anim_time > 1.4 ? 22 : 0)', 0] },
    [`${P}angel_wingR`]: { rotation: [0, '-(query.anim_time < 1.4 ? 80 - math.clamp((query.anim_time - 0.6) / 0.8, 0, 1) * 70 : 10) - math.sin(query.anim_time * 220) * (query.anim_time > 1.4 ? 22 : 0)', 0] },
  };
  // Fuegos artificiales: cada cohete sale con retraso, sube y estalla.
  for (let i = 0; i < 3; i++) {
    const t0 = 0.15 + i * 0.45;
    const top = 38 + i * 5;
    const boom = t0 + 0.8;
    bones[`${P}firework_r${i}`] = {
      position: k([[0, [0, 0, 0]], [t0, [0, 0, 0]], [boom, [0, top - 3, 0]]], false),
      scale: k([[0, v3(1)], [boom - 0.01, v3(1)], [boom, v3(0)]], false),
    };
    bones[`${P}firework_b${i}`] = {
      scale: k([[0, v3(0)], [boom, v3(0.2)], [boom + 0.35, v3(7)], [boom + 0.9, v3(9)], [boom + 1.8, v3(0)]]),
      position: k([[0, [0, 0, 0]], [boom + 0.4, [0, 0, 0]], [boom + 1.8, [0, -8, 0]]]),
      rotation: [0, `query.anim_time * ${40 + i * 25}`, 0],
    };
    bones[`${P}firework_f${i}`] = { scale: k([[0, v3(0)], [boom, v3(0)], [boom + 0.08, v3(1.4)], [boom + 0.4, v3(0)]], false) };
  }
  return { animation_length: 4.5, loop: 'hold_on_last_frame', bones };
}
