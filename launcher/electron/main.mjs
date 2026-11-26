import { app, BrowserWindow, ipcMain, shell } from 'electron';
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { detect, inject, isRunning, launch } from './game.mjs';
import { PRESETS, applyPreset, currentOptions, restore } from './optimizer.mjs';
import { PACK_NAME, buildPack, readParticleIds, findSkin, readPlayerEntity, setGlobalPack, writeClientAssets, writeItemIcons, writePack } from './pack.mjs';
import { vanillaPets } from './vanilla.mjs';
import { cleanLoadout, find, registerFiles } from '../shared/cosmetics.mjs';
import { loadFileCosmetics } from './filecosmetics.mjs';

app.setAppUserModelId('com.lexclient.app');

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
// Cosmeticos por archivos: una carpeta rota no debe tumbar el launcher.
let fileAssets = null;
try {
  fileAssets = loadFileCosmetics(root);
  registerFiles(fileAssets.compiled);
} catch (error) {
  console.error('Cosmeticos por archivos:', error.message);
}
const nativeDir = app.isPackaged ? join(process.resourcesPath, 'native') : join(root, 'native');
const appData = (!app.isPackaged && process.env.LEX_APPDATA) || app.getPath('appData');
const dataDir = join(appData, 'lex-client');
const configPath = join(dataDir, 'client.json');
const loadoutPath = join(dataDir, 'loadout.json');
const statePath = join(dataDir, 'launcher.json');
const usersDir = join(appData, 'Minecraft Bedrock', 'Users');
const packRoot = join(usersDir, 'Shared', 'games', 'com.mojang', 'development_resource_packs', PACK_NAME);

const readJson = (path, fallback) => {
  try {
    return JSON.parse(readFileSync(path, 'utf8'));
  } catch {
    return fallback;
  }
};

// Escritura atomica: el cliente lee estos archivos mientras se juega.
function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(`${path}.tmp`, JSON.stringify(value, null, 2));
  renameSync(`${path}.tmp`, path);
}

function moduleList() {
  const catalog = readJson(join(root, 'shared', 'modules.json'), []);
  const saved = readJson(configPath, {}).modules ?? {};
  return catalog.map((m) => ({ ...m, enabled: saved[m.name]?.enabled ?? m.enabled }));
}

function setModule(name, enabled) {
  if (!moduleList().some((m) => m.name === name)) throw new Error('Módulo desconocido');
  // Si el JSON existe pero no se puede leer, no se pisa la configuracion.
  if (existsSync(configPath) && !readJson(configPath, null)) throw new Error('client.json está dañado');
  const config = readJson(configPath, { version: 1, profile: 'client' });
  config.modules ??= {};
  config.modules[name] = { ...config.modules[name], enabled: Boolean(enabled) };
  writeJson(configPath, config);
  return moduleList();
}

const state = () => ({ preset: null, pending: null, ...readJson(statePath, {}) });
const saveState = (patch) => writeJson(statePath, { ...state(), ...patch });

// Detectar el juego tarda (PowerShell): se responde con lo ultimo conocido y se
// refresca por detras.
let known = null;
const snapshot = () => ({
  ...(known ?? state().game ?? { installed: false, version: null, dir: null, running: false, pending: true }),
  native: existsSync(join(nativeDir, 'LeXClient.dll')) && existsSync(join(nativeDir, 'lex-injector.exe')),
  ready: existsSync(join(packRoot, 'manifest.json')),
  client: app.getVersion(),
  selfTest: Boolean(state().selfTest),
});
// Modelos del juego para las mascotas: se leen una vez, sin parar la ventana.
let vanillaReady = null;
const vanilla = () => {
  vanillaReady ??= (async () => {
    const dir = state().game?.dir ?? (await detect()).dir;
    await new Promise((r) => setTimeout(r, 30));
    try {
      return dir ? vanillaPets(dir) : { bones: [], images: new Map() };
    } catch {
      return { bones: [], images: new Map() };
    }
  })();
  return vanillaReady;
};

async function status() {
  const [game, running] = await Promise.all([detect(), isRunning()]);
  known = { ...game, running };
  if (JSON.stringify(state().game) !== JSON.stringify({ ...game, running: false })) saveState({ game: { ...game, running: false } });
  return snapshot();
}



