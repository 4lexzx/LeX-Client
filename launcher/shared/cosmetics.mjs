import { DFX_KINDS, dfxBones } from './deathfx.mjs';
import { KILL_KINDS, killBones } from './killfx.mjs';

// Catalogo, atlas y geometria de los cosmeticos. La geometria de TODOS va
// siempre en el juego; un cosmetico se muestra u oculta repintando su region
// del atlas (alfa 0 = invisible), por eso se puede cambiar en vivo.
export const DENSITY = 12;
export const ATLAS = { w: 161, h: 184 }; // 1932x2208 (112-159: cosmeticos por archivos; 160-183: mas animaciones de muerte) px: tamano que la DLL reconoce

export const SLOTS = [
  { id: 'cape', label: 'Capas' },
  { id: 'wings', label: 'Alas' },
  { id: 'head', label: 'Cabeza' },
  { id: 'pet', label: 'Mascotas' },
  { id: 'aura', label: 'Auras' },
  { id: 'trail', label: 'Partículas' },
  { id: 'hit', label: 'Golpes' },
  { id: 'kill', label: 'Kills' },
  { id: 'death', label: 'Muertes' },
];

const CAPE = [0, 0, 21, 16];
export const CAPE_PLAIN = [0, 17, 21, 16]; // la misma capa, sin fisica
const WINGS = [22, 0, 30, 30];
const DRAGON = [53, 0, 34, 30]; // membrana 30x30 y hueso 4x30
const AURA = [88, 0, 32, 48]; // suelo 32x32, muro 32x12, cristal 4x4
const HAT = [121, 0, 14, 22];
const WIZARD = [136, 0, 20, 24];
const HALO = [0, 34, 12, 12];
const CROWN = [13, 34, 10, 6];
const HORNS = [24, 34, 8, 8];
const EARS = [33, 34, 4, 5];
const PHONES = [38, 34, 8, 8];
const BAND = [0, 49, 40, 8];
const GOGGLES = [41, 49, 20, 8];
const MINI = [18, 58, 15, 15];
// Texturas del propio juego, 1 texel = 1 pixel del atlas.
export const PET_RECT = {
  dragon: [62, 49, 30, 22], // textura del juego + fuego del aliento (8x6 a la derecha) cat: [85, 49, 6, 3], fox: [92, 49, 6, 3], parrot: [99, 49, 3, 3], allay: [103, 49, 3, 3], bee: [85, 53, 6, 6],
};

const item = (slot, key, name, rect, { frames = 1, fps = 12, geo = key } = {}) => ({ id: `${slot}-${key}`, slot, name, rect, frames, fps, geo });

// Las particulas usan pocos emisores compartidos; la DLL pinta en cada uno el
// dibujo elegido. El indice es el canal: su alto en pixeles es TRAIL.h + canal.
export const TRAIL = { w: 264, h: 40 };
export const CHANNELS_FX = ['rise', 'fall', 'burst', 'hit', 'death', 'soul'];
const fx = (slot, key, name, channel, art = key) => ({ id: `${slot}-${key}`, slot, name, rect: [0, 0, 22, 3], frames: 1, fps: 12, geo: 'trail', trail: CHANNELS_FX.indexOf(channel), art });

const pet = (key, name, kind, folder, names, tint = null, breath = null) => ({ ...item('pet', key, name, PET_RECT[kind], { geo: `pet${kind}` }), source: { folder, names, tint, breath } });
const DRAGON_TEX = ['dragon', ['dragon.tga']];
export const VANILLA_PETS = [
  pet('enddragon', 'Dragón del End', 'dragon', ...DRAGON_TEX, null, [190, 70, 255]),
];

