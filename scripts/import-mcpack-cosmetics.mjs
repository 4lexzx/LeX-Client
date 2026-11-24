// LEX CLIENT — Importador de cosméticos animados desde un .mcpack de Bedrock.
// Convierte secuencias de PNG en LEXTEX1 (BGRA premultiplicado) para el overlay.
// Uso: node scripts/import-mcpack-cosmetics.mjs <carpeta extraída>
// El packoriginal: Animated Cosmetics v1.2.0 por ambient#2309.
// Créditos del manifest: capas de cBionicBen / Lunar Client / Badlion Client;
// armaduras de JabaPacks y Alexuz. Uso local, no redistribución pública.

import { readdirSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { deflateSync, inflateSync } from 'node:zlib';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const src = process.argv[2] || 'C:/Users/Alexander/AppData/Local/Temp/opencode/cosmetic-pack';
const assetsDir = resolve(root, 'cosmetics/assets');
mkdirSync(assetsDir, { recursive: true });

// ---------- PNG decoder (8-bit RGB/RGBA, non-interlaced) ----------
function paeth(a, b, c) {
  const p = a + b - c;
  const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

function decodePng(file) {
  const buf = readFileSync(file);
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('PNG inválido: ' + file);
  let pos = 8, w = 0, h = 0, colorType = 6, bitDepth = 8;
  let palette = null, trns = null;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.slice(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      bitDepth = data[8]; colorType = data[9];
      if (data[12] !== 0) throw new Error('PNG interlazado no soportado');
    } else if (type === 'PLTE') palette = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += 12 + len;
  }
  const channels = colorType === 6 ? 4 : colorType === 2 ? 3 :
                   colorType === 0 || colorType === 3 ? 1 : 0;
  if (!channels) throw new Error('ColorType PNG no soportado: ' + colorType);
  if (![1, 2, 4, 8].includes(bitDepth)) throw new Error('BitDepth PNG no soportado: ' + bitDepth);
  const raw = inflateSync(Buffer.concat(idat));
  const stride = Math.ceil((w * channels * bitDepth) / 8);
  const filterBpp = Math.max(1, Math.ceil((channels * bitDepth) / 8));
  const sample = (row, x, c) => {
    if (bitDepth === 8) return row[x * channels + c];
    const bitPos = (x * channels + c) * bitDepth;
    const byte = row[bitPos >> 3];
    const shift = 8 - bitDepth - (bitPos & 7);
    return (byte >> shift) & ((1 << bitDepth) - 1);
  };
  const rgba = Buffer.alloc(w * h * 4);
  let prev = Buffer.alloc(stride);
  let off = 0;
  for (let y = 0; y < h; y++) {
    const filter = raw[off++];
    const cur = Buffer.from(raw.slice(off, off + stride));
    off += stride;
    for (let i = 0; i < stride; i++) {
      const a = i >= filterBpp ? cur[i - filterBpp] : 0;
      const b = prev[i];
      const c = i >= filterBpp ? prev[i - filterBpp] : 0;
      let v = cur[i];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += Math.floor((a + b) / 2);
      else if (filter === 4) v += paeth(a, b, c);
      cur[i] = v & 0xff;
    }
    for (let x = 0; x < w; x++) {
      const d = (y * w + x) * 4;
      if (colorType === 6) {
        rgba[d] = sample(cur, x, 0); rgba[d + 1] = sample(cur, x, 1);
        rgba[d + 2] = sample(cur, x, 2); rgba[d + 3] = sample(cur, x, 3);
      } else if (colorType === 2) {
        rgba[d] = sample(cur, x, 0); rgba[d + 1] = sample(cur, x, 1);
        rgba[d + 2] = sample(cur, x, 2); rgba[d + 3] = 255;
      } else if (colorType === 3) {
        const idx = sample(cur, x, 0);
        rgba[d] = palette ? palette[idx * 3] : 0;
        rgba[d + 1] = palette ? palette[idx * 3 + 1] : 0;
        rgba[d + 2] = palette ? palette[idx * 3 + 2] : 0;
        rgba[d + 3] = trns && idx < trns.length ? trns[idx] : 255;
      } else {
        const v = sample(cur, x, 0) * 255 / ((1 << bitDepth) - 1);
        rgba[d] = rgba[d + 1] = rgba[d + 2] = v; rgba[d + 3] = 255;
      }
    }
    prev = cur;
  }
  return { w, h, rgba };
}

function resize(img, tw, th) {
  const out = Buffer.alloc(tw * th * 4);
  for (let y = 0; y < th; y++) {
    const sy = Math.min(img.h - 1, Math.floor(((y + 0.5) * img.h) / th));
    for (let x = 0; x < tw; x++) {
      const sx = Math.min(img.w - 1, Math.floor(((x + 0.5) * img.w) / tw));
      const s = (sy * img.w + sx) * 4, d = (y * tw + x) * 4;
      out[d] = img.rgba[s]; out[d + 1] = img.rgba[s + 1];
      out[d + 2] = img.rgba[s + 2]; out[d + 3] = img.rgba[s + 3];
    }
  }
  return out;
}

// ---------- PNG encoder (atlas de preview) ----------
function crc32(buf) {
  let crc = 0xffffffff;
  for (const byte of buf) {
    crc ^= byte;
    for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}
function writePng(path, w, h, rgba) {
  const raw = Buffer.alloc(h * (1 + w * 4));
  for (let y = 0; y < h; y++) {
    const row = y * (1 + w * 4);
    raw[row] = 0;
    for (let x = 0; x < w; x++) {
      const s = (y * w + x) * 4, d = row + 1 + x * 4;
      raw[d] = rgba[s]; raw[d + 1] = rgba[s + 1];
      raw[d + 2] = rgba[s + 2]; raw[d + 3] = rgba[s + 3];
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6;
  writeFileSync(path, Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0)),
  ]));
}

