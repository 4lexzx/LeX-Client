// Lee las carpetas de launcher/cosmetics/ y prepara sus imagenes. La forma de
// cada archivo esta descrita en shared/filecosmetics.mjs.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { DENSITY } from '../shared/cosmetics.mjs';
import { PARTICLE_SIZE, compileAll } from '../shared/filecosmetics.mjs';
import { encodePng } from './pack.mjs';
import { decodePng } from './vanilla.mjs';

const readJson = (path) => JSON.parse(readFileSync(path, 'utf8').replace(/^﻿/, ''));

function image(path) {
  const png = decodePng(readFileSync(path));
  if (!png) throw new Error(`${path}: PNG no soportado (usa RGBA de 8 bits sin entrelazar)`);
  return png;
}

// Cambio de tono/saturacion/brillo para las variantes de color. Los colores de
// keep (ojos, dientes...) se conservan.
function tint(src, { hue = 0, saturation = 1, light = 1, keep = [] }) {
  if (!hue && saturation === 1 && light === 1) return src;
  const out = new Uint8ClampedArray(src.data);
  for (let i = 0; i < out.length; i += 4) {
    if (!out[i + 3]) continue;
    const [r, g, b] = [out[i], out[i + 1], out[i + 2]];
    if (keep.some(([kr, kg, kb]) => Math.abs(kr - r) + Math.abs(kg - g) + Math.abs(kb - b) < 40)) continue;
    const max = Math.max(r, g, b) / 255;
    const min = Math.min(r, g, b) / 255;
    let h = 0;
    const d = max - min;
    if (d) {
      if (max === r / 255) h = ((g - b) / 255 / d) % 6;
      else if (max === g / 255) h = (b - r) / 255 / d + 2;
      else h = (r - g) / 255 / d + 4;
      h *= 60;
    }
    const s = max ? d / max : 0;
    const hh = (((h + hue) % 360) + 360) % 360;
    const ss = Math.min(1, s * saturation);
    const vv = Math.min(1, max * light);
    const c = vv * ss;
    const x = c * (1 - Math.abs(((hh / 60) % 2) - 1));
    const m = vv - c;
    const [r1, g1, b1] = hh < 60 ? [c, x, 0] : hh < 120 ? [x, c, 0] : hh < 180 ? [0, c, x] : hh < 240 ? [0, x, c] : hh < 300 ? [x, 0, c] : [c, 0, x];
    out[i] = (r1 + m) * 255;
    out[i + 1] = (g1 + m) * 255;
    out[i + 2] = (b1 + m) * 255;
  }
  return { width: src.width, height: src.height, data: out };
}

// Copia una imagen dentro de un lienzo transparente del tamano dado.
function canvas(src, width, height) {
  const data = new Uint8ClampedArray(width * height * 4);
  for (let y = 0; y < Math.min(height, src.height); y++) {
    data.set(src.data.subarray(y * src.width * 4, y * src.width * 4 + Math.min(width, src.width) * 4), y * width * 4);
  }
  return { width, height, data };
}

// Flipbook del autor (fotogramas en horizontal, cualquier tamano) -> el formato
// fijo que pinta la DLL: fotogramas de 64x64 en una fila de 512 de ancho.
function particleSheet(src, frames, channel) {
  const F = PARTICLE_SIZE.frame;
  const out = { width: PARTICLE_SIZE.w, height: PARTICLE_SIZE.h + channel, data: new Uint8ClampedArray(PARTICLE_SIZE.w * (PARTICLE_SIZE.h + channel) * 4) };
  const fw = src.width / frames;
  for (let f = 0; f < frames; f++) {
    for (let y = 0; y < F; y++) {
      for (let x = 0; x < F; x++) {
        const sx = Math.min(src.width - 1, Math.floor(f * fw + (x + 0.5) * (fw / F)));
        const sy = Math.min(src.height - 1, Math.floor((y + 0.5) * (src.height / F)));
        out.data.set(src.data.subarray((sy * src.width + sx) * 4, (sy * src.width + sx) * 4 + 4), (y * out.width + f * F + x) * 4);
      }
    }
  }
  return out;
}