export const COSMETICS = [
  item('cape', 'lynx', 'Lince', CAPE),
  item('cape', 'gold', 'Lince dorado', CAPE),
  item('cape', 'noir', 'Lince negro', CAPE),
  item('cape', 'aurora', 'Aurora', CAPE, { frames: 24 }),
  item('cape', 'galaxy', 'Galaxia', CAPE, { frames: 24 }),
  item('cape', 'inferno', 'Infierno', CAPE, { frames: 24, fps: 14 }),
  item('cape', 'storm', 'Tormenta', CAPE, { frames: 24, fps: 14 }),
  item('cape', 'rgb', 'RGB', CAPE, { frames: 24 }),
  item('cape', 'glass', 'Vidriera', CAPE),
  item('cape', 'cherry', 'Cerezo', CAPE, { frames: 24, fps: 8 }),
  item('cape', 'ocean', 'Océano', CAPE, { frames: 24 }),
  item('cape', 'matrix', 'Código', CAPE, { frames: 24, fps: 14 }),
  item('cape', 'synth', 'Neón', CAPE, { frames: 24, fps: 14 }),
  item('cape', 'hex', 'Energía', CAPE, { frames: 24 }),
  item('cape', 'carbon', 'Carbono', CAPE),
  item('cape', 'sunset', 'Atardecer', CAPE),
  item('cape', 'moon', 'Luna de sangre', CAPE),
  item('cape', 'ice', 'Cristal de hielo', CAPE, { frames: 24, fps: 8 }),
  item('cape', 'royal', 'Real', CAPE),
  item('cape', 'endcape', 'Vacío del End', CAPE, { frames: 24, fps: 8 }),
  item('wings', 'end', 'Dragón del End', DRAGON, { geo: 'dragon' }),
  item('wings', 'frost', 'Dragón helado', DRAGON, { geo: 'dragon' }),
  item('wings', 'ender', 'Dragón ender', DRAGON, { geo: 'dragon' }),
  item('wings', 'magma', 'Dragón magma', DRAGON, { geo: 'dragon' }),
  item('wings', 'gold', 'Dragón dorado', DRAGON, { geo: 'dragon' }),
  item('wings', 'angel', 'Ángel', WINGS, { geo: 'wing' }),
  item('wings', 'raven', 'Cuervo', WINGS, { geo: 'wing' }),
  item('wings', 'seraph', 'Serafín', WINGS, { geo: 'wing' }),
  item('wings', 'demon', 'Demonio', WINGS, { geo: 'wing' }),
  item('wings', 'morpho', 'Mariposa', WINGS, { frames: 12, fps: 10, geo: 'wing' }),
  item('wings', 'minirgb', 'Alitas End RGB', MINI, { frames: 24, geo: 'mini' }),
  item('wings', 'miniend', 'Alitas del End', MINI, { geo: 'mini' }),
  item('wings', 'minifrost', 'Alitas de hielo', MINI, { geo: 'mini' }),
  item('wings', 'minimagma', 'Alitas de magma', MINI, { geo: 'mini' }),
  item('wings', 'minigold', 'Alitas doradas', MINI, { geo: 'mini' }),
  item('wings', 'miniangel', 'Alitas de ángel', MINI, { geo: 'mini' }),
  item('wings', 'miniangelrgb', 'Alitas de ángel RGB', MINI, { frames: 24, geo: 'mini' }),
  item('wings', 'minimorpho', 'Alitas de mariposa', MINI, { frames: 12, fps: 10, geo: 'mini' }),
  item('head', 'band', 'Bandana roja', BAND, { geo: 'band' }),
  item('head', 'bandlex', 'Bandana negra LeX', BAND, { geo: 'band' }),
  item('head', 'bandblue', 'Bandana azul LeX', BAND, { geo: 'band' }),
  item('head', 'bandwhite', 'Bandana blanca LeX', BAND, { geo: 'band' }),
  item('head', 'bandrgb', 'Bandana RGB', BAND, { frames: 24, geo: 'band' }),
  item('head', 'halo', 'Halo', HALO, { frames: 8, fps: 10 }),
  item('head', 'halodark', 'Halo oscuro', HALO, { frames: 8, fps: 10, geo: 'halo' }),
  item('head', 'crown', 'Corona', CROWN),
  item('head', 'crownice', 'Corona de hielo', CROWN, { geo: 'crown' }),
  item('head', 'flowers', 'Corona de flores', CROWN, { geo: 'crown' }),
  item('head', 'horns', 'Cuernos', HORNS),
  item('head', 'hornsend', 'Cuernos del End', HORNS, { geo: 'horns' }),
  item('head', 'hat', 'Sombrero', HAT),
  item('head', 'wizard', 'Sombrero de mago', WIZARD),
  item('head', 'cat', 'Orejas de gato', EARS),
  item('head', 'fox', 'Orejas de zorro', EARS, { geo: 'cat' }),
  item('head', 'phones', 'Auriculares RGB', PHONES, { frames: 24 }),
  item('head', 'goggles', 'Gafas de aviador', GOGGLES),
  ...VANILLA_PETS,
  item('aura', 'rune', 'Runas', AURA, { frames: 12, fps: 10, geo: 'aura' }),
  item('aura', 'ember', 'Ascuas', AURA, { frames: 12, fps: 12, geo: 'aura' }),
  item('aura', 'void', 'Vacío', AURA, { frames: 12, fps: 10, geo: 'aura' }),
  item('aura', 'frost', 'Escarcha', AURA, { frames: 12, fps: 10, geo: 'aura' }),
  // Animaciones 3D de golpe, baja y muerte.
  ...[...DFX_KINDS, ...KILL_KINDS].map((k) => ({ id: `${k.slot}-${k.key}`, slot: k.slot, name: k.name, rect: k.rect, frames: 1, fps: 12, geo: 'dfx', kind: k.key })),
  fx('trail', 'fire', 'Fuego', 'rise'),
  fx('trail', 'bolt', 'Rayos', 'burst'),
  fx('trail', 'water', 'Agua', 'fall'),
  fx('trail', 'frost', 'Nieve', 'fall'),
  fx('trail', 'hearts', 'Corazones', 'rise'),
  fx('trail', 'stars', 'Estrellas', 'burst'),
  fx('trail', 'petals', 'Pétalos', 'fall'),
  fx('trail', 'smoke', 'Humo', 'rise'),
  fx('trail', 'ender', 'Ender', 'rise'),
  fx('trail', 'rgb', 'RGB', 'burst'),
  fx('trail', 'souls', 'Almas', 'rise'),
  fx('trail', 'notes', 'Notas', 'rise'),
];

