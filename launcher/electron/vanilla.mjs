// Mascotas con los modelos y texturas de la instalacion del propio usuario.
// Nada de Mojang se distribuye con LeX: se lee y se convierte en este equipo.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { inflateSync } from 'node:zlib';
import { DENSITY, PET_RECT, VANILLA_PETS } from '../shared/cosmetics.mjs';
import { encodePng, findEntry } from './pack.mjs';

// Carpetas del juego, de la mas nueva a la mas antigua.
const layers = (gameDir) => {
  const root = join(gameDir, 'data', 'resource_packs');
  return readdirSync(root).filter((n) => /^vanilla/.test(n))
    .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }))
    .map((n) => join(root, n, '__brarchive'));
};

function entry(gameDir, archive, name) {
  for (const dir of layers(gameDir)) {
    const path = join(dir, ...archive, '') + '.brarchive';
    if (!existsSync(path)) continue;
    const found = findEntry(readFileSync(path), name);
    if (found) return found;
  }
  return null;
}

// --- imagenes ---
export function decodePng(data) {
  const width = data.readUInt32BE(16);
  const height = data.readUInt32BE(20);
  if (data[24] !== 8 || (data[25] !== 6 && data[25] !== 2) || data[28] !== 0) return null;
  const step = data[25] === 6 ? 4 : 3;
  const parts = [];
  for (let at = 8; at + 8 <= data.length;) {
    const size = data.readUInt32BE(at);
    if (data.toString('latin1', at + 4, at + 8) === 'IDAT') parts.push(data.subarray(at + 8, at + 8 + size));
    at += 12 + size;
  }
  const raw = inflateSync(Buffer.concat(parts));
  const row = width * step;
  const out = new Uint8ClampedArray(width * height * 4);
  let above = Buffer.alloc(row);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (row + 1)];
    const line = Buffer.from(raw.subarray(y * (row + 1) + 1, (y + 1) * (row + 1)));
    for (let i = 0; i < row; i++) {
      const a = i >= step ? line[i - step] : 0;
      const b = above[i];
      const c = i >= step ? above[i - step] : 0;
      let add = 0;
      if (filter === 1) add = a;
      else if (filter === 2) add = b;
      else if (filter === 3) add = (a + b) >> 1;
      else if (filter === 4) {
        const p = a + b - c;
        const [pa, pb, pc] = [Math.abs(p - a), Math.abs(p - b), Math.abs(p - c)];
        add = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      line[i] = (line[i] + add) & 255;
    }
    for (let x = 0; x < width; x++) {
      const o = (y * width + x) * 4;
      out[o] = line[x * step];
      out[o + 1] = line[x * step + 1];
      out[o + 2] = line[x * step + 2];
      out[o + 3] = step === 4 ? line[x * step + 3] : 255;
    }
    above = line;
  }
  return { width, height, data: out };
}

export function decodeTga(data) {
  const type = data[2];
  const width = data.readUInt16LE(12);
  const height = data.readUInt16LE(14);
  const step = data[16] / 8;
  if ((type !== 2 && type !== 10) || (step !== 3 && step !== 4)) return null;
  const out = new Uint8ClampedArray(width * height * 4);
  let at = 18 + data[0];
  let i = 0;
  const put = (o) => {
    const y = data[17] & 0x20 ? Math.floor(i / width) : height - 1 - Math.floor(i / width);
    const to = (y * width + (i % width)) * 4;
    out[to] = data[o + 2];
    out[to + 1] = data[o + 1];
    out[to + 2] = data[o];
    out[to + 3] = step === 4 ? data[o + 3] : 255;
    i++;
  };
  while (i < width * height && at < data.length) {
    if (type === 2) { put(at); at += step; continue; }
    const head = data[at++];
    const run = (head & 127) + 1;
    if (head & 128) { for (let k = 0; k < run; k++) put(at); at += step; }
    else for (let k = 0; k < run; k++) { put(at); at += step; }
  }
  return { width, height, data: out };
}

