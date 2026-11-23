// LEX CLIENT — Detection (electron main, plain JS).
// Version CANÓNICA en packages/shared/src/version.ts (TS); esta copia es para electron main.
// Dedicada a electron main no depende de tooling TS por compatibilidad de versiones de node.

import { execFile } from 'node:child_process';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';

const RETAIL_PREFIX = 'microsoft.minecraftuwp_';
const PREVIEW_PREFIX = 'microsoft.minecraftwindowsbeta_';
const EDUCATION_PREFIX = 'microsoft.minecrafteducationedition_';

export function normalizeInternalVersion(v) {
  if (!v) return null;
  const parts = String(v).trim().split('.');
  const valid = parts.length > 0 && parts.every((p) => /^\d+$/.test(p) && parseInt(p, 10) >= 0);
  return valid ? String(v).trim() : null;
}

export function detectEditionByPackageName(pkg) {
  const p = pkg.toLowerCase();
  if (p.startsWith(PREVIEW_PREFIX)) return 'preview';
  if (p.startsWith(EDUCATION_PREFIX)) return 'education';
  return 'retail';
}

export function detectInstallationFromPackageEntry(entry) {
  const version = normalizeInternalVersion(entry.version);
  if (!version) return null;
  const edition = detectEditionByPackageName(entry.name);
  const installLocation = entry.installLocation ?? '';
  return {
    id: entry.name,
    packageId: entry.name,
    displayName: edition === 'preview' ? 'Minecraft Preview for Windows' : edition === 'education' ? 'Minecraft Education Edition' : 'Minecraft for Windows',
    edition,
    version,
    executable: installLocation ? findExecutable(installLocation) : null,
    installLocation,
    packageFamilyName: entry.packageFamilyName ?? '',
    source: 'powershell-appxpackage',
    isPreview: edition === 'preview',
    detectedAt: new Date().toISOString(),
  };
}

function runPowerShell(script) {
  return new Promise((resolve) => {
    execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], { encoding: 'utf8', timeout: 15000 }, (_err, stdout) => {
      resolve((stdout ?? '')
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter((l) => l.length > 0));
    });
  });
}

async function detectViaPowerShell() {
  const fields = "$_.Name + '|' + $_.Version + '|' + $_.PackageFamilyName + '|' + $_.InstallLocation";
  const script = [
    `Get-AppxPackage -Name 'Microsoft.MinecraftUWP' -ErrorAction SilentlyContinue | ForEach-Object { ${fields} }`,
    `Get-AppxPackage -Name 'Microsoft.MinecraftWindowsBeta' -ErrorAction SilentlyContinue | ForEach-Object { ${fields} }`,
    `Get-AppxPackage -Name 'Microsoft.MinecraftEducationEdition' -ErrorAction SilentlyContinue | ForEach-Object { ${fields} }`,
  ].join('; ');
  const lines = await runPowerShell(script);
  const out = [];
  for (const line of lines) {
    const [name, version, packageFamilyName, installLocation] = line.split('|');
    const inst = detectInstallationFromPackageEntry({ name, version, packageFamilyName, installLocation });
    if (inst) out.push(inst);
  }
  return out;
}

export function extractVersionFromFolderName(pkg) {
  const m = pkg.match(/(?:^|_)(\d+\.\d+(?:\.\d+)*)_x64__/i);
  return m ? m[1] : null;
}

function findExecutable(dir) {
  for (const name of ['Minecraft.Windows.exe', 'minecraft.exe']) {
    try {
      if (statSync(join(dir, name)).isFile()) return join(dir, name);
    } catch {
      /* no existe */
    }
  }
  try {
    const exe = readdirSync(dir).find((n) => /\.exe$/i.test(n));
    return exe ? join(dir, exe) : null;
  } catch {
    return null;
  }
}

async function detectViaFileSystem() {
  const bases = [
    process.env.ProgramFiles ?? 'C:\\Program Files',
    process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)',
    process.env.LOCALAPPDATA ?? '',
  ];
  const installRoots = [];
  for (const b of bases) {
    if (b) {
      installRoots.push(join(b, 'WindowsApps'));
      installRoots.push(join(b, 'Packages'));
    }
  }
  const results = [];
  for (const root of installRoots) {
    let entries = [];
    try {
      entries = readdirSync(root);
    } catch {
      continue;
    }
    for (const name of entries) {
      if (!/^microsoft\.(minecraftuwp|minecraftwindowsbeta|minecrafteducationedition)_/i.test(name)) continue;
      const dir = join(root, name);
      try {
        if (!statSync(dir).isDirectory()) continue;
        const edition = detectEditionByPackageName(name);
        let version = normalizeInternalVersion(extractVersionFromFolderName(name));
        try {
          const xml = readFileSync(join(dir, 'AppxManifest.xml'), 'utf8');
          const m = xml.match(/Version="([^"]+)"/i);
          if (m && !version) version = normalizeInternalVersion(m[1]);
        } catch {
          /* no manifest */
        }
        if (!version) continue;
        results.push({
          id: name,
          packageId: name,
          displayName: edition === 'preview' ? 'Minecraft Preview for Windows' : 'Minecraft for Windows',
          edition,
          version,
          executable: findExecutable(dir),
          installLocation: dir,
          source: 'filesystem-manifest',
          isPreview: edition === 'preview',
          detectedAt: new Date().toISOString(),
        });
      } catch {
        /* inaccesible */
      }
    }
  }
  return results;
}

export async function detectAll() {
  const [ps, fs] = await Promise.all([detectViaPowerShell(), detectViaFileSystem()]);
  const map = new Map();
  for (const i of ps) map.set(i.packageId.toLowerCase(), i);
  for (const i of fs) {
    const k = i.packageId.toLowerCase();
    const existing = map.get(k);
    if (!existing) map.set(k, i);
    else if (i.executable && !existing.executable) map.set(k, i);
  }
  return [...map.values()].sort((a, b) => b.version.localeCompare(a.version));
}

export function pickLatest(installations) {
  const sorted = [...installations].sort((a, b) => b.version.localeCompare(a.version));
  return sorted[0] ?? null;
}
