import { execFile, spawn } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

const FAMILY = 'Microsoft.MinecraftUWP_8wekyb3d8bbwe';

const run = (file, args, timeout = 20000) => new Promise((resolve) => {
  execFile(file, args, { windowsHide: true, timeout }, (error, stdout) => {
    const code = error ? (typeof error.code === 'number' ? error.code : -1) : 0;
    resolve({ code, out: String(stdout).trim() });
  });
});

// Ruta rapida: la carpeta del juego y su manifiesto, sin abrir PowerShell.
function quick() {
  for (const letter of 'CDEFGH') {
    const dir = `${letter}:\\XboxGames\\Minecraft for Windows\\Content`;
    if (!existsSync(join(dir, 'data', 'resource_packs', 'vanilla'))) continue;
    for (const name of ['appxmanifest.xml', 'AppxManifest.xml']) {
      try {
        const version = /<Identity[^>]*\bVersion="([\d.]+)"/.exec(readFileSync(join(dir, name), 'utf8'));
        if (version) return { installed: true, version: displayVersion(version[1]), dir };
      } catch { /* sin permiso o sin archivo */ }
    }
  }
  return null;
}

export async function detect() {
  const found = quick();
  if (found) return found;
  const script = 'Get-AppxPackage Microsoft.MinecraftUWP | Select-Object -First 1 Version,InstallLocation | ConvertTo-Json -Compress';
  const { out } = await run('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', script]);
  let info = null;
  try {
    info = JSON.parse(out);
  } catch {
    // sin instalacion
  }
  const candidates = [...'CDEFGH'].map((l) => `${l}:\\XboxGames\\Minecraft for Windows\\Content`);
  if (info?.InstallLocation) candidates.push(info.InstallLocation);
  const dir = candidates.find((c) => existsSync(join(c, 'data', 'resource_packs', 'vanilla'))) ?? null;
  return { installed: Boolean(info), version: info ? displayVersion(info.Version) : null, dir };
}

// El paquete usa 1.26.6501.0 para lo que el juego muestra como 1.26.65.1.
export function displayVersion(raw) {
  const [a, b, c] = String(raw).split('.');
  const n = Number(c);
  if (!(n >= 100)) return String(raw);
  return `${a}.${b}.${Math.floor(n / 100)}${n % 100 ? `.${n % 100}` : ''}`;
}

export async function isRunning() {
  const { out } = await run('tasklist.exe', ['/FI', 'IMAGENAME eq Minecraft.Windows.exe', '/NH']);
  return /Minecraft\.Windows/i.test(out);
}

export function launch() {
  spawn('explorer.exe', [`shell:AppsFolder\\${FAMILY}!Game`], { detached: true, stdio: 'ignore' }).unref();
}

const RESULTS = {
  0: { ok: true, message: 'LeX está activo. Entra en un mundo y pulsa L.' },
  1: { ok: false, message: 'Faltan los archivos nativos. Compila el cliente primero.' },
  2: { ok: false, message: 'Minecraft no llegó a abrirse a tiempo. Inténtalo otra vez.' },
  3: { ok: false, message: 'Windows no permitió acceder al juego. Ciérralo y ábrelo desde aquí.' },
};

export async function inject(nativeDir) {
  const exe = join(nativeDir, 'lex-injector.exe');
  const dll = join(nativeDir, 'LeXClient.dll');
  if (!existsSync(exe) || !existsSync(dll)) return RESULTS[1];
  const { code } = await run(exe, [dll, '90'], 120000);
  return RESULTS[code] ?? { ok: false, message: 'No se pudo cargar LeX. Revisa si tu antivirus lo bloqueó.' };
}
