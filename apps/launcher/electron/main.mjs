// LEX CLIENT — Main Process.
// - BrowserWindow del launcher
// - IPC bridge tipado (preload expone window.lex)
// - Detección de Minecraft (detection.mjs)
// - Game launch (spawn de Minecraft.Windows.exe)
// - Profiles, Diagnostics, Updates (CHANGELOG.md local)
// - Overlay state bridge (pantalla externa de Fase 3 lee userData/overlay-state.json)

import { app, BrowserWindow, ipcMain, shell, Tray, Menu, nativeImage, dialog } from 'electron';
import { readFileSync, readdirSync, mkdirSync, writeFileSync, existsSync, copyFileSync } from 'node:fs';
import { join } from 'node:path';
import os from 'node:os';
import { spawn, execSync } from 'node:child_process';
import { detectAll, pickLatest } from './detection.mjs';
import { MODULES_CATALOG, filterCatalog } from '../modules/catalog.mjs';
import { withBuiltinCosmetics } from '../modules/cosmetics.mjs';

const CURRENT_VERSION = '0.1.0';
const DEV_URL = 'http://127.0.0.1:5173';
const VERSION_PATTERN = /^##\s*\[?\s*v?(\d+\.\d+\.\d+)[^)]*\]?\s*(.*)$/m;

let detectionCache = null;
let mainWindow = null;
let tray = null;
const childByGame = new Map();

function userDataPath(name) {
  return join(app.getPath('userData'), name);
}

// Ruta compartida con el motor nativo de overlays.
function bridgeDir() {
  const dir = join(app.getPath('appData'), 'lex-client');
  mkdirSync(dir, { recursive: true });
  return dir;
}
function bridgePath(name) {
  return join(bridgeDir(), name);
}

function readJson(path, fallback = []) {
  try {
    return JSON.parse(readFileSync(path, 'utf8'));
  } catch {
    return fallback;
  }
}

function loadInstances() {
  if (!detectionCache) detectionCache = [];
  return detectionCache;
}

function readChangelog() {
  const candidates = [
    () => join(app.getAppPath(), 'CHANGELOG.md'),
    () => 'CHANGELOG.md',
  ];
  for (const fn of candidates) {
    try {
      return readFileSync(fn(), 'utf8');
    } catch {
      /* intentar siguiente */
    }
  }
  return '';
}

function versionCompare(a, b) {
  const pa = String(a).split('.').map((n) => parseInt(n, 10) || 0);
  const pb = String(b).split('.').map((n) => parseInt(n, 10) || 0);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const d = (pa[i] ?? 0) - (pb[i] ?? 0);
    if (d !== 0) return d;
  }
  return 0;
}

function detectUpdates() {
  const log = readChangelog();
  const m = log.match(VERSION_PATTERN);
  if (!m) return { version: CURRENT_VERSION, notes: '', changed: false, launcherVersion: CURRENT_VERSION };
  const newer = versionCompare(m[1], CURRENT_VERSION) > 0;
  return { version: newer ? m[1] : CURRENT_VERSION, notes: m[2].trim(), changed: newer, launcherVersion: CURRENT_VERSION };
}

function buildDiagnostics() {
  const installations = loadInstances();
  const issues = [];
  if (installations.length === 0) {
    issues.push({ id: 'mc-no-install', level: 'warning', message: 'No se encontró ninguna instalación de Minecraft for Windows.', suggest: 'Instala Minecraft desde Microsoft Store (https://www.minecraft.net/es-es/download/windows).' });
  }
  const bad = installations.filter((i) => !i.executable);
  if (bad.length > 0) {
    issues.push({ id: 'mc-no-executable', level: 'warning', message: `Encontradas ${bad.length} instalación(es) sin ruta de ejecutable resuelta.`, suggest: 'Cierra Minecraft y reintenta; el scanner de archivos recupera la ruta si es posible.' });
  }
  if (!process.env['SystemRoot']) {
    issues.push({ id: 'env-roots', level: 'warning', message: '%SystemRoot% no está definido en el entorno del launcher.', suggest: 'No afecta a LEX CLIENT: la detección no lo necesita para enumerar versiones.' });
  }
  return {
    timestamp: new Date().toISOString(),
    platform: { os: `${os.type()} ${os.release()}`, arch: process.arch, node: process.version, electron: process.versions.electron },
    versions: { launcher: CURRENT_VERSION, minecraft: installations.map((i) => `${i.displayName} ${i.version} (${i.edition})`) },
    issues,
  };
}

