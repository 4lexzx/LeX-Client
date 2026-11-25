// Optimizador: cambia options.txt de Minecraft con el juego cerrado y guarda
// una copia para poder volver a los ajustes originales con un clic.
import { copyFileSync, existsSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from 'node:fs';
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
