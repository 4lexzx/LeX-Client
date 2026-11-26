// Optimizador: cambia options.txt de Minecraft con el juego cerrado y guarda
// una copia para poder volver a los ajustes originales con un clic.
import { copyFileSync, existsSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const COMMON = {
  gfx_vsync: 0,
  gfx_max_framerate: 0,
  gfx_multithreaded_renderer: 1,
  gfx_toggleclouds: 0,
  gfx_bubble_particles: 0,
  shadow_quality: 0,
  cloud_quality: 0,
  volumetric_fog_quality: 0,
  gfx_texel_aa_2: 0,
};

// null = conservar lo que tenga el usuario.
export const PRESETS = {
  balanced: {
    label: 'Equilibrado',
    description: 'Quita el límite de FPS, nubes y burbujas. Conserva hojas e iluminación.',
    particles: false,
    options: { ...COMMON, gfx_msaa: 1 },
    maxDistance: 160,
  },
  max: {
    label: 'Máximos FPS',
    description: 'Gráficos simples, sin partículas y menos distancia. Lo más rápido.',
    particles: true,
    options: { ...COMMON, graphics_mode: 0, gfx_msaa: 1, gfx_smoothlighting: 0, gfx_fancyskies: 0, gfx_transparentleaves: 0, screen_animations: 0 },
    maxDistance: 96,
  },
};

export function applyOptions(text, values, maxDistance) {
  const eol = text.includes('\r\n') ? '\r\n' : '\n';
  const changed = [];
  const lines = text.split(/\r?\n/).map((line) => {
    const at = line.indexOf(':');
    if (at < 0) return line;
    const key = line.slice(0, at);
    let next = null;
    if (Object.hasOwn(values, key)) next = String(values[key]);
    // La distancia solo se baja, nunca se sube.
    if (key === 'gfx_viewdistance' && maxDistance && Number(line.slice(at + 1)) > maxDistance) next = String(maxDistance);
    if (next === null || next === line.slice(at + 1)) return line;
    changed.push(key);
    return `${key}:${next}`;
  });
  return { text: lines.join(eol), changed };
}

function optionFiles(usersDir) {
  if (!existsSync(usersDir)) return [];
  return readdirSync(usersDir)
    .map((u) => join(usersDir, u, 'games', 'com.mojang', 'minecraftpe', 'options.txt'))
    .filter((p) => existsSync(p));
}

const backupOf = (path) => `${path}.lex-backup`;

function replace(path, body) {
  writeFileSync(`${path}.tmp`, body);
  renameSync(`${path}.tmp`, path);
}

export function applyPreset(usersDir, id) {
  const preset = PRESETS[id];
  if (!preset) throw new Error('Preajuste desconocido');
  let changed = 0;
  for (const path of optionFiles(usersDir)) {
    if (!existsSync(backupOf(path))) copyFileSync(path, backupOf(path));
    // Siempre se parte del original: cambiar de preajuste no acumula cambios.
    const original = readFileSync(backupOf(path), 'utf8');
    const result = applyOptions(original, preset.options, preset.maxDistance);
    replace(path, result.text);
    changed += result.changed.length;
  }
  return changed;
}

export function restore(usersDir) {
  let restored = 0;
  for (const path of optionFiles(usersDir)) {
    if (!existsSync(backupOf(path))) continue;
    // Solo se devuelven las claves que LeX pudo cambiar; el resto (controles,
    // volumen...) se queda como el usuario lo tenga ahora.
    const keys = new Set([...Object.keys(PRESETS.max.options), ...Object.keys(PRESETS.balanced.options), 'gfx_viewdistance']);
    const old = {};
    for (const line of readFileSync(backupOf(path), 'utf8').split(/\r?\n/)) {
      const at = line.indexOf(':');
      if (at > 0 && keys.has(line.slice(0, at))) old[line.slice(0, at)] = line.slice(at + 1);
    }
    replace(path, applyOptions(readFileSync(path, 'utf8'), old).text);
    rmSync(backupOf(path));
    restored++;
  }
  return restored;
}

// Lo que el juego tiene puesto ahora mismo (archivo mas reciente).
export function currentOptions(usersDir) {
  const files = optionFiles(usersDir).map((path) => ({ path, time: statSync(path).mtimeMs })).sort((a, b) => b.time - a.time);
  if (!files.length) return [];
  const values = Object.fromEntries(readFileSync(files[0].path, 'utf8').split(/\r?\n/).map((l) => [l.slice(0, l.indexOf(':')), l.slice(l.indexOf(':') + 1)]));
  const on = (key) => (values[key] === undefined ? null : values[key] !== '0');
  const cap = Number(values.gfx_max_framerate);
  return [
    ['Límite de FPS', on('gfx_vsync') ? 'Sincronía vertical' : cap > 0 ? `${cap} FPS` : 'Sin límite', !on('gfx_vsync')],
    ['Distancia de visión', values.gfx_viewdistance ? `${Math.round(Number(values.gfx_viewdistance) / 16)} chunks` : '-', Number(values.gfx_viewdistance) <= 160],
    ['Modo gráfico', { 0: 'Simple', 1: 'Elegante', 2: 'Vibrante', 3: 'Trazado de rayos' }[values.graphics_mode] ?? '-', values.graphics_mode === '0'],
    ['Nubes', on('gfx_toggleclouds') ? 'Activadas' : 'Apagadas', !on('gfx_toggleclouds')],
    ['Luz suave', on('gfx_smoothlighting') ? 'Activada' : 'Apagada', !on('gfx_smoothlighting')],
    ['Sombras', Number(values.shadow_quality) > 0 ? 'Activadas' : 'Apagadas', !(Number(values.shadow_quality) > 0)],
    ['Hojas', on('gfx_transparentleaves') ? 'Transparentes' : 'Opacas', !on('gfx_transparentleaves')],
    ['Render multihilo', on('gfx_multithreaded_renderer') ? 'Activado' : 'Apagado', on('gfx_multithreaded_renderer')],
  ].map(([label, value, good]) => ({ label, value, good: Boolean(good) }));
}