function writeLextex(path, frames, frameMs) {
  const { w, h, rgba } = frames[0];
  const header = Buffer.alloc(24);
  header.write('LEXTEX1\0', 0, 'ascii');
  header.writeInt32LE(w, 8); header.writeInt32LE(h, 12);
  header.writeInt32LE(frames.length, 16); header.writeInt32LE(frameMs, 20);
  const pixels = Buffer.alloc(w * h * frames.length * 4);
  frames.forEach((frame, fi) => {
    for (let i = 0; i < frame.rgba.length; i += 4) {
      const a = frame.rgba[i + 3] / 255;
      const dst = fi * w * h * 4 + i;
      pixels[dst] = Math.round(frame.rgba[i + 2] * a);
      pixels[dst + 1] = Math.round(frame.rgba[i + 1] * a);
      pixels[dst + 2] = Math.round(frame.rgba[i] * a);
      pixels[dst + 3] = frame.rgba[i + 3];
    }
  });
  writeFileSync(path, Buffer.concat([header, pixels]));
}

// ---------- Conjuntos importados ----------
const SETS = [
  { id: 'cape-creeper', name: 'Capa Creeper cargada', folder: 'cape_creeper', fps: 8, w: 96, h: 160 },
  { id: 'cape-lunar-chroma', name: 'Capa Lunar Chroma', folder: 'cape_lunar_chroma', fps: 10, w: 96, h: 160 },
  { id: 'cape-portal', name: 'Capa Portal', folder: 'cape_portal', fps: 20, w: 96, h: 160 },
  { id: 'cape-speed', name: 'Capa Speed', folder: 'cape_speed', fps: 12, w: 96, h: 160 },
  { id: 'cape-tnt', name: 'Capa TNT', folder: 'cape_tnt', fps: 8, w: 96, h: 160 },
  { id: 'cape-totem', name: 'Capa Totem', folder: 'cape_totem', fps: 15, w: 96, h: 160 },
];

const meta = [];
for (const set of SETS) {
  const dir = join(src, 'subpacks', set.folder, 'textures/entity');
  if (!readdirSync(dir)) continue;
  const files = readdirSync(dir)
    .filter((f) => /^cape\d+\.png$/i.test(f))
    .sort((a, b) => parseInt(a.match(/\d+/)[0]) - parseInt(b.match(/\d+/)[0]));
  if (!files.length) { console.warn('[LEX] sin frames:', set.id); continue; }
  const frames = files.map((f) => {
    const img = decodePng(join(dir, f));
    return { w: set.w, h: set.h, rgba: resize(img, set.w, set.h) };
  });
  const frameMs = Math.round(1000 / set.fps);
  writeLextex(resolve(assetsDir, `${set.id}.lextex`), frames, frameMs);
  const atlasW = set.w * frames.length;
  const atlas = Buffer.alloc(atlasW * set.h * 4);
  frames.forEach((frame, fi) => {
    for (let y = 0; y < set.h; y++)
      for (let x = 0; x < set.w; x++) {
        const s = (y * set.w + x) * 4;
        const d = (y * atlasW + fi * set.w + x) * 4;
        atlas[d] = frame.rgba[s]; atlas[d + 1] = frame.rgba[s + 1];
        atlas[d + 2] = frame.rgba[s + 2]; atlas[d + 3] = frame.rgba[s + 3];
      }
  });
  writePng(resolve(assetsDir, `${set.id}.png`), atlasW, set.h, atlas);
  meta.push({
    id: set.id,
    name: set.name,
    description: `${frames.length} frames importados desde Animated Cosmetics v1.2.0 (ambient). Efecto client-side sobre el juego.`,
    version: '1.0.0', type: 'import', source: 'private',
    animated: true, frames: frames.length, frameMs, layers: 1,
    preview: `${set.id}.png`, atlas: `${set.id}.lextex`,
  });
  console.log(`[LEX] ${set.id}: ${frames.length} frames @ ${set.fps}fps`);
}