const FACES = ['north', 'south', 'east', 'west', 'up', 'down'];
const SIDES = ['north', 'south', 'east', 'west'];
const faces = (list, uv, uv_size) => Object.fromEntries(list.map((f) => [f, { uv, uv_size }]));
const all = (uv, uv_size) => faces(FACES, uv, uv_size);
// Un plano con dos caras coincidentes parpadea: se deja solo la primera.
const cube = (origin, size, uv, extra = {}) => ({
  origin, size, ...extra,
  uv: size.includes(0) ? Object.fromEntries(Object.entries(uv).slice(0, 1)) : uv,
});

// --- capa: 8 tiras encadenadas para que ondee, y otra copia sin fisica ---
const STRIPS = 8;
function cape(name, [u0, v0], strips) {
  const tall = 16 / strips;
  return Array.from({ length: strips }, (_, i) => {
    const top = 24 - i * tall;
    const v = v0 + i * tall;
    return {
      name: `${name}${i}`,
      parent: i === 0 ? 'body' : `${name}${i - 1}`,
      pivot: [0, top, 2.4],
      // Cada tira baja un poco por detras de la siguiente, con su mismo
      // dibujo: al ondear no quedan rendijas entre tiras.
      cubes: [cube([-5, top - tall - (i < strips - 1 ? OVERLAP : 0), 2.4 + (i % 2) * 0.03], [10, tall + (i < strips - 1 ? OVERLAP : 0), 0.5], {
        south: { uv: [u0, v], uv_size: [10, tall + (i < strips - 1 ? OVERLAP : 0)] },
        north: { uv: [u0 + 10, v], uv_size: [10, tall + (i < strips - 1 ? OVERLAP : 0)] },
        east: { uv: [u0 + 20, v], uv_size: [1, tall + (i < strips - 1 ? OVERLAP : 0)] },
        west: { uv: [u0 + 20, v], uv_size: [1, tall + (i < strips - 1 ? OVERLAP : 0)] },
        ...(i === 0 ? { up: { uv: [u0, v0], uv_size: [10, 1] } } : {}),
        ...(i === strips - 1 ? { down: { uv: [u0, v0 + 15], uv_size: [10, 1] } } : {}),
      })],
    };
  });
}
const OVERLAP = 0.5;
const capeBones = [...cape('lexCape', CAPE, STRIPS), ...cape('lexPlain', CAPE_PLAIN, 1)];

