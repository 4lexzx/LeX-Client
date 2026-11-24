// LEX CLIENT — Genera texturas animadas propias para cosméticos.
// Salida: PNG (atlas para preview en el launcher) + LEXTEX1 (frames BGRA premultiplicados para el overlay).
// No se copian assets de otros clientes ni se tocan archivos del juego.
import { mkdirSync, writeFileSync } from 'node:fs';
import { deflateSync } from 'node:zlib';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const outDir = resolve(root, 'cosmetics/assets');
mkdirSync(outDir, { recursive: true });

const FRAMES = 8;

function newPixels(w, h) {
  return new Uint8ClampedArray(w * h * 4);
}

function blend(buf, w, h, x, y, r, g, b, a) {
  x = Math.round(x); y = Math.round(y);
  if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return;
  const i = (y * w + x) * 4;
  const sa = Math.min(1, Math.max(0, a / 255));
  const da = buf[i + 3] / 255;
  const oa = sa + da * (1 - sa);
  if (oa <= 0) return;
  buf[i] = (r * sa + buf[i] * da * (1 - sa)) / oa;
  buf[i + 1] = (g * sa + buf[i + 1] * da * (1 - sa)) / oa;
  buf[i + 2] = (b * sa + buf[i + 2] * da * (1 - sa)) / oa;
  buf[i + 3] = oa * 255;
}

function fillPoly(buf, w, h, points, colorAt) {
  let minY = Infinity, maxY = -Infinity;
  for (const [, y] of points) { minY = Math.min(minY, y); maxY = Math.max(maxY, y); }
  for (let y = Math.max(0, Math.floor(minY)); y <= Math.min(h - 1, Math.ceil(maxY)); y++) {
    const yy = y + 0.5;
    const xs = [];
    for (let i = 0; i < points.length; i++) {
      const [x1, y1] = points[i];
      const [x2, y2] = points[(i + 1) % points.length];
      if (y1 === y2) continue;
      if (yy < Math.min(y1, y2) || yy >= Math.max(y1, y2)) continue;
      xs.push(x1 + ((yy - y1) * (x2 - x1)) / (y2 - y1));
    }
    xs.sort((a, b) => a - b);
    for (let i = 0; i + 1 < xs.length; i += 2) {
      for (let x = Math.max(0, Math.ceil(xs[i])); x <= Math.min(w - 1, Math.floor(xs[i + 1])); x++) {
        const [r, g, b, a] = colorAt(x, y);
        blend(buf, w, h, x, y, r, g, b, a);
      }
    }
  }
}

function line(buf, w, h, x1, y1, x2, y2, width, color) {
  const minX = Math.max(0, Math.floor(Math.min(x1, x2) - width));
  const maxX = Math.min(w - 1, Math.ceil(Math.max(x1, x2) + width));
  const minY = Math.max(0, Math.floor(Math.min(y1, y2) - width));
  const maxY = Math.min(h - 1, Math.ceil(Math.max(y1, y2) + width));
  const dx = x2 - x1, dy = y2 - y1;
  const len2 = dx * dx + dy * dy || 1;
  for (let y = minY; y <= maxY; y++) {
    for (let x = minX; x <= maxX; x++) {
      const t = Math.max(0, Math.min(1, ((x - x1) * dx + (y - y1) * dy) / len2));
      const d = Math.hypot(x - (x1 + t * dx), y - (y1 + t * dy));
      if (d <= width) {
        const a = color[3] * (1 - d / (width + 0.5));
        blend(buf, w, h, x, y, color[0], color[1], color[2], a);
      }
    }
  }
}

function makeWings(frame) {
  const w = 160, h = 96;
  const buf = newPixels(w, h);
  const t = (frame / FRAMES) * Math.PI * 2;
  const flap = Math.sin(t);
  for (const side of [-1, 1]) {
    const px = (x) => (side === 1 ? x : w - x);
    const pts = [
      [px(80), 26], [px(103), 10 + flap * 9], [px(140), 14 + flap * 18],
      [px(157), 35 + flap * 14], [px(145), 54 + flap * 10], [px(118), 78 + flap * 5],
      [px(94), 52], [px(80), 34],
    ];
    fillPoly(buf, w, h, pts, (x, y) => {
      const outward = Math.min(1, Math.abs(x - 80) / 80);
      const low = Math.min(1, Math.max(0, (y - 20) / 75));
      return [58 + 62 * outward + 40 * low, 36 + 110 * outward + 40 * low,
        128 + 60 * outward - 50 * low, 224];
    });
    line(buf, w, h, px(80), 27, px(103), 10 + flap * 9, 2, [255, 214, 128, 210]);
    line(buf, w, h, px(103), 10 + flap * 9, px(140), 14 + flap * 18, 2, [0, 200, 232, 220]);
    line(buf, w, h, px(80), 30, px(118), 78 + flap * 5, 2, [255, 214, 128, 190]);
    line(buf, w, h, px(80), 32, px(145), 54 + flap * 10, 2, [0, 200, 232, 190]);
  }
  return { w, h, buf };
}