function picture(gameDir, folder, names) {
  for (const name of names) {
    const raw = entry(gameDir, ['textures', 'entity', folder], name);
    const image = raw && (name.endsWith('.tga') ? decodeTga(raw) : decodePng(raw));
    if (image) return image;
  }
  return null;
}

// Coloca la imagen (1 texel = 1 pixel del atlas) y la tine si hace falta.
function sheet(image, rect, tint, breath = null) {
  const width = rect[2] * DENSITY;
  const height = rect[3] * DENSITY;
  if (image.width > width || image.height > height) return null;
  const data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < image.height; y++) {
    for (let x = 0; x < image.width; x++) {
      const from = (y * image.width + x) * 4;
      const to = (y * width + x) * 4;
      let [r, g, b] = image.data.subarray(from, from + 3);
      if (tint) {
        // Los ojos y zonas muy saturadas conservan su color.
        const max = Math.max(r, g, b);
        const vivid = max > 120 && max - Math.min(r, g, b) > 70;
        if (!vivid || tint.all) {
          const l = Math.min(1, ((r * 0.3 + g * 0.59 + b * 0.11) / 255) * (tint.gain ?? 1) + (tint.lift ?? 0));
          [r, g, b] = [0, 1, 2].map((k) => tint.dark[k] + (tint.light[k] - tint.dark[k]) * l);
        }
      }
      data[to] = r;
      data[to + 1] = g;
      data[to + 2] = b;
      data[to + 3] = image.data[from + 3] > 100 ? 255 : 0;
    }
  }
  if (breath) paintBreath(data, width, 22 * DENSITY, 0, 8 * DENSITY, 6 * DENSITY, breath);
  return encodePng({ width, height, data });
}

// Aliento: de la boca (izquierda, casi blanco) a la punta (derecha, se apaga),
// con lenguas de fuego que se deshilachan.
function paintBreath(data, width, x0, y0, w, h, [r, g, b]) {
  const hash = (x, y) => {
    const s = Math.sin(x * 12.9898 + y * 78.233) * 43758.5453;
    return s - Math.floor(s);
  };
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const u = x / (w - 1);
      const v = (y + 0.5) / h * 2 - 1;
      const wave = Math.sin(u * 14 + v * 3) * 0.08 + Math.sin(u * 31 - v * 5) * 0.05;
      const spread = 0.25 + u * 0.75;
      const d = Math.abs(v + wave) / spread;
      const edge = 1 - d;
      const n = hash(Math.floor(x / 3), Math.floor(y / 3));
      let a = Math.max(0, edge) * (1 - u ** 1.6) * 1.4 - n * 0.25 * u;
      a = Math.max(0, Math.min(1, a));
      const hot = Math.max(0, Math.min(1, (1 - d * 1.6) * (1 - u * 1.3)));
      const at = ((y0 + y) * width + x0 + x) * 4;
      data[at] = r + (255 - r) * hot;
      data[at + 1] = g + (255 - g) * hot;
      data[at + 2] = b + (255 - b) * hot;
      data[at + 3] = a > 0.18 ? 255 : 0;
    }
  }
}

// --- modelos ---
function model(gameDir, file, id) {
  for (const dir of layers(gameDir)) {
    const path = join(dir, 'models', 'entity.brarchive');
    const raw = existsSync(path) ? findEntry(readFileSync(path), file) : null;
    if (!raw) continue;
    const json = JSON.parse(raw.toString('utf8').replace(/^\s*\/\/.*$/gm, ''));
    const found = json['minecraft:geometry']?.find((g) => g.description.identifier === id);
    if (found) return found.bones;
    for (const [key, g] of Object.entries(json)) if (key.split(':')[0] === id && g.bones) return g.bones;
  }
  return null;
}