// --- alas planas: dos tramos por lado; la raiz del dibujo esta a la izquierda ---
const FOLD = 12;
const membrane = (side, [u0, v0], from, width, z) => {
  const x = side > 0 ? 1 + from : -1 - from - width;
  const straight = { uv: [u0 + from, v0], uv_size: [width, 30] };
  const flipped = { uv: [u0 + from + width, v0], uv_size: [-width, 30] };
  return cube([x, 2, z], [width, 30, 0], side > 0 ? { north: straight, south: flipped } : { north: flipped, south: straight });
};
function wingBones(side) {
  const name = side > 0 ? 'lexWingL' : 'lexWingR';
  // Tres tramos (hombro, codo, punta): cada uno sigue al anterior con retraso.
  return [
    { name, parent: 'body', pivot: [side, 20, 2.9], cubes: [membrane(side, WINGS, 0, 10, 2.9)] },
    { name: `${name}2`, parent: name, pivot: [side * 11, 20, 2.9], cubes: [membrane(side, WINGS, 10, 10, 2.9)] },
    { name: `${name}3`, parent: `${name}2`, pivot: [side * 21, 20, 2.9], cubes: [membrane(side, WINGS, 20, 10, 2.9)] },
  ];
}

// --- alas de dragon: membrana y huesos con volumen ---
function dragonBones(side) {
  const name = side > 0 ? 'lexDragonL' : 'lexDragonR';
  const [u0, v0] = [DRAGON[0] + 30, DRAGON[1]];
  const bone = (v, h) => all([u0, v0 + v], [4, h]);
  const at = (x, w) => (side > 0 ? x : -x - w);
  // Los dedos salen de la muneca, como en el dibujo de la membrana.
  const finger = (length, thick, angle, v) => cube([at(13, length), 28.6 - thick / 2, 3.4 - thick / 2], [length, thick, thick], bone(v, 6), { pivot: [side * 13, 28.6, 3.4], rotation: [0, 0, side * angle] });
  return [
    {
      name, parent: 'body', pivot: [side, 20, 3.4],
      cubes: [
        membrane(side, DRAGON, 0, FOLD, 3.4),
        cube([at(0, 14.6), 18.4, 2.2], [14.6, 2.4, 2.4], bone(0, 8), { pivot: [side, 19.6, 3.4], rotation: [0, 0, side * -37] }),
        cube([at(0.4, 3.6), 17.6, 1.8], [3.6, 4, 3.2], bone(8, 6)),
      ],
    },
    {
      name: `${name}2`, parent: name, pivot: [side * 13, 24, 3.4],
      cubes: [
        membrane(side, DRAGON, FOLD, 30 - FOLD, 3.4),
        finger(17.4, 1.5, 4, 14), finger(19.4, 1.2, 36, 14), finger(22.6, 1.1, 70, 14),
        cube([at(12, 2.4), 27.4, 2.2], [2.4, 2.4, 2.4], bone(8, 6)),
        cube([at(12.6, 1), 29.6, 2.9], [1, 2.8, 1], bone(22, 6), { pivot: [side * 13, 29.6, 3.4], rotation: [-14, 0, side * 16] }),
      ],
    },
  ];
}

