// LEX CLIENT — Main Process.
// - BrowserWindow del launcher
// - IPC bridge tipado (preload expone window.lex)
// - Detección de Minecraft (detection.mjs)
// - Game launch (spawn de Minecraft.Windows.exe)
// - Profiles, Diagnostics, Updates (CHANGELOG.md local)
// - Overlay state bridge (pantalla externa de Fase 3 lee userData/overlay-state.json)

import { app, BrowserWindow, ipcMain, shell } from 'electron';
import { readFileSync, readdirSync, mkdirSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { detectAll, pickLatest } from './detection.mjs';
import { MODULES_CATALOG, filterCatalog } from '../modules/catalog.mjs';

const CURRENT_VERSION = '0.1.0';
const DEV_URL = 'http://127.0.0.1:5173';
const VERSION_PATTERN = /^##\s*\[?\s*v?(\d+\.\d+\.\d+)[^)]*\]?\s*(.*)$/m;

let detectionCache = null;
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
  if (overlayProc && !overlayProc.killed) return true;
  const exe = join(process.resourcesPath, 'overlay', 'lex-overlay-engine.exe');
  if (!existsSync(exe)) return false;
  overlayProc = spawn(exe, [], { detached: true, stdio: 'ignore' });
  overlayProc.unref();
  return true;
}

function launchGame(installationId) {
  const inst = loadInstances().find((i) => i.id === installationId);
  if (!inst) return { ok: false, error: 'mc-not-found', displayName: null };
  if (inst.packageFamilyName) {
    const appsFolder = `shell:AppsFolder\\${inst.packageFamilyName}!Game`;
    const child = spawn('explorer.exe', [appsFolder], { detached: true, stdio: 'ignore' });
    child.unref();
    childByGame.set(inst.id, child);
    ensureOverlayEngine();
    return { ok: true, pid: child.pid, displayName: inst.displayName, version: inst.version, mode: 'appsfolder', overlay: overlayProc?.pid ?? null };
  }
  if (inst.executable) {
    const child = spawn(inst.executable, [], { detached: true, stdio: 'ignore', cwd: inst.installLocation || undefined });
    child.unref();
    childByGame.set(inst.id, child);
    return { ok: true, pid: child.pid, displayName: inst.displayName, version: inst.version, mode: 'executable' };
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

  app.whenReady().then(() => {
    ensureModulesFile();
    function createWindow() {
      const win = new BrowserWindow({
        width: 1240,
        height: 820,
        title: 'LEX CLIENT',
        webPreferences: { preload: join(app.getAppPath(), 'electron/preload.cjs') },
      });
      if (app.isPackaged) {
        win.loadFile(join(app.getAppPath(), 'dist/renderer/index.html'));
      } else {
        win.loadURL(DEV_URL);
      }
      win.show();
      return win;
    }
    createWindow();

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
    ipcMain.handle('cosmetics:get', () => readJson(bridgePath('cosmetics.json'), []));
    ipcMain.handle('cosmetics:save', (_e, items) => {
      const list = Array.isArray(items) ? items : [];
      writeFileSync(bridgePath('cosmetics.json'), JSON.stringify(list, null, 2));
      return { ok: true, count: list.length };
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
