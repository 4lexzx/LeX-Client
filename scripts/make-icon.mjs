// LEX CLIENT — Genera iconos PNG para el instalador (256x256) y la bandeja (32x32).
import { deflateSync } from 'node:zlib';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const crcTable = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c;
  }
  return t;
})();
function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const t = Buffer.from(type, 'ascii');
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([t, data])), 0);
  return Buffer.concat([len, t, data, crc]);
}
function makePng(size) {
  const raw = Buffer.alloc(size * (1 + size * 4));
  const border = Math.max(1, Math.floor(size * 0.08));
  for (let y = 0; y < size; y++) {
    const row = y * (1 + size * 4);
    raw[row] = 0;
    for (let x = 0; x < size; x++) {
      const o = row + 1 + x * 4;
      const edge = x < border || y < border || x >= size - border || y >= size - border;
      raw[o] = edge ? 10 : 0;
      raw[o + 1] = edge ? 12 : 200;
      raw[o + 2] = edge ? 16 : 232;
      raw[o + 3] = 255;
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0); ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk('IHDR', ihdr),
    chunk('IDAT', deflateSync(raw)),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const assets = resolve(root, 'apps/launcher/assets');
mkdirSync(assets, { recursive: true });
for (const [name, size] of [['icon.png', 256], ['tray.png', 32]]) {
  const dest = resolve(assets, name);
  writeFileSync(dest, makePng(size));
  console.log(`[LEX] icono generado: ${dest} (${size}x${size})`);
}