// --- cabeza: una pieza por cosmetico, todas presentes a la vez ---
const head = (name, cubes, extra = {}) => ({ name, parent: 'head', pivot: [0, 24, 0], cubes, ...extra });
const haloUv = { uv: HALO.slice(0, 2), uv_size: [12, 12] };
const crownUv = { uv: CROWN.slice(0, 2), uv_size: [10, 6] };
const hornUv = (i) => all([HORNS[0], HORNS[1] + 6.5 - i * 1.5], [2, 1.5]);
const horn = (s) => [
  [[3.4, 30], [2.6, 2, 2.6]], [[5.0, 31], [2.2, 2, 2.2]], [[6.2, 32.4], [1.8, 2, 1.8]],
  [[6.6, 34], [1.4, 2, 1.4]], [[6.4, 35.6], [0.9, 1.8, 0.9]],
].map(([[x, y], size], i) => cube([s > 0 ? x : -x - size[0], y, -size[2] / 2 - 1], size, hornUv(i)));
// Orejas con grosor: la cara interna va delante y el borde alrededor.
const ear = (x, tilt) => [
  cube([x, 31, -2], [4, 1.6, 1.6], faces(FACES, [EARS[0] + 0.2, EARS[1] + 3.4], [3.6, 1.6])),
  cube([x + 0.5, 32.6, -1.8], [3, 1.6, 1.3], faces(FACES, [EARS[0] + 0.7, EARS[1] + 2], [2.6, 1.4])),
  cube([x + 1.1, 34.2, -1.6], [1.8, 1.4, 1], faces(FACES, [EARS[0] + 1.2, EARS[1] + 0.8], [1.6, 1.2])),
  cube([x + 1.6, 35.6, -1.4], [0.8, 0.9, 0.7], faces(FACES, [EARS[0] + 1.6, EARS[1]], [0.8, 0.8])),
].map((c) => ({ ...c, pivot: [x + 2, 31, -1], rotation: [0, 0, tilt] }));
const CUP = { uv: [PHONES[0], PHONES[1] + 2.4], uv_size: [6.4, 5.6] };
const DARK = { uv: PHONES.slice(0, 2), uv_size: [8, 1] };
const dark = Object.fromEntries(FACES.map((f) => [f, DARK]));
const bandFront = { uv: BAND.slice(0, 2), uv_size: [20, 6.2] };
const bandCloth = { uv: [BAND[0] + 20, BAND[1]], uv_size: [20, 6.2] };
const bandTail = { uv: [BAND[0], BAND[1] + 6.4], uv_size: [12, 1.5] };
const bandEdge = { uv: [BAND[0] + 20, BAND[1] + 6.5], uv_size: [12, 1] };
const W = (u, v, w, h) => ({ uv: [WIZARD[0] + u, WIZARD[1] + v], uv_size: [w, h] });
const wizardCone = [[8, 3, 33.2], [6.6, 3, 36.2], [5, 2.8, 39.2], [3.4, 2.6, 42], [2, 2.4, 44.6]]
  .map(([w, h, y], i) => cube([-w / 2, y - 0.6, -w / 2 + i * 0.35], [w, h, w], {
    ...Object.fromEntries(SIDES.map((f) => [f, W(0, 20 - i * 1.6 - 3.2, 8, 3)])), up: W(10, 18, 4, 4),
  }, { pivot: [0, y, 0], rotation: [-i * 4, 0, 0] }));
const G = (u, v, w, h) => ({ uv: [GOGGLES[0] + u, GOGGLES[1] + v], uv_size: [w, h] });
const lens = (x) => cube([x, 27.2, -5.2], [3.4, 2.8, 1.2], { north: G(0, 0, 8, 6), ...Object.fromEntries(['south', 'east', 'west', 'up', 'down'].map((f) => [f, G(9, 0, 4, 4)])) });