// Prepara todo lo que el juego necesita. Solo con Minecraft cerrado: al salir
// reescribe sus propios ajustes.
export function prepare(gameDir) {
  const { pending } = state();
  if (pending === 'restore') {
    restore(usersDir);
    saveState({ preset: null, pending: null });
  } else if (pending) {
    applyPreset(usersDir, pending);
    saveState({ preset: pending, pending: null });
  }
  const preset = PRESETS[state().preset];
  const previous = readJson(join(packRoot, 'manifest.json'), null)?.header?.version ?? [1, 0, 0];
  const version = [previous[0], previous[1], previous[2] + 1];
  const pets = vanillaPets(gameDir);
  // Equipamiento al dia (nombres antiguos -> nuevos) para el pack y para la DLL.
  const loadout = cleanLoadout(readJson(loadoutPath, {}));
  writeJson(loadoutPath, loadout);
  writePack(packRoot, buildPack(readPlayerEntity(gameDir), {
    version,
    icon: readFileSync(join(root, 'renderer', 'logo-mark.png')),
    disabledParticles: Boolean(preset?.particles),
    particleIds: preset?.particles ? readParticleIds(gameDir) : [],
    crosshair: readFileSync(join(root, 'assets', 'cosmetics', 'crosshair.png')),
    extraBones: pets.bones,
    // Animaciones de baja y muerte: la elegida se fija en el pack.
    loadout,
    fxTextures: Object.fromEntries(existsSync(join(root, 'assets', 'fx')) ? readdirSync(join(root, 'assets', 'fx')).filter((n) => n.endsWith('.png')).map((n) => [n.slice(0, -4), readFileSync(join(root, 'assets', 'fx', n))]) : []),
    // Modo prueba: vida, golpes y kills tambien sobre ti, para verlos sin otro jugador.
    healthOnSelf: process.argv.includes('--health-on-self') || Boolean(state().selfTest),
  }));
  setGlobalPack(usersDir, version);
  writeClientAssets(root, dataDir, pets, fileAssets);
  writeItemIcons(gameDir, usersDir, dataDir);
  const skin = findSkin(usersDir);
  if (skin) copyFileSync(skin, join(dataDir, 'skin.png'));
}

let playing = null;
async function play() {
  const game = await detect();
  if (!game.installed) return { ok: false, message: 'Instala Minecraft for Windows desde Microsoft Store.' };
  if (!(await isRunning())) {
    let note = null;
    try {
      if (!game.dir) throw new Error('no se encontró la carpeta del juego');
      prepare(game.dir);
    } catch (error) {
      note = `Los cosméticos no se pudieron preparar: ${error.message}`;
    }
    launch();
    const result = await inject(nativeDir);
    return note && result.ok ? { ok: false, message: `LeX está activo. ${note}` } : result;
  }
  return inject(nativeDir);
}

async function optimize(id) {
  if (id !== 'restore' && !PRESETS[id]) throw new Error('Preajuste desconocido');
  saveState({ pending: id });
  if (await isRunning()) return { ok: true, message: 'Listo. Se aplicará solo la próxima vez que pulses Jugar.' };
  if (id === 'restore') {
    restore(usersDir);
    saveState({ preset: null, pending: null });
    return { ok: true, message: 'Tus gráficos originales están de vuelta.' };
  }
  applyPreset(usersDir, id);
  saveState({ preset: id, pending: null });
  return { ok: true, message: `${PRESETS[id].label} aplicado. Pulsa Jugar.` };
}

const handlers = {
  status,
  play: () => {
    playing ??= play().finally(() => { playing = null; });
    return playing;
  },
  modules: moduleList,
  'modules:set': setModule,
  loadout: () => cleanLoadout(readJson(loadoutPath, {})),
  'loadout:save': (value) => {
    const clean = cleanLoadout(value);
    writeJson(loadoutPath, clean);
    return clean;
  },
  snapshot,
  files: () => (fileAssets ? { items: fileAssets.compiled.items, bones: fileAssets.compiled.bones, animations: {}, controllers: {}, entity: { animations: {}, animate: [], particles: {} }, particles: [] } : null),
  'selftest:set': (on) => {
    saveState({ selfTest: Boolean(on) });
    return snapshot();
  },
  models: async () => (await vanilla()).bones,
  available: async () => [...(await vanilla()).images.keys()],
  texture: async (id) => {
    const item = find(id);
    if (!item) throw new Error('Cosmético desconocido');
    const game = item.file && fileAssets ? fileAssets.texture(item) : item.source ? (await vanilla()).images.get(id) : null;
    const body = game ?? await readFile(join(root, 'assets', 'cosmetics', `${item.geo === 'trail' ? `trail-${item.art}` : id}.png`));
    return `data:image/png;base64,${body.toString('base64')}`;
  },
  icon: async (name) => {
    if (!/^[a-z0-9-]+$/.test(name)) return null;
    try {
      return `data:image/png;base64,${(await readFile(join(root, 'assets', 'icons', `${name}.png`))).toString('base64')}`;
    } catch {
      return null;
    }
  },
  skin: async () => {
    const saved = join(dataDir, 'skin.png');
    const path = existsSync(saved) ? saved : findSkin(usersDir);
    return path ? `data:image/png;base64,${(await readFile(path)).toString('base64')}` : null;
  },
  'skin:fresh': async () => {
    const path = findSkin(usersDir);
    if (!path) return null;
    mkdirSync(dataDir, { recursive: true });
    copyFileSync(path, join(dataDir, 'skin.png'));
    return `data:image/png;base64,${(await readFile(path)).toString('base64')}`;
  },
  optimizer: () => ({
    presets: Object.entries(PRESETS).map(([id, p]) => ({ id, label: p.label, description: p.description })),
    ...state(),
    current: currentOptions(usersDir),
    ingame: readJson(configPath, {}).modules?.Optimizador?.enabled ?? false,
  }),
  'optimizer:apply': optimize,
  uninstall: async () => {
    if (await isRunning()) return { ok: false, message: 'Cierra Minecraft primero.' };
    setGlobalPack(usersDir, null);
    rmSync(packRoot, { recursive: true, force: true });
    restore(usersDir);
    saveState({ preset: null, pending: null });
    return { ok: true, message: 'Minecraft quedó como estaba antes de LeX.' };
  },
  folder: () => {
    mkdirSync(dataDir, { recursive: true });
    return shell.openPath(dataDir);
  },
};