// Caja UV clasica -> UV por cara dentro del atlas.
function faces([w, h, d], [u, v], rect, mirror) {
  const at = (x, y, a, b) => (mirror
    ? { uv: [rect[0] + (x + a) / DENSITY, rect[1] + y / DENSITY], uv_size: [-a / DENSITY, b / DENSITY] }
    : { uv: [rect[0] + x / DENSITY, rect[1] + y / DENSITY], uv_size: [a / DENSITY, b / DENSITY] });
  const east = at(u + d + w, v + d, d, h);
  const west = at(u, v + d, d, h);
  const all = {
    north: at(u + d, v + d, w, h), south: at(u + 2 * d + w, v + d, w, h),
    east: mirror ? west : east, west: mirror ? east : west,
    up: at(u + d, v, w, d), down: at(u + d + w, v, w, d),
  };
  // Un plano lleva una sola cara: dos coincidentes parpadean.
  if (h === 0) return { up: all.up };
  if (w === 0) return { east: all.east };
  if (d === 0) return { north: all.north };
  return all;
}

function place(cube, { scale, offset, rect, flip = false, rotation, pivot }) {
  const size = cube.size.map(Number);
  const origin = cube.origin.map(Number);
  if (flip) origin[0] = -origin[0] - size[0];
  const grow = cube.inflate ?? 0;
  const out = {
    origin: origin.map((n, i) => (n - grow) * scale + offset[i]),
    size: size.map((n) => (n === 0 ? 0 : (n + grow * 2) * scale)),
    uv: faces(size, cube.uv, rect, flip !== Boolean(cube.mirror)),
  };
  if (rotation) Object.assign(out, { rotation, pivot: pivot.map((n, i) => n * scale + offset[i]) });
  return out;
}

function animal(bones, { prefix, scale, offset, rect, body, skip = [] }) {
  const keep = bones.filter((b) => !skip.includes(b.name));
  const names = new Set(keep.map((b) => b.name));
  const out = keep.map((b) => ({
    name: `${prefix}_${b.name}`,
    parent: names.has(b.parent) ? `${prefix}_${b.parent}` : prefix,
    pivot: (b.pivot ?? [0, 0, 0]).map((n, i) => n * scale + offset[i]),
    ...(b.rotation ? { rotation: b.rotation } : {}),
    cubes: (b.cubes ?? []).map((c) => place(c, {
      scale, offset, rect, ...(body?.[b.name] ? { rotation: [body[b.name], 0, 0], pivot: b.pivot } : {}),
    })),
  }));
  return [{ name: prefix, pivot: offset.slice(), cubes: [] }, ...out];
}