const headBones = [
  // Aro 3D: 16 segmentos alrededor de la cabeza con el dibujo del anillo
  // (y su brillo animado) en un plano fino por dentro.
  head('lexHalo', [
    cube([-6, 35.9, -6], [12, 0, 12], { up: haloUv, down: haloUv }),
    ...Array.from({ length: 16 }, (_, i) => cube([-1.05, 35.4, -5.6], [2.1, 1, 1], all([HALO[0] + 0.7, HALO[1] + 5.6], [0.6, 0.8]), { pivot: [0, 35.9, 0], rotation: [0, i * 22.5, 0] })),
  ], { pivot: [0, 35.9, 0] }),
  head('lexCrown', [
    cube([-5, 31, -4.8], [10, 6, 0.4], { north: crownUv, south: crownUv }),
    cube([-5, 31, 4.4], [10, 6, 0.4], { north: crownUv, south: crownUv }),
    cube([-4.8, 31, -5], [0.4, 6, 10], { east: crownUv, west: crownUv }),
    cube([4.4, 31, -5], [0.4, 6, 10], { east: crownUv, west: crownUv }),
  ]),
  head('lexHorns', [...horn(1), ...horn(-1)]),
  head('lexHat', [
    cube([-7, 31.6, -7], [14, 0.6, 14], {
      up: { uv: HAT.slice(0, 2), uv_size: [14, 14] }, down: { uv: HAT.slice(0, 2), uv_size: [14, 14] },
      ...faces(SIDES, HAT.slice(0, 2), [14, 0.6]),
    }),
    cube([-4, 32.2, -4], [8, 8, 8], { ...faces(SIDES, [HAT[0], 14], [8, 8]), up: { uv: [HAT[0] + 8, 14], uv_size: [6, 6] } }),
  ]),
  head('lexWizard', [
    cube([-8, 31.8, -8], [16, 0.8, 16], { up: W(0, 0, 16, 16), down: W(0, 0, 16, 16), ...Object.fromEntries(SIDES.map((f) => [f, W(0, 0, 16, 0.8)])) }),
    ...wizardCone,
  ]),
  head('lexCat', [...ear(-4.8, 6), ...ear(0.8, -6)]),
  head('lexPhones', [
    cube([-5.4, 25.5, -2.6], [1.4, 5, 5], { ...dark, west: CUP }),
    cube([4, 25.5, -2.6], [1.4, 5, 5], { ...dark, east: CUP }),
    cube([-5, 30, -1], [0.8, 3, 2], dark),
    cube([4.2, 30, -1], [0.8, 3, 2], dark),
    cube([-5, 32.6, -1], [10, 0.8, 2], dark),
  ]),
  head('lexBand', [
    cube([-4.65, 27.9, -4.65], [9.3, 2.9, 9.3], { north: bandFront, south: bandCloth, east: bandCloth, west: bandCloth, up: bandEdge, down: bandEdge }),
    cube([-1, 28.3, 4.6], [2, 1.8, 1], Object.fromEntries(FACES.map((f) => [f, bandEdge]))),
  ]),
  // Las cintas cuelgan del nudo y se mueven al correr.
  { name: 'lexBandA', parent: 'lexBand', pivot: [-0.6, 29, 5.4], cubes: [cube([-2.2, 23.4, 5.2], [1.8, 5.8, 0.3], Object.fromEntries(FACES.map((f) => [f, bandTail])), { pivot: [-0.8, 29, 5.4], rotation: [0, 0, 12] })] },
  { name: 'lexBandB', parent: 'lexBand', pivot: [0.6, 29, 5.4], cubes: [cube([0.4, 24.4, 5.5], [1.8, 4.8, 0.3], Object.fromEntries(FACES.map((f) => [f, bandTail])), { pivot: [0.8, 29, 5.4], rotation: [0, 0, -14] })] },
  head('lexGoggles', [
    lens(-3.8), lens(0.4),
    cube([-0.5, 28.2, -4.9], [1, 0.9, 0.6], Object.fromEntries(FACES.map((f) => [f, G(9, 0, 4, 4)]))),
    cube([-4.6, 27.9, -4.6], [9.2, 1.4, 9.2], Object.fromEntries(SIDES.map((f) => [f, G(0, 6.2, 18, 1.6)]))),
  ]),
];