function createWindow() {
  const win = new BrowserWindow({
    width: 1180,
    height: 740,
    minWidth: 960,
    minHeight: 620,
    backgroundColor: '#0a0f1a',
    title: 'LeX Client',
    autoHideMenuBar: true,
    icon: join(root, 'renderer', 'icon.ico'),
    show: false,
    webPreferences: { preload: join(here, 'preload.cjs') },
  });
  win.once('ready-to-show', () => win.show());
  win.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  win.webContents.on('will-navigate', (event) => event.preventDefault());
  win.loadFile(join(root, 'renderer', 'index.html'));
  return win;
}

// Capturas de verificacion: LEX_SHOTS=<carpeta> npm start
async function shots(win, folder) {
  win.webContents.on('console-message', (e) => console.log('[renderer]', e.message));
  await new Promise((r) => win.webContents.once('did-finish-load', r));
  await new Promise((r) => setTimeout(r, 4000));
  for (const page of ['play', 'modules', 'cosmetics', 'optimizer', 'settings']) {
    await win.webContents.executeJavaScript(`document.querySelector('[data-page="${page}"]').click()`);
    await new Promise((r) => setTimeout(r, 2500));
    writeFileSync(join(folder, `launcher_${page}.png`), (await win.webContents.capturePage()).toPNG());
    if (page === 'cosmetics') {
      // Tambien cada ranura (mascotas, golpes...) para revisar sus vistas previas.
      for (const tab of (process.env.LEX_SHOT_TABS ?? '').split(',').filter(Boolean)) {
        await win.webContents.executeJavaScript(`[...document.querySelectorAll('button')].find((b) => b.textContent.trim() === ${JSON.stringify(tab)})?.click()`);
        await win.webContents.executeJavaScript(`document.querySelectorAll('*').forEach((e) => { e.scrollTop = 0; })`);
        await new Promise((r) => setTimeout(r, 3000));
        writeFileSync(join(folder, `launcher_${tab}.png`), (await win.webContents.capturePage()).toPNG());
        await win.webContents.executeJavaScript(`(() => { const s = [...document.querySelectorAll('*')].find((e) => e.scrollHeight > e.clientHeight + 40 && getComputedStyle(e).overflowY !== 'visible'); if (s) s.scrollTop = 99999; })()`);
        await new Promise((r) => setTimeout(r, 2000));
        writeFileSync(join(folder, `launcher_${tab}_2.png`), (await win.webContents.capturePage()).toPNG());
      }
    }
  }
  app.quit();
}

if (process.argv.includes('--prepare')) {
  // Prepara el juego sin abrir la ventana (accesos directos y pruebas).
  app.whenReady().then(async () => {
    const game = await detect();
    if (!game.dir || (await isRunning())) {
      console.log('no preparado');
      app.exit(1);
      return;
    }
    prepare(game.dir);
    console.log('preparado');
    app.exit(0);
  });
} else if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => BrowserWindow.getAllWindows()[0]?.focus());
  app.on('window-all-closed', () => app.quit());
  app.whenReady().then(() => {
    for (const [name, fn] of Object.entries(handlers)) ipcMain.handle(name, (_event, ...args) => fn(...args));
    const win = createWindow();
    status();
    setTimeout(vanilla, 600);
    if (process.env.LEX_SHOTS && !app.isPackaged) shots(win, process.env.LEX_SHOTS);
  });
}