// --- vista previa animada ---
// El launcher y el menu L no ejecutan Molang: aqui se calcula la animacion de
// muestra (meta.preview) a 20 fotogramas por segundo y se guardan los valores
// de cada hueso. Ellos solo los reproducen, igual que se mueve en el juego.
const M = {
  sin: (d) => Math.sin((d * Math.PI) / 180), cos: (d) => Math.cos((d * Math.PI) / 180), abs: Math.abs, pow: Math.pow, sqrt: Math.sqrt,
  floor: Math.floor, ceil: Math.ceil, round: Math.round, max: Math.max, min: Math.min, mod: (a, b) => a % b,
  clamp: (v, lo, hi) => Math.min(hi, Math.max(lo, v)), lerp: (a, b, t) => a + (b - a) * t, random: (a, b) => (a + b) / 2,
};
const compiledExpr = new Map();
function molang(expr) {
  if (typeof expr === 'number') return () => expr;
  if (!compiledExpr.has(expr)) {
    const js = String(expr)
      .replace(/\b(query|q)\.(life_time|anim_time)\b/g, 't')
      .replace(/\b(query|variable|q|v)\.[a-z_]+(\([^)]*\))?/g, '0')
      .replace(/\bmath\./g, 'M.')
      .replace(/;\s*$/, '');
    // eslint-disable-next-line no-new-func
    compiledExpr.set(expr, new Function('M', 't', `return (${js});`));
  }
  const fn = compiledExpr.get(expr);
  return (t) => Number(fn(M, t)) || 0;
}
function channel(value, t, length, loop) {
  if (value == null) return null;
  if (Array.isArray(value)) return value.map((v) => molang(v)(t));
  if (typeof value !== 'object') { const n = molang(value)(t); return [n, n, n]; }
  // Keyframes: interpolacion lineal entre los dos mas cercanos.
  const keys = Object.keys(value).map(Number).sort((a, b) => a - b);
  const at = (k) => { const v = value[k.toFixed(2)] ?? value[String(k)] ?? Object.entries(value).find(([x]) => Number(x) === k)[1]; const p = Array.isArray(v) || typeof v !== 'object' ? v : (v.post ?? v.pre); return Array.isArray(p) ? p.map((x) => molang(x)(t)) : [molang(p)(t), molang(p)(t), molang(p)(t)]; };
  const time = loop && length ? t % length : Math.min(t, length || t);
  if (time <= keys[0]) return at(keys[0]);
  if (time >= keys[keys.length - 1]) return at(keys[keys.length - 1]);
  const i = keys.findIndex((k) => k > time);
  const [k0, k1] = [keys[i - 1], keys[i]];
  const [a, b] = [at(k0), at(k1)];
  const u = (time - k0) / (k1 - k0);
  return a.map((x, j) => x + (b[j] - x) * u);
}
const round1 = (n) => Math.round(n * 100) / 100;
function samplePreview(compiled, id, state, bones) {
  const names = [`animation.lexf.${id}.${state}`];
  const anims = names.map((n) => compiled.animations[n]).filter(Boolean);
  if (!anims.length) return null;
  const length = Math.min(8, anims[0].animation_length ?? 5);
  const fps = 20;
  const frames = Math.round(length * fps);
  const out = {};
  for (const b of bones) {
    const rows = [];
    let moving = false;
    for (let f = 0; f < frames; f++) {
      const t = f / fps;
      const row = [...(b.rotation ?? [0, 0, 0]), 0, 0, 0, 1, 1, 1];
      for (const a of anims) {
        const ch = a.bones?.[b.name];
        if (!ch) continue;
        const loop = a.loop === true;
        const r = channel(ch.rotation, t, a.animation_length, loop);
        const p = channel(ch.position, t, a.animation_length, loop);
        const sc = channel(ch.scale, t, a.animation_length, loop);
        if (r) for (let k = 0; k < 3; k++) row[k] += r[k];
        if (p && !(process.env.LEX_PREVIEW_NOPOS && b.name.endsWith('_root'))) for (let k = 0; k < 3; k++) row[3 + k] += p[k];
        if (sc) for (let k = 0; k < 3; k++) row[6 + k] *= sc[k];
        moving ||= Boolean(r || p || sc);
      }
      rows.push(row.map(round1));
    }
    if (moving || b.rotation) out[b.name] = rows;
  }
  return { fps, frames, bones: out };
}

export function loadFileCosmetics(root) {
  const dir = join(root, 'cosmetics');
  const sources = [];
  const textures = new Map();
  const particleImages = new Map();
  if (existsSync(dir)) {
    for (const name of readdirSync(dir).sort()) {
      const folder = join(dir, name);
      if (!existsSync(join(folder, 'cosmetic.json'))) continue;
      const meta = readJson(join(folder, 'cosmetic.json'));
      const texture = image(join(folder, meta.texture ?? 'texture.png'));
      textures.set(meta.id, { base: texture, folder });
      const particles = (meta.particles ?? []).map((p) => {
        particleImages.set(`${meta.id}/${p.name}`, image(join(folder, p.texture)));
        return { ...p, json: readJson(join(folder, p.file)) };
      });
      sources.push({
        meta,
        model: readJson(join(folder, meta.model ?? 'model.geo.json')),
        animations: existsSync(join(folder, meta.animations ?? 'animation.json')) ? readJson(join(folder, meta.animations ?? 'animation.json')) : null,
        particles,
        texture: { width: texture.width, height: texture.height },
      });
    }
  }
  const compiled = compileAll(sources);
  // Vista previa animada de cada mascota (todas sus variantes la comparten).
  for (const src of sources) {
    if (!src.meta.preview) continue;
    const prefix = `lexF${src.meta.id}`;
    const bones = compiled.bones.filter((b) => b.name === prefix || b.name.startsWith(`${prefix}_`));
    // LEX_PREVIEW_STATE (solo desarrollo): ver otra animacion en la vista previa.
    const anim = samplePreview(compiled, src.meta.id, process.env.LEX_PREVIEW_STATE ?? src.meta.preview, bones);
    if (anim) for (const item of compiled.items) if (item.file.id === src.meta.id) item.anim = anim;
  }

  // Imagen de un item: la textura (o la de su variante) en un lienzo del tamano
  // de su region del atlas, como la espera la DLL.
  const texture = (item) => {
    const t = textures.get(item.file.id);
    const base = item.file.texture ? image(join(t.folder, item.file.texture)) : t.base;
    const colored = tint(base, item.file);
    return encodePng(canvas(colored, item.rect[2] * DENSITY, item.rect[3] * DENSITY));
  };
  const particle = (p) => encodePng(particleSheet(particleImages.get(`${p.id}/${p.name}`), p.frames, p.channel));
  return { compiled, texture, particle };
}