// --- alitas: asoman por los lados sin tapar la capa ---
const miniBones = [1, -1].map((side) => {
  const straight = { uv: [MINI[0], MINI[1]], uv_size: [15, 15] };
  const flipped = { uv: [MINI[0] + 15, MINI[1]], uv_size: [-15, 15] };
  return {
    name: side > 0 ? 'lexMiniL' : 'lexMiniR', parent: 'body', pivot: [side * 2, 21, 3.1],
    cubes: [cube([side > 0 ? 2 : -13, 14, 3.1], [11, 11, 0], side > 0 ? { north: straight } : { north: flipped })],
  };
});

// --- mascota propia: flota junto al hombro. Las demas usan modelos del juego
// y las arma electron/vanilla.mjs ---
const six = (f, over = {}) => ({ ...Object.fromEntries(FACES.map((k) => [k, f])), ...over });
// --- aura: suelo, muro de llamas o runas y cristales en orbita. Sin padre:
// agacharse mueve el esqueleto, pero no el suelo. ---
const A = (u, v, w, h) => ({ uv: [AURA[0] + u, AURA[1] + v], uv_size: [w, h] });
const SIDE = 11.2; // lado de un octogono de radio 13.5
const auraBones = [
  { name: 'lexAura', pivot: [0, 0.4, 0], cubes: [cube([-16, 0.4, -16], [32, 0, 32], { up: A(0, 0, 32, 32) })] },
  {
    name: 'lexAuraWall', pivot: [0, 0, 0],
    cubes: Array.from({ length: 8 }, (_, i) => cube([-SIDE / 2, 0.5, -13.5], [SIDE, 12, 0], { north: A((i % 4) * 8, 32, 8, 12) }, { pivot: [0, 0, 0], rotation: [0, i * 45, 0] })),
  },
  {
    name: 'lexAuraOrbs', pivot: [0, 0, 0],
    cubes: Array.from({ length: 6 }, (_, i) => [
      cube([-0.9, 9 + (i % 3) * 3.4, -15.4], [1.8, 2.6, 1.8], six(A(0, 44, 4, 4)), { pivot: [0, 0, 0], rotation: [0, i * 60, 0] }),
      cube([-0.5, 11.6 + (i % 3) * 3.4, -15], [1, 1.2, 1], six(A(0, 44, 4, 4)), { pivot: [0, 0, 0], rotation: [0, i * 60, 0] }),
      cube([-0.5, 7.8 + (i % 3) * 3.4, -15], [1, 1.2, 1], six(A(0, 44, 4, 4)), { pivot: [0, 0, 0], rotation: [0, i * 60, 0] }),
    ]).flat(),
  },
];

// Alas a la altura de los hombros (antes quedaban bajas, en la cintura).
const lift = (dy) => (b) => ({
  ...b,
  pivot: [b.pivot[0], b.pivot[1] + dy, b.pivot[2]],
  cubes: b.cubes.map((c) => ({ ...c, origin: [c.origin[0], c.origin[1] + dy, c.origin[2]], ...(c.pivot ? { pivot: [c.pivot[0], c.pivot[1] + dy, c.pivot[2]] } : {}) })),
});
const WING_LIFT = 4;
const MINI_LIFT = 3;

export const BONES = [
  ...capeBones,
  ...[...wingBones(1), ...wingBones(-1), ...dragonBones(1), ...dragonBones(-1)].map(lift(WING_LIFT)),
  ...miniBones.map(lift(MINI_LIFT)),
  ...headBones, ...auraBones,
];