// El dragon del juego guarda cada pieza en su propio espacio: aqui se arma.
function dragon(bones, { prefix, scale, offset, rect }) {
  const find = (n) => bones.find((b) => b.name === n);
  const out = [];
  const at = (p) => p.map((n, i) => n * scale + offset[i]);
  const add = (name, parent, from, move, pivot, { flip = false, pick = null } = {}) => {
    const base = find(from);
    const cubes = (base.cubes ?? []).filter((_, i) => !pick || pick.includes(i))
      .map((c) => place({ ...c, origin: c.origin.map((n, i) => n + move[i]) }, { scale, offset, rect, flip }));
    out.push({ name: `${prefix}_${name}`, parent: parent ? `${prefix}_${parent}` : prefix, pivot: at(flip ? [-pivot[0], pivot[1], pivot[2]] : pivot), cubes });
  };
  out.push({ name: prefix, pivot: at([0, 8, 10]), cubes: [] });
  add('body', null, 'body', [0, 0, 0], [0, 8, 24]);
  let last = 'body';
  [[12, -13], [13, -23], [15, -32], [17, -41], [19, -50]].forEach(([y, z], i) => {
    add(`neck${i}`, last, 'neck', [0, y - 24, z], [0, y, z + 5]);
    last = `neck${i}`;
  });
  add('head', last, 'head', [0, -4, -55], [0, 20, -55]);
  add('jaw', 'head', 'jaw', [0, -4, -55], [0, 16, -63]);
  // Aliento: dos planos cruzados que salen de la boca; escala 0 en reposo.
  {
    const [mx, my, mz] = at([0, 16.5, -79]);
    const fire = { uv: [rect[0] + 22, rect[1]], uv_size: [8, 6] };
    out.push({
      name: `${prefix}_breath`, parent: `${prefix}_head`, pivot: [mx, my, mz], motion: { scale: 0 },
      cubes: [
        { origin: [mx, my - 2, mz - 9], size: [0, 4, 9], uv: { east: fire } },
        { origin: [mx, my - 2, mz - 9], size: [0, 4, 9], uv: { east: fire }, rotation: [0, 0, 90], pivot: [mx, my, mz] },
      ],
    });
  }
  last = 'body';
  for (let i = 0; i < 10; i++) {
    const y = 11 - i * 0.4;
    add(`tail${i}`, last, 'neck', [0, y - 24, 61 + i * 10], [0, y, 56 + i * 10]);
    last = `tail${i}`;
  }
  for (const [side, flip] of [['R', false], ['L', true]]) {
    add(`wing${side}`, 'body', 'wing', [0, 0, 0], [-12, 19, 2], { flip });
    add(`tip${side}`, `wing${side}`, 'wingtip', [-12, -5, 2], [-68, 19, 2], { flip });
    add(`rear${side}`, 'body', 'rearleg', [0, 0, 0], [-16, 8, 42], { flip });
    add(`reartip${side}`, `rear${side}`, 'rearlegtip', [-16, -12, 46], [-16, -18, 42], { flip });
    add(`rearfoot${side}`, `reartip${side}`, 'rearfoot', [-16, -43, 40], [-16, -50, 44], { flip });
    add(`front${side}`, 'body', 'frontleg', [0, 0, 0], [-12, 4, 2], { flip });
    add(`fronttip${side}`, `front${side}`, 'frontlegtip', [-12, -20, 3], [-12, -15, 2], { flip });
    add(`frontfoot${side}`, `fronttip${side}`, 'frontfoot', [-12, -40, 2], [-12, -39, 2], { flip });
  }
  return out;
}

const KINDS = {
  dragon: { file: 'ender_dragon.geo.json', id: 'geometry.dragon', make: dragon, scale: 0.09, offset: [-19, 33, 0] },
  // Solo el dragon: el resto de animales del juego no se ofrece.
  _cat: { file: 'cat.geo.json', id: 'geometry.cat', make: animal, scale: 0.85, offset: [-15, 0, 2], body: { body: 90 } },
  _fox: { file: 'fox.geo.json', id: 'geometry.fox', make: animal, scale: 0.75, offset: [-15, 0, 2], body: { body: 90 }, skip: ['head_sleeping', 'held_item'] },
  _parrot: { file: 'parrot.geo.json', id: 'geometry.parrot', make: animal, scale: 0.9, offset: [-15, 28, 0] },
  _bee: { file: 'bee.geo.json', id: 'geometry.bee', make: animal, scale: 0.6, offset: [-15, 27, 0] },
  _allay: { file: 'allay.geo.json', id: 'geometry.allay', make: animal, scale: 0.95, offset: [-15, 26, 0], skip: ['rightItem'] },
};

// { bones, images: Map(id -> png) }. Lo que falte en esta version se omite.
export function vanillaPets(gameDir) {
  const bones = [];
  const images = new Map();
  const ready = new Set();
  for (const [kind, spec] of Object.entries(KINDS).filter(([k]) => !k.startsWith('_'))) {
    try {
      const source = model(gameDir, spec.file, spec.id);
      if (!source) continue;
      const prefix = `lexPet${kind[0].toUpperCase()}${kind.slice(1)}`;
      bones.push(...spec.make(source, { ...spec, prefix, rect: PET_RECT[kind] }));
      ready.add(kind);
    } catch {
      // modelo distinto al esperado: esa mascota no se ofrece
    }
  }
  for (const pet of VANILLA_PETS) {
    if (!ready.has(pet.geo.slice(3))) continue;
    try {
      const image = picture(gameDir, pet.source.folder, pet.source.names);
      const png = image && sheet(image, pet.rect, pet.source.tint, pet.source.breath);
      if (png) images.set(pet.id, png);
    } catch {
      // textura ilegible
    }
  }
  return { bones, images };
}
