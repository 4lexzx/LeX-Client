// Generador de iconos originales para LeX-Client.
// No copia assets de Lunar/Flarial: son dibujos lineales generados aqui.
// Salida: PNG 32/64 para assets/, RGBA 32 para embebir en la DLL y un preview.
// Uso: node scripts/make-icons.mjs
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const ROOT = path.resolve(import.meta.dirname, '..');
const OUT = path.join(ROOT, 'assets', 'icons');
fs.mkdirSync(OUT, { recursive: true });

const SIZE = 100;      // coordenadas logicas
const SS = 4;          // supersampling

class Surface {
  constructor(n) {
    this.n = n;
    this.cov = new Float32Array(n * n);
  }
  plot(x, y, value = 1) {
    if (x < 0 || y < 0 || x >= this.n || y >= this.n) return;
    const i = y * this.n + x;
    this.cov[i] = Math.min(1, this.cov[i] + value);
  }
  line(x0, y0, x1, y1, width = 4) {
    const steps = Math.max(1, Math.ceil(Math.hypot(x1 - x0, y1 - y0)));
    const r = width * 0.5;
    for (let s = 0; s <= steps; s++) {
      const t = s / steps;
      const cx = x0 + (x1 - x0) * t;
      const cy = y0 + (y1 - y0) * t;
      const minX = Math.floor(cx - r - 1), maxX = Math.ceil(cx + r + 1);
      const minY = Math.floor(cy - r - 1), maxY = Math.ceil(cy + r + 1);
      for (let y = minY; y <= maxY; y++) {
        for (let x = minX; x <= maxX; x++) {
          if (Math.hypot(x + 0.5 - cx, y + 0.5 - cy) <= r) this.plot(x, y, 0.9);
        }
      }
    }
  }
  polyline(points, width = 4, closed = false) {
    for (let i = 0; i < points.length - 1; i++) {
      this.line(points[i][0], points[i][1], points[i + 1][0], points[i + 1][1], width);
    }
    if (closed && points.length > 2) {
      const a = points[points.length - 1], b = points[0];
      this.line(a[0], a[1], b[0], b[1], width);
    }
  }
  circle(cx, cy, radius, width = 4) {
    const steps = Math.max(24, Math.round(radius * 4));
    const outer = radius + width * 0.5;
    const inner = Math.max(0, radius - width * 0.5);
    const minX = Math.floor(cx - outer - 1), maxX = Math.ceil(cx + outer + 1);
    const minY = Math.floor(cy - outer - 1), maxY = Math.ceil(cy + outer + 1);
    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        const d = Math.hypot(x + 0.5 - cx, y + 0.5 - cy);
        if (d <= outer && d >= inner) this.plot(x, y, 0.9);
      }
    }
    void steps;
  }
  disc(cx, cy, radius) {
    const minX = Math.floor(cx - radius - 1), maxX = Math.ceil(cx + radius + 1);
    const minY = Math.floor(cy - radius - 1), maxY = Math.ceil(cy + radius + 1);
    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        if (Math.hypot(x + 0.5 - cx, y + 0.5 - cy) <= radius) this.plot(x, y, 1);
      }
    }
  }
  rect(x0, y0, x1, y1, width = 4, radius = 6) {
    if (radius <= 0) {
      this.polyline([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], width, true);
      return;
    }
    const r = Math.min(radius, (x1 - x0) / 2, (y1 - y0) / 2);
    const steps = 8;
    const pts = [];
    const corners = [
      [x1 - r, y0 + r, -Math.PI / 2, 0],
      [x1 - r, y1 - r, 0, Math.PI / 2],
      [x0 + r, y1 - r, Math.PI / 2, Math.PI],
      [x0 + r, y0 + r, Math.PI, Math.PI * 1.5],
    ];
    for (const [cx, cy, a0, a1] of corners) {
      for (let i = 0; i <= steps; i++) {
        const a = a0 + (a1 - a0) * (i / steps);
        pts.push([cx + Math.cos(a) * r, cy + Math.sin(a) * r]);
      }
    }
    this.polyline(pts, width, true);
  }
  arc(cx, cy, radius, start, end, width = 4) {
    const steps = Math.max(12, Math.round((end - start) * radius));
    for (let i = 0; i < steps; i++) {
      const a0 = start + (end - start) * (i / steps);
      const a1 = start + (end - start) * ((i + 1) / steps);
      this.line(cx + Math.cos(a0) * radius, cy + Math.sin(a0) * radius,
                cx + Math.cos(a1) * radius, cy + Math.sin(a1) * radius, width);
    }
  }
  poly(points, fill = true, width = 4) {
    if (fill) {
      for (let y = 0; y < this.n; y++) {
        for (let x = 0; x < this.n; x++) {
          if (pointInPoly(x + 0.5, y + 0.5, points)) this.plot(x, y, 1);
        }
      }
    } else {
      this.polyline(points, width, true);
    }
  }
  downsample(target) {
    const out = new Uint8Array(target * target * 4);
    const block = this.n / target;
    for (let y = 0; y < target; y++) {
      for (let x = 0; x < target; x++) {
        let sum = 0, count = 0;
        for (let sy = Math.floor(y * block); sy < Math.floor((y + 1) * block); sy++) {
          for (let sx = Math.floor(x * block); sx < Math.floor((x + 1) * block); sx++) {
            sum += this.cov[sy * this.n + sx];
            count++;
          }
        }
        const alpha = Math.round((sum / Math.max(1, count)) * 255);
        const i = (y * target + x) * 4;
        out[i] = 255; out[i + 1] = 255; out[i + 2] = 255; out[i + 3] = alpha;
      }
    }
    return out;
  }
}