// Cosmeticos por archivos (launcher/cosmetics/*): el launcher los lee al
// arrancar y los registra aqui, junto a los del catalogo fijo.
export const FILE_DATA = { animations: {}, controllers: {}, entity: { animations: {}, animate: [], particles: {} }, particles: [] };
export function registerFiles(compiled) {
  for (const item of compiled.items) if (!COSMETICS.some((c) => c.id === item.id)) COSMETICS.push(item);
  for (const bone of compiled.bones) if (!BONES.some((b) => b.name === bone.name)) BONES.push(bone);
  Object.assign(FILE_DATA, { animations: compiled.animations, controllers: compiled.controllers, entity: compiled.entity, particles: compiled.particles });
}

export const find = (id) => COSMETICS.find((c) => c.id === id) ?? null;

// Huesos que pertenecen a cada cosmetico (para las vistas previas).
export const bonesOf = (id, extra = []) => {
  const c = find(id);
  if (!c || c.geo === 'trail') return [];
  if (c.file) {
    const root = `lexF${c.file.id}`;
    return [...BONES, ...extra].filter((b) => b.name === root || b.name.startsWith(`${root}_`));
  }
  if (c.geo === 'dfx') {
    const all = c.slot === 'hit' ? dfxBones('lexDfx', c.slot) : killBones('lexDfx', [c.kind]);
    const root = `lexDfx${c.kind}`;
    return all.filter((b) => b.name === root || b.name.startsWith(`${root}_`)).map((b) => (b.name === root ? { ...b, parent: undefined } : b));
  }
  const prefix = { cape: 'lexCape', wing: 'lexWing', dragon: 'lexDragon', mini: 'lexMini' }[c.geo] ?? (c.slot === 'cape' ? 'lexCape' : `lex${c.geo}`);
  return [...BONES, ...extra].filter((b) => {
    const name = b.name.toLowerCase();
    const p = prefix.toLowerCase();
    if (c.slot === 'cape' || c.slot === 'wings' || c.slot === 'pet' || c.slot === 'aura' || c.geo === 'band') return name.startsWith(p);
    return name === p;
  });
};

// Cosmeticos que cambiaron de nombre: se pasa al equivalente nuevo.
const RENAMED = {
  'kill-skull': 'kill-execution', 'kill-bolt': 'kill-wrath', 'kill-firework': 'kill-rocket', 'kill-crown': 'kill-blackhole',
  'death-soul': 'death-ascension', 'death-angel': 'death-ascension', 'death-grave': 'death-bleed', 'death-coffin': 'death-bleed',
  'pet-wyvern': 'pet-dragon', 'pet-wyvern-red': 'pet-dragon', 'pet-wyvern-black': 'pet-dragon-black', 'pet-wyvern-gold': 'pet-dragon', 'pet-wyvern-blue': 'pet-dragon',
  'pet-dragonfire': 'pet-dragon', 'pet-dragonfire-black': 'pet-dragon-black', 'pet-dragonfire-bronze': 'pet-dragon', 'pet-dragonice': 'pet-dragon',
  'pet-dragonice-white': 'pet-dragon', 'pet-dragonice-sapphire': 'pet-dragon', 'pet-dragonstorm': 'pet-dragon-black', 'pet-dragonstorm-copper': 'pet-dragon', 'pet-dragonstorm-electric': 'pet-dragon-black',
  'pet-cat-black': 'pet-cat', 'pet-cat-white': 'pet-cat', 'pet-cat-gray': 'pet-cat-orange', 'pet-ghost-blue': 'pet-ghost', 'pet-ghost-purple': 'pet-ghost', 'pet-ghost-mint': 'pet-ghost',
};

export function cleanLoadout(value) {
  const out = {};
  for (const { id } of SLOTS) {
    const found = find(value?.[id]) ?? find(RENAMED[value?.[id]]);
    out[id] = found?.slot === id ? found.id : null;
  }
  return out;
}