let overlayProc = null;
function ensureOverlayEngine() {
  // overlayProc puede referirse a un proceso que murió o fue cerrado a mano:
  // comprobar exitCode, no sólo killed, para poder relanzarlo.
  if (overlayProc && overlayProc.exitCode === null && !overlayProc.killed && !overlayProc.signalCode) return true;
  const exe = join(process.resourcesPath, 'overlay', 'lex-overlay-engine.exe');
  if (!existsSync(exe)) return false;
  overlayProc = spawn(exe, [], { detached: true, stdio: 'ignore' });
  overlayProc.unref();
  return true;
}

// Si el motor muere mientras Minecraft sigue abierto, F8 deja de responder.
// Vigilar el proceso y relanzarlo mantiene el overlay siempre disponible.
function gameIsRunning() {
  try {
    const out = execSync('tasklist /FI "IMAGENAME eq Minecraft.Windows" /NH', { encoding: 'utf8' });
    return /Minecraft\.Windows/i.test(out);
  } catch {
    return false;
  }
}
const overlayWatchdog = setInterval(() => {
  if (gameIsRunning()) ensureOverlayEngine();
}, 2500);
overlayWatchdog.unref();

function launchGame(installationId) {
  const inst = loadInstances().find((i) => i.id === installationId);
  if (!inst) return { ok: false, error: 'mc-not-found', displayName: null };
  // El motor espera a que exista la ventana de Bedrock y sólo entonces inyecta
  // el puente; inyectar durante el arranque DXGI crashea el juego.
  ensureOverlayEngine();
  if (inst.packageFamilyName) {
    const appsFolder = `shell:AppsFolder\\${inst.packageFamilyName}!Game`;
    const child = spawn('explorer.exe', [appsFolder], { detached: true, stdio: 'ignore' });
    child.unref();
    childByGame.set(inst.id, child);
    ensureOverlayEngine();
    // El launcher permanece visible: ocultarlo dejaba la UI "intocable" tras JUGAR.
    return { ok: true, pid: child.pid, displayName: inst.displayName, version: inst.version, mode: 'appsfolder', overlay: overlayProc?.pid ?? null };
  }
  if (inst.executable) {
    const child = spawn(inst.executable, [], { detached: true, stdio: 'ignore', cwd: inst.installLocation || undefined });
    child.unref();
    childByGame.set(inst.id, child);
    ensureOverlayEngine();
    // El launcher permanece visible: ocultarlo dejaba la UI "intocable" tras JUGAR.
    return { ok: true, pid: child.pid, displayName: inst.displayName, version: inst.version, mode: 'executable', overlay: overlayProc?.pid ?? null };
  }
  return { ok: false, error: 'mc-no-executable', displayName: inst.displayName };
}