function pointInPoly(x, y, points) {
  let inside = false;
  for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
    const [xi, yi] = points[i], [xj, yj] = points[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) {
      inside = !inside;
    }
  }
  return inside;
}

const S = SIZE * SS;
const C = SIZE / 2;
const draw = (fn) => { const s = new Surface(S); fn(s); return s; };
const L = (s, ...a) => s.line(...a.map(v => v * SS));
const P = (s, pts, w = 4, closed = false) => s.polyline(pts.map(p => p.map(v => v * SS)), w * SS, closed);

const icons = {
  category_hud: draw(s => {
    L(s, 14, 78, 86, 78, 4);
    for (const [x, h] of [[24, 34], [42, 52], [60, 26], [78, 44]]) L(s, x, 78, x, 78 - h, 9);
  }),
  category_visual: draw(s => {
    s.circle(C * SS, C * SS, 30 * SS, 4 * SS);
    s.disc(C * SS, C * SS, 10 * SS);
    s.circle(C * SS, C * SS, 10 * SS, 4 * SS);
  }),
  category_utility: draw(s => {
    P(s, [[30, 72], [66, 36]], 9);
    s.arc(69 * SS, 31 * SS, 15 * SS, -Math.PI * 0.15, Math.PI * 1.15, 5 * SS);
    s.disc(28 * SS, 74 * SS, 6 * SS);
  }),
  category_cosmetics: draw(s => {
    P(s, [[34, 22], [22, 32], [30, 44], [30, 78], [70, 78], [70, 44], [78, 32], [66, 22],
          [58, 22], [50, 30], [42, 22]], 4, true);
  }),
  category_settings: draw(s => {
    s.circle(C * SS, C * SS, 18 * SS, 4 * SS);
    s.circle(C * SS, C * SS, 8 * SS, 4 * SS);
    for (let i = 0; i < 8; i++) {
      const a = i * Math.PI / 4;
      L(s, C + Math.cos(a) * 24, C + Math.sin(a) * 24,
        C + Math.cos(a) * 38, C + Math.sin(a) * 38, 5);
    }
  }),
  favorites: draw(s => {
    const pts = [];
    for (let i = 0; i < 10; i++) {
      const r = i % 2 === 0 ? 36 : 15;
      const a = -Math.PI / 2 + i * Math.PI / 5;
      pts.push([C + Math.cos(a) * r, C + Math.sin(a) * r]);
    }
    P(s, pts, 4, true);
  }),
  search: draw(s => {
    s.circle(44 * SS, 44 * SS, 22 * SS, 5 * SS);
    L(s, 60, 60, 82, 82, 6);
  }),
  gear: draw(s => {
    s.circle(C * SS, C * SS, 16 * SS, 4 * SS);
    s.circle(C * SS, C * SS, 7 * SS, 4 * SS);
    for (let i = 0; i < 8; i++) {
      const a = i * Math.PI / 4;
      L(s, C + Math.cos(a) * 22, C + Math.sin(a) * 22,
        C + Math.cos(a) * 36, C + Math.sin(a) * 36, 5);
    }
  }),
  reset: draw(s => {
    s.arc(C * SS, C * SS, 28 * SS, Math.PI * 0.35, Math.PI * 2.05, 5 * SS);
    const ax = C + Math.cos(Math.PI * 2.05) * 28;
    const ay = C + Math.sin(Math.PI * 2.05) * 28;
    P(s, [[ax - 12, ay - 2], [ax, ay + 12], [ax + 12, ay - 4]], 5);
  }),
  close: draw(s => { L(s, 28, 28, 72, 72, 6); L(s, 72, 28, 28, 72, 6); }),
  fps: draw(s => {
    L(s, 12, 84, 88, 84, 4);
    L(s, 12, 16, 12, 84, 4);
    P(s, [[18, 66], [32, 44], [46, 58], [60, 26], [74, 40], [86, 22]], 5);
    s.disc(60 * SS, 26 * SS, 4 * SS);
  }),
  cps: draw(s => {
    s.rect(30 * SS, 16 * SS, 70 * SS, 84 * SS, 4 * SS, 16 * SS);
    L(s, 50, 18, 50, 44, 3);
    s.disc(42 * SS, 34 * SS, 6 * SS);
    s.disc(58 * SS, 34 * SS, 6 * SS);
  }),
  coordinates: draw(s => {
    s.circle(C * SS, C * SS, 32 * SS, 4 * SS);
    P(s, [[C, 22], [C + 10, C], [C, C + 10], [C - 10, C]], 4, true);
    s.disc(C * SS, C * SS, 4 * SS);
    L(s, C, 10, C, 18, 3); L(s, C, 82, C, 90, 3);
  }),
  keystrokes: draw(s => {
    s.rect(12 * SS, 28 * SS, 88 * SS, 72 * SS, 4 * SS, 6 * SS);
    for (const x of [24, 40, 56, 72]) s.rect(x * SS, 38 * SS, (x + 10) * SS, 50 * SS, 3 * SS, 2 * SS);
    s.rect(34 * SS, 56 * SS, 66 * SS, 66 * SS, 3 * SS, 2 * SS);
  }),
  ping: draw(s => {
    for (let i = 0; i < 3; i++) s.arc(C * SS, 78 * SS, (12 + i * 12) * SS, Math.PI * 1.15, Math.PI * 1.85, 5 * SS);
    s.disc(C * SS, 80 * SS, 5 * SS);
  }),
  clock: draw(s => {
    s.circle(C * SS, C * SS, 32 * SS, 4 * SS);
    L(s, C, C, C, 30, 4); L(s, C, C, 70, C + 10, 4);
    s.disc(C * SS, C * SS, 4 * SS);
  }),
  armor: draw(s => {
    P(s, [[50, 14], [80, 26], [76, 56], [50, 86], [24, 56], [20, 26]], 4, true);
    L(s, 34, 44, 66, 44, 4); L(s, 50, 30, 50, 62, 4);
  }),
  speedometer: draw(s => {
    s.arc(C * SS, 70 * SS, 34 * SS, Math.PI, Math.PI * 2, 5 * SS);
    L(s, C, 70, 72, 40, 5);
    s.disc(C * SS, 70 * SS, 6 * SS);
  }),
  zoom: draw(s => {
    s.circle(44 * SS, 44 * SS, 22 * SS, 5 * SS);
    L(s, 60, 60, 82, 82, 6);
    L(s, 44, 34, 44, 54, 4); L(s, 34, 44, 54, 44, 4);
  }),
  fullbright: draw(s => {
    s.circle(C * SS, C * SS, 14 * SS, 4 * SS);
    for (let i = 0; i < 8; i++) {
      const a = i * Math.PI / 4;
      L(s, C + Math.cos(a) * 21, C + Math.sin(a) * 21,
        C + Math.cos(a) * 36, C + Math.sin(a) * 36, 5);
    }
  }),
  crosshair: draw(s => {
    L(s, 12, C, 36, C, 5); L(s, 64, C, 88, C, 5);
    L(s, C, 12, C, 36, 5); L(s, C, 64, C, 88, 5);
    s.circle(C * SS, C * SS, 4 * SS, 3 * SS);
  }),
  hurtcam: draw(s => {
    s.circle(C * SS, C * SS, 32 * SS, 4 * SS);
    P(s, [[C, 26], [C + 12, 44], [C + 2, 54], [C + 16, 72]], 4);
  }),
  nametag: draw(s => {
    s.rect(14 * SS, 30 * SS, 86 * SS, 70 * SS, 4 * SS, 8 * SS);
    L(s, 26, 44, 60, 44, 4); L(s, 26, 56, 48, 56, 4);
    P(s, [[86, 50], [94, 58], [86, 66]], 4, true);
  }),
  toggle_sprint: draw(s => {
    L(s, 16, 34, 62, 34, 6); L(s, 16, 50, 74, 50, 6); L(s, 16, 66, 62, 66, 6);
    P(s, [[62, 22], [84, 34], [62, 46]], 5, true);
    P(s, [[74, 54], [90, 66], [74, 78]], 5, true);
  }),
  screenshot: draw(s => {
    s.rect(12 * SS, 28 * SS, 88 * SS, 76 * SS, 4 * SS, 8 * SS);
    P(s, [[30, 28], [38, 18], [62, 18], [70, 28]], 4);
    s.circle(C * SS, 52 * SS, 14 * SS, 4 * SS);
  }),
  chat: draw(s => {
    P(s, [[16, 22], [84, 22], [84, 68], [46, 68], [28, 84], [28, 68], [16, 68]], 4, true);
    s.disc(34 * SS, 45 * SS, 4 * SS); s.disc(50 * SS, 45 * SS, 4 * SS); s.disc(66 * SS, 45 * SS, 4 * SS);
  }),
  fps_limit: draw(s => {
    s.arc(C * SS, 68 * SS, 32 * SS, Math.PI, Math.PI * 2, 5 * SS);
    L(s, C, 68, 70, 42, 5);
    s.rect(20 * SS, 80 * SS, 80 * SS, 90 * SS, 4 * SS, 3 * SS);
  }),
  module_test: draw(s => {
    P(s, [[50, 14], [84, 32], [84, 68], [50, 86], [16, 68], [16, 32]], 4, true);
    P(s, [[16, 32], [50, 50], [84, 32]], 4);
    L(s, 50, 50, 50, 86, 4);
  }),
};

