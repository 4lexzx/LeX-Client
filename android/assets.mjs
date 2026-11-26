// Genera los assets del APK con el mismo codigo que el launcher de PC:
//   lex/pack/...                  pack de LeX comun a todas las elecciones
//   lex/combos/<baja>__<muerte>/  solo los archivos que cambian segun la
//                                 animacion de baja y de muerte elegidas
//   lex/catalog.json              cosmeticos disponibles y su sitio en el atlas
//   lex/cosmetics/*.png           dibujo de cada cosmetico
// Uso: node android/assets.mjs <carpeta de salida>
import { mkdirSync, readFileSync, rmSync, writeFileSync, existsSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const launcher = join(here, '..', 'launcher');
const out = process.argv[2] ?? join(here, 'build', 'assets');

const { COSMETICS, ATLAS, registerFiles } = await import(`file://${join(launcher, 'shared', 'cosmetics.mjs').replace(/\\/g, '/')}`);
const { KILL_KINDS } = await import(`file://${join(launcher, 'shared', 'killfx.mjs').replace(/\\/g, '/')}`);
const { loadFileCosmetics } = await import(`file://${join(launcher, 'electron', 'filecosmetics.mjs').replace(/\\/g, '/')}`);
const { buildPack, readPlayerEntity } = await import(`file://${join(launcher, 'electron', 'pack.mjs').replace(/\\/g, '/')}`);

const fileAssets = loadFileCosmetics(launcher);
registerFiles(fileAssets.compiled);

// La entidad del jugador sale del Minecraft de PC instalado (misma version).
const gameDir = process.env.LEX_GAME_DIR ?? 'C:\\XboxGames\\Minecraft for Windows\\Content';
if (!existsSync(gameDir)) throw new Error(`No se encontro Minecraft en ${gameDir} (define LEX_GAME_DIR)`);
const entity = readPlayerEntity(gameDir);
const fx = Object.fromEntries(readdirSync(join(launcher, 'assets', 'fx')).map((n) => [n.slice(0, -4), readFileSync(join(launcher, 'assets', 'fx', n))]));
const common = {
  version: [1, 0, Math.floor(Date.now() / 60000) % 100000],
  icon: readFileSync(join(launcher, 'renderer', 'logo-mark.png')),
  crosshair: readFileSync(join(launcher, 'assets', 'cosmetics', 'crosshair.png')),
  fxTextures: fx,
};

rmSync(join(out, 'lex'), { recursive: true, force: true });
const put = (path, data) => {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, data);
};

const kills = ['none', ...KILL_KINDS.filter((k) => k.slot === 'kill').map((k) => k.key)];
const deaths = ['none', ...KILL_KINDS.filter((k) => k.slot === 'death').map((k) => k.key)];
const pack = (kill, death) => buildPack(entity, { ...common, loadout: { kill: kill === 'none' ? null : `kill-${kill}`, death: death === 'none' ? null : `death-${death}` } });

const base = pack('none', 'none');
for (const [path, data] of base) put(join(out, 'lex', 'pack', path), data);
let comboFiles = 0;
for (const kill of kills) {
  for (const death of deaths) {
    if (kill === 'none' && death === 'none') continue;
    for (const [path, data] of pack(kill, death)) {
      const same = base.has(path) && Buffer.compare(Buffer.from(base.get(path)), Buffer.from(data)) === 0;
      if (same) continue;
      put(join(out, 'lex', 'combos', `${kill}__${death}`, path), data);
      comboFiles++;
    }
  }
}

// Catalogo y dibujos (sin las mascotas hechas con texturas del juego).
const items = [];
for (const c of COSMETICS) {
  if (c.source) continue;
  let png = null;
  if (c.file) png = fileAssets.texture(c);
  else {
    const path = join(launcher, 'assets', 'cosmetics', `${c.geo === 'trail' ? `trail-${c.art}` : c.id}.png`);
    if (existsSync(path)) png = readFileSync(path);
  }
  if (!png) continue;
  put(join(out, 'lex', 'cosmetics', `${c.id}.png`), png);
  items.push({ id: c.id, slot: c.slot, name: c.name, rect: c.rect, ...(c.trail >= 0 ? { trail: c.trail } : {}), ...(c.particles?.length ? { particles: c.particles } : {}) });
}
for (const p of fileAssets.compiled.particles) put(join(out, 'lex', 'cosmetics', `${p.id}.fp${p.channel}.png`), fileAssets.particle(p));
put(join(out, 'lex', 'catalog.json'), JSON.stringify({ atlas: [ATLAS.w, ATLAS.h], items }));
console.log(`assets: pack ${base.size} archivos, ${comboFiles} de combinaciones, ${items.length} cosmeticos`);

// Interfaz: la misma del launcher de PC (app.mjs, viewer.mjs, estilos) con el
// puente a Java y los ajustes de movil de android/ui.
rmSync(join(out, 'ui'), { recursive: true, force: true });
const copyDir = (from, to, filter = () => true) => {
  for (const name of readdirSync(from).filter(filter)) put(join(to, name), readFileSync(join(from, name)));
};
copyDir(join(here, 'ui'), join(out, 'ui', 'renderer'));
for (const name of ['style.css', 'app.mjs', 'viewer.mjs', 'logo-mark.png']) put(join(out, 'ui', 'renderer', name), readFileSync(join(launcher, 'renderer', name)));
copyDir(join(launcher, 'assets', 'icons'), join(out, 'ui', 'renderer', 'icons'), (n) => n.endsWith('.png'));
copyDir(join(launcher, 'shared'), join(out, 'ui', 'shared'), (n) => n.endsWith('.mjs'));
const compiled = fileAssets.compiled;
put(join(out, 'lex', 'files.json'), JSON.stringify({ items: compiled.items, bones: compiled.bones, animations: {}, controllers: {}, entity: { animations: {}, animate: [], particles: {} }, particles: [] }));
console.log('interfaz copiada');