function makeCape(frame) {
  const w = 96, h = 160;
  const buf = newPixels(w, h);
  const t = (frame / FRAMES) * Math.PI * 2;
  const wave = Math.sin(t);
  const pts = [
    [16, 12], [80, 12], [86, 54 + wave * 4], [90, 104 + wave * 8],
    [80 + wave * 7, 150], [48 + wave * 3, 157], [16 - wave * 7, 150],
    [6, 104 - wave * 8], [10, 54 - wave * 4],
  ];
  fillPoly(buf, w, h, pts, (x, y) => {
    const low = Math.min(1, Math.max(0, (y - 12) / 148));
    return [64 + 128 * low, 28 + 84 * low, 116 - 58 * low, 238];
  });
  line(buf, w, h, 16, 12, 80, 12, 3, [255, 206, 122, 235]);
  line(buf, w, h, 80 + wave * 7, 150, 48 + wave * 3, 157, 3, [255, 206, 122, 220]);
  line(buf, w, h, 48 + wave * 3, 157, 16 - wave * 7, 150, 3, [255, 206, 122, 220]);
  for (const x of [28, 48, 68]) {
    line(buf, w, h, x, 20, x + wave * 3, 140, 1, [0, 200, 232, 90]);
  }
  return { w, h, buf };
}

function crc32(buf) {
  let crc = 0xffffffff;
  for (const byte of buf) {
    crc ^= byte;
    for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}

function writePng(path, atlas) {
  const { w, h, buf } = atlas;
  const raw = Buffer.alloc(h * (1 + w * 4));
  for (let y = 0; y < h; y++) {
    const row = y * (1 + w * 4);
    raw[row] = 0; // filter None
    for (let x = 0; x < w; x++) {
      const src = (y * w + x) * 4;
      const dst = row + 1 + x * 4;
      raw[dst] = buf[src]; raw[dst + 1] = buf[src + 1];
      raw[dst + 2] = buf[src + 2]; raw[dst + 3] = buf[src + 3];
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  writeFileSync(path, Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk('IHDR', ihdr),
    pngChunk('IDAT', deflateSync(raw, { level: 9 })),
    pngChunk('IEND', Buffer.alloc(0)),
  ]));
}

function writeLextex(path, frames, frameMs) {
  const { w, h } = frames[0];
  const header = Buffer.alloc(24);
  header.write('LEXTEX1\0', 0, 'ascii');
  header.writeInt32LE(w, 8);
  header.writeInt32LE(h, 12);
  header.writeInt32LE(frames.length, 16);
  header.writeInt32LE(frameMs, 20);
  const pixels = Buffer.alloc(w * h * frames.length * 4);
  frames.forEach((frame, fi) => {
    for (let i = 0; i < frame.buf.length; i += 4) {
      const a = frame.buf[i + 3] / 255;
      const dst = fi * w * h * 4 + i;
      pixels[dst] = Math.round(frame.buf[i + 2] * a);     // B premultiplicado
      pixels[dst + 1] = Math.round(frame.buf[i + 1] * a); // G
      pixels[dst + 2] = Math.round(frame.buf[i] * a);     // R
      pixels[dst + 3] = frame.buf[i + 3];                 // A
    }
  });
  writeFileSync(path, Buffer.concat([header, pixels]));
}

function emit(name, maker, frameMs) {
  const frames = Array.from({ length: FRAMES }, (_, f) => maker(f));
  const { w, h } = frames[0];
  const atlas = { w: w * FRAMES, h, buf: newPixels(w * FRAMES, h) };
  frames.forEach((frame, fi) => {
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const src = (y * w + x) * 4;
        const dst = (y * atlas.w + fi * w + x) * 4;
        for (let c = 0; c < 4; c++) atlas.buf[dst + c] = frame.buf[src + c];
      }
    }
  });
  writePng(resolve(outDir, `${name}.png`), atlas);
  writeLextex(resolve(outDir, `${name}.lextex`), frames, frameMs);
  console.log(`[LEX] textura ${name}: ${atlas.w}x${atlas.h} PNG, ${FRAMES} frames LEXTEX1`);
}

emit('dragon-wings', makeWings, 110);
emit('ember-cape', makeCape, 140);