// Conjuntos especiales: alas y armadura (el pack las anima con huesos, aquí
// generamos atlas reproducibles con el mismo contenido visual).
const SPECIAL = [
  { id: 'dragon-wings-pack', name: 'Alas de dragón (ambient)', file: 'textures/entity/wings.png', fps: 12, w: 160, h: 96, kind: 'wings' },
  { id: 'armor-chroma', name: 'Armadura Chroma', dir: 'textures/models/armor/diamond_1', fps: 15, w: 128, h: 64, kind: 'armor' },
  { id: 'armor-wavy', name: 'Armadura Wavy', dir: 'subpacks/wavy_armor/textures/models/armor/diamond_1', fps: 15, w: 128, h: 64, kind: 'armor' },
];

function flapWings(img, frame, frames, w, h) {
  const base = resize(img, w, h);
  const out = Buffer.alloc(w * h * 4);
  const scaleY = 0.78 + 0.22 * Math.sin((frame / frames) * Math.PI * 2);
  for (let y = 0; y < h; y++) {
    const sy = Math.max(0, Math.min(h - 1, Math.round(h / 2 + (y - h / 2) / scaleY)));
    for (let x = 0; x < w; x++) {
      const s = (sy * w + x) * 4, d = (y * w + x) * 4;
      out[d] = base[s]; out[d + 1] = base[s + 1];
      out[d + 2] = base[s + 2]; out[d + 3] = base[s + 3];
    }
  }
  return { w, h, rgba: out };
}

function emitSet(set, frames, frameMs, layers) {
  writeLextex(resolve(assetsDir, `${set.id}.lextex`), frames, frameMs);
  const atlasW = set.w * frames.length;
  const atlas = Buffer.alloc(atlasW * set.h * 4);
  frames.forEach((frame, fi) => {
    for (let y = 0; y < set.h; y++)
      for (let x = 0; x < set.w; x++) {
        const s = (y * set.w + x) * 4;
        const d = (y * atlasW + fi * set.w + x) * 4;
        atlas[d] = frame.rgba[s]; atlas[d + 1] = frame.rgba[s + 1];
        atlas[d + 2] = frame.rgba[s + 2]; atlas[d + 3] = frame.rgba[s + 3];
      }
  });
  writePng(resolve(assetsDir, `${set.id}.png`), atlasW, set.h, atlas);
  meta.push({
    id: set.id, name: set.name,
    description: `${frames.length} frames importados desde Animated Cosmetics v1.2.0 (ambient). ${set.kind === 'armor' ? 'Armadura animada' : 'Alas animadas'}. Uso client-side.`,
    version: '1.0.0', type: 'import', source: 'private',
    animated: true, frames: frames.length, frameMs, layers,
    preview: `${set.id}.png`, atlas: `${set.id}.lextex`,
  });
  console.log(`[LEX] ${set.id}: ${frames.length} frames @ ${set.fps}fps`);
}

for (const set of SPECIAL) {
  if (set.kind === 'wings') {
    const img = decodePng(join(src, set.file));
    const frames = Array.from({ length: 8 }, (_, f) => flapWings(img, f, 8, set.w, set.h));
    emitSet(set, frames, Math.round(1000 / set.fps), 2);
  } else {
    const dir = join(src, set.dir);
    if (!readdirSync(dir)) continue;
    const files = readdirSync(dir)
      .filter((f) => /^armor\d+\.png$/i.test(f))
      .sort((a, b) => parseInt(a.match(/\d+/)[0]) - parseInt(b.match(/\d+/)[0]));
    const frames = files.map((f) => {
      const img = decodePng(join(dir, f));
      return { w: set.w, h: set.h, rgba: resize(img, set.w, set.h) };
    });
    emitSet(set, frames, Math.round(1000 / set.fps), 4);
  }
}

const ts = `// Generado por scripts/import-mcpack-cosmetics.mjs. No editar a mano.
// Créditos: ambient#2309; capas cBionicBen / Lunar / Badlion; armaduras JabaPacks / Alexuz.
import type { Cosmetic } from './types.js';

export const IMPORTED_COSMETICS: Cosmetic[] = ${JSON.stringify(meta, null, 2)};
`;
writeFileSync(resolve(root, 'cosmetics/src/imported.ts'), ts);
const js = `// Generado por scripts/import-mcpack-cosmetics.mjs. No editar a mano.\nexport const IMPORTED_COSMETICS = ${JSON.stringify(meta, null, 2)};\n`;
writeFileSync(resolve(root, 'apps/launcher/modules/cosmetics.generated.mjs'), js);
console.log(`[LEX] importados ${meta.length} cosméticos`);