const crcTable = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }
  return t;
})();

function crc32(buffer) {
  let c = 0xFFFFFFFF;
  for (const byte of buffer) c = crcTable[(c ^ byte) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

function chunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const typeAndData = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(typeAndData), 0);
  return Buffer.concat([length, typeAndData, crc]);
}

function writePNG(file, width, height, rgba) {
  const stride = width * 4;
  const raw = Buffer.alloc((stride + 1) * height);
  for (let y = 0; y < height; y++) {
    raw[y * (stride + 1)] = 0;
    Buffer.from(rgba.buffer, rgba.byteOffset + y * stride, stride)
      .copy(raw, y * (stride + 1) + 1);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  const png = Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(raw, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
  fs.writeFileSync(file, png);
}

const names = Object.keys(icons);
for (const name of names) {
  const surface = icons[name];
  const rgba32 = surface.downsample(32);
  const rgba64 = surface.downsample(64);
  writePNG(path.join(OUT, `${name}_32.png`), 32, 32, rgba32);
  writePNG(path.join(OUT, `${name}_64.png`), 64, 64, rgba64);
  fs.writeFileSync(path.join(OUT, `${name}_32.rgba`), Buffer.from(rgba32));
}

// Preview: rejilla de todos los iconos a 64px sobre fondo oscuro.
const columns = 6;
const cell = 80;
const rows = Math.ceil(names.length / columns);
const pw = columns * cell + 16;
const ph = rows * cell + 16;
const preview = new Uint8Array(pw * ph * 4);
for (let i = 0; i < pw * ph; i++) {
  preview[i * 4] = 16; preview[i * 4 + 1] = 18; preview[i * 4 + 2] = 24; preview[i * 4 + 3] = 255;
}
for (let i = 0; i < names.length; i++) {
  const icon = icons[names[i]].downsample(64);
  const ox = 16 + (i % columns) * cell;
  const oy = 16 + Math.floor(i / columns) * cell;
  for (let y = 0; y < 64; y++) {
    for (let x = 0; x < 64; x++) {
      const src = (y * 64 + x) * 4;
      const alpha = icon[src + 3] / 255;
      const dst = ((oy + y) * pw + ox + x) * 4;
      preview[dst] = Math.round(16 + (255 - 16) * alpha);
      preview[dst + 1] = Math.round(18 + (255 - 18) * alpha);
      preview[dst + 2] = Math.round(24 + (255 - 24) * alpha);
    }
  }
}
writePNG(path.join(OUT, 'preview.png'), pw, ph, preview);

console.log(`Generados ${names.length} iconos (32/64 PNG + 32 RGBA) en ${OUT}`);
console.log('Preview: assets/icons/preview.png');