async function main() {
  const profilesDir = () => {
    mkdirSync(app.getPath('userData'), { recursive: true });
    return app.getPath('userData');
  };
  const profilePath = (id) => join(profilesDir(), `profile-${id}.json`);
  const readProfile = (id) => readJson(profilePath(id), null);
  const readModules = () => readJson(bridgePath('modules.json'), {});
  const writeModules = (m) => writeFileSync(bridgePath('modules.json'), JSON.stringify(m, null, 2));
  const ensureModulesFile = () => {
    const current = readModules();
    let changed = false;
    for (const mod of MODULES_CATALOG) {
      if (!current[mod.id]) { current[mod.id] = { enabled: false, updatedAt: new Date().toISOString() }; changed = true; }
    }
    if (changed) writeModules(current);
  };
  // Catálogo para el mod menu in-game (F8): nombres, categorías y
  // descripciones reales. El motor nativo lo lee de catalog.json.
  const ensureCatalogFile = () => {
    const list = MODULES_CATALOG.map((m) => ({
      id: m.id,
      name: m.name,
      category: m.category,
      dataSource: m.dataSource ?? '',
      description: m.description ?? '',
    }));
    writeFileSync(bridgePath('catalog.json'), JSON.stringify(list, null, 2));
  };

  // Galería de cosméticos: sembramos los incluidos y fusionamos con lo guardado.
  const ensureCosmeticsFile = () => {
    const p = bridgePath('cosmetics.json');
    const saved = readJson(p, []);
    const list = withBuiltinCosmetics(Array.isArray(saved) ? saved : []);
    writeFileSync(p, JSON.stringify(list, null, 2));
    return list;
  };
  const readCosmetics = () => withBuiltinCosmetics(readJson(bridgePath('cosmetics.json'), []));

  // Sólo nombre de archivo: un JSON importado no puede escapar de assets/cosmetics.
  const safeAssetName = (value) => typeof value === 'string'
    ? value.replace(/\\/g, '/').split('/').filter(Boolean).pop() ?? ''
    : '';

  function createTray() {
    try {
      const iconPath = join(app.getAppPath(), 'assets/tray.png');
      const icon = nativeImage.createFromPath(iconPath);
      tray = new Tray(icon.isEmpty() ? nativeImage.createEmpty() : icon);
      tray.setToolTip('LEX CLIENT');
      tray.setContextMenu(Menu.buildFromTemplate([
        { label: 'Abrir LEX CLIENT', click: () => { if (mainWindow) { mainWindow.show(); mainWindow.focus(); } } },
        { label: 'Salir', click: () => app.quit() },
      ]));
      tray.on('double-click', () => { if (mainWindow) { mainWindow.show(); mainWindow.focus(); } });
    } catch {
      /* bandeja opcional */
    }
  }

  app.whenReady().then(() => {
    ensureModulesFile();
    ensureCatalogFile();
    ensureCosmeticsFile();
    function createWindow() {
      const win = new BrowserWindow({
        width: 1240,
        height: 820,
        title: 'LEX CLIENT',
        webPreferences: { preload: join(app.getAppPath(), 'electron/preload.cjs') },
      });
      mainWindow = win;
      if (app.isPackaged) {
        win.loadFile(join(app.getAppPath(), 'dist/renderer/index.html'));
      } else {
        win.loadURL(DEV_URL);
      }
      win.show();
      return win;
    }
    createWindow();
    createTray();

    // Detección inicial (caché mapeado a IPC)
    (async () => {
      detectionCache = await detectAll();
    })().catch(() => {
      detectionCache = [];
    });

    // === IPC: Minecraft ===
    ipcMain.handle('bedrock:detect', async () => {
      detectionCache = await detectAll();
      return { installations: detectionCache, latest: pickLatest(detectionCache) };
    });
    ipcMain.handle('game:launch', (_e, id) => launchGame(id));
    ipcMain.handle('game:window', () => ({ installed: loadInstances().map((i) => ({ id: i.id, displayName: i.displayName, version: i.version, edition: i.edition, isPreview: i.isPreview })) }));
    ipcMain.handle('game:alive', (_e, id) => {
      const p = childByGame.get(id);
      return { ok: !!p, pid: p?.pid };
    });

    // === IPC: catálogo de módulos ===
    ipcMain.handle('modules:catalog', () => ({ modules: MODULES_CATALOG }));

    // === IPC: estado de módulos (persistido en %APPDATA%\lex-client\modules.json) ===
    ipcMain.handle('modules:get', () => readModules());
    ipcMain.handle('modules:setState', (_e, state) => {
      const m = readModules();
      m[state.id] = { enabled: !!state.enabled, updatedAt: new Date().toISOString() };
      writeModules(m);
      for (const win of BrowserWindow.getAllWindows()) win.webContents.send('lex:modules-updated');
      return { ok: true, id: state.id, enabled: !!state.enabled };
    });

    // === IPC: Diagnostics ===
    ipcMain.handle('diagnostics:run', () => buildDiagnostics());
    ipcMain.handle('diagnostics:apply-fix', (_e, id) => {
      switch (id) {
        case 'mc-no-install':
          return { applied: false, id, note: 'Funcionality de instalación requiere confirmación del usuario (abierto en el navegador del sistema).', url: 'https://www.minecraft.net/es-es/download/windows' };
        case 'mc-no-executable':
          return { applied: false, id, note: 'Reintentado: se relanza la detección; sin launchable encontrado no hay acción posible de forma segura.' };
        default:
          return { applied: false, id, note: 'Reintentado.' };
      }
    });

    // === IPC: Updates ===
    ipcMain.handle('updates:check', () => detectUpdates());

    // === IPC: Overlay state bridge ===
    ipcMain.handle('overlay:set-state', (_e, state) => {
      // Ruta compartida con el motor nativo: %APPDATA%\lex-client\overlay-state.json
      const dir = join(app.getPath('appData'), 'lex-client');
      mkdirSync(dir, { recursive: true });
      const p = join(dir, 'overlay-state.json');
      writeFileSync(p, JSON.stringify({ ts: new Date().toISOString(), ...state }, null, 2));
      return { ok: true };
    });

    // === IPC: Cosméticos bridge (el menú in-game los lee desde aquí) ===
    ipcMain.handle('cosmetics:get', () => readCosmetics());
    ipcMain.handle('cosmetics:save', (_e, items) => {
      const list = withBuiltinCosmetics(Array.isArray(items) ? items : []);
      writeFileSync(bridgePath('cosmetics.json'), JSON.stringify(list, null, 2));
      return { ok: true, count: list.length };
    });
    // Preview animado en la galería: devuelve el atlas PNG como data URL.
    ipcMain.handle('cosmetics:preview', (_e, file) => {
      const name = safeAssetName(file);
      if (!/^[A-Za-z0-9._-]+\.png$/i.test(name)) return { dataUrl: null };
      try {
        const bytes = readFileSync(join(app.getAppPath(), 'assets', 'cosmetics', name));
        return { dataUrl: `data:image/png;base64,${bytes.toString('base64')}` };
      } catch {
        return { dataUrl: null };
      }
    });

    // Abrir el .mcpack con Minecraft: el juego aplica la geometría real al jugador.
    ipcMain.handle('cosmetics:open-pack', async () => {
      const pack = join(app.getAppPath(), 'assets', 'cosmetics', 'animated-cosmetics.mcpack');
      try { await shell.openPath(pack); return { ok: true }; }
      catch (e) { return { ok: false, error: String(e) }; }
    });

    // Instalar el pack en la carpeta de resource packs de Minecraft y abrirlo.
    ipcMain.handle('cosmetics:install-pack', async () => {
      const pack = join(app.getAppPath(), 'assets', 'cosmetics', 'animated-cosmetics.mcpack');
      const candidates = [
        join(process.env.LOCALAPPDATA || '', 'Packages', 'Microsoft.MinecraftUWP_8wekyb3d8bbwe', 'LocalState', 'games', 'com.mojang', 'resource_packs'),
        join(process.env.LOCALAPPDATA || '', 'Packages', 'Microsoft.MinecraftWindowsBeta_8wekyb3d8bbwe', 'LocalState', 'games', 'com.mojang', 'resource_packs'),
        join(app.getPath('roaming'), 'Minecraft', 'resource_packs'),
        join(process.env.LOCALAPPDATA || '', 'Temp', 'Minecraft Bedrock', 'minecraftpe', 'resource_packs'),
      ];
      const existing = candidates.find((p) => existsSync(p));
      const targetDir = existing || candidates[0];
      let copied = false, error = null;
      try {
        mkdirSync(targetDir, { recursive: true });
        copyFileSync(pack, join(targetDir, 'animated-cosmetics.mcpack'));
        copied = true;
      } catch (e) { error = String(e); }
      writeFileSync(join(app.getPath('userData'), 'lex-cosmetics-installed.json'),
        JSON.stringify({ path: join(targetDir, 'animated-cosmetics.mcpack'), copied }, null, 2));
      try { await shell.openPath(pack); } catch {}
      return { ok: copied, path: join(targetDir, 'animated-cosmetics.mcpack'), error };
    });

    // Importar el logotipo oficial desde PNG del usuario y prepararlo para el overlay.
    ipcMain.handle('logo:get', () => {
      try {
        const p = join(app.getPath('userData'), 'lex-logo.png');
        const bytes = readFileSync(p);
        return { dataUrl: `data:image/png;base64,${bytes.toString('base64')}` };
      } catch { return { dataUrl: null }; }
    });
    ipcMain.handle('logo:pick', async () => {
      const res = await dialog.showOpenDialog(mainWindow, {
        title: 'Importar logotipo de LEX CLIENT',
        filters: [{ name: 'PNG', extensions: ['png'] }],
        properties: ['openFile'],
      });
      if (res.canceled || !res.filePaths[0]) return { ok: false };
      const file = res.filePaths[0];
      const dest = join(app.getPath('userData'), 'lex-logo.png');
      const bytes = readFileSync(file);
      writeFileSync(dest, bytes);
      // BGRA crudo para el overlay nativo (header: ancho, alto).
      try {
        const img = nativeImage.createFromPath(file);
        const size = img.getSize();
        const bmp = img.toBitmap(); // RGBA
        const out = Buffer.alloc(8 + size.width * size.height * 4);
        out.writeInt32LE(size.width, 0); out.writeInt32LE(size.height, 4);
        for (let i = 0; i < size.width * size.height; ++i) {
          out[8 + i * 4] = bmp[i * 4 + 2];
          out[8 + i * 4 + 1] = bmp[i * 4 + 1];
          out[8 + i * 4 + 2] = bmp[i * 4];
          out[8 + i * 4 + 3] = bmp[i * 4 + 3];
        }
        writeFileSync(join(app.getPath('userData'), 'lex-logo.bgra'), out);
      } catch { /* el launcher sigue funcionando aunque falle el BGRA */ }
      mainWindow?.webContents.send('lex:logo-updated');
      return { ok: true };
    });

    // === IPC: Profiles ===
    ipcMain.handle('profiles:list', () => {
      try {
        return readdirSync(profilesDir()).filter((f) => /^profile-.*\.json$/.test(f)).map((f) => readProfile(f.replace(/^profile-|\.json$/g, '')));
      } catch {
        return [];
      }
    });
    ipcMain.handle('profiles:apply', (_e, id) => {
      const cfg = readProfile(id);
      if (!cfg) return { ok: false, error: 'profile-not-found' };
      writeFileSync(profilePath(id), JSON.stringify({ appliedAt: new Date().toISOString(), ...cfg }, null, 2));
      return { ok: true, id };
    });

    // === IPC: Auth (stubs — no credenciales almacenadas) ===
    ipcMain.handle('auth:status', () => ({ signedIn: false, account: null, note: 'Inicia sesión en la Microsoft Store; LEX CLIENT no almacena credenciales de cuenta.' }));
    ipcMain.handle('auth:logout', () => ({ ok: true }));
  });

  app.webContentsCreated((_e, webContents) => {
    webContents.on('will-navigate', async (ev, url) => {
      if (url.startsWith('http') && !url.includes('127.0.0.1')) {
        ev.preventDefault();
        await shell.openExternal(url);
      }
    });
  });

  app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
  });

  app.on('before-quit', () => {
    for (const child of childByGame.values()) {
      try {
        child.kill('SIGTERM');
      } catch {
        /* ignore */
      }
    }
  });
}

main().catch((err) => {
  console.error('[LEX] error fatal en main:', err);
});
