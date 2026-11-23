// @lex/shared — normalización y detección de versiones de Minecraft Bedrock.

import { execFile } from 'node:child_process';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import type { MinecraftEdition, MinecraftInstallation } from './types.js';

const RETAIL_PREFIX = 'microsoft.minecraftuwp_';
const PREVIEW_PREFIX = 'microsoft.minecraftwindowsbeta_';
const EDUCATION_PREFIX = 'microsoft.minecrafteducationedition_';

export function normalizeInternalVersion(v: string | undefined | null): string | null {
  if (!v) return null;
  const parts = v.trim().split('.');
  if (parts.length === 0) return null;
  const valid = parts.every((p) => /^\d+$/.test(p) && parseInt(p, 10) >= 0);
  if (!valid) return null;
  return v.trim();
}

export function detectEditionByPackageName(pkg: string): MinecraftEdition {
  const p = pkg.toLowerCase();
  if (p.startsWith(PREVIEW_PREFIX)) return 'preview';
  if (p.startsWith(EDUCATION_PREFIX.toLowerCase())) return 'education';
  if (p.startsWith(RETAIL_PREFIX)) return 'retail';
  return 'retail';
}

export function detectInstallationFromPackageEntry(entry: { name: string; version?: string }): MinecraftInstallation | null {
  const pkg = entry.name;
  const version = normalizeInternalVersion(entry.version ?? '');
  if (!version) return null;
  const edition = detectEditionByPackageName(pkg);
  return {
    id: pkg,
    packageId: pkg,
    displayName: edition === 'preview' ? 'Minecraft Preview for Windows' : 'Minecraft for Windows',
    edition,
    version,
    executable: null,
    installLocation: '',
    source: 'powershell-appxpackage',
    isPreview: edition === 'preview',
    detectedAt: new Date().toISOString(),
  };
}

function runPowerShell(script: string): Promise<string[]> {
  return new Promise((resolve) => {
    execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], { encoding: 'utf8', timeout: 15000 }, (_err, stdout) => {
      const lines = (stdout ?? '')
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter((l) => l.length > 0);
      resolve(lines);
    });
  });
}

export async function detectViaPowerShell(): Promise<MinecraftInstallation[]> {
  const lines = await runPowerShell(
    "Get-AppxPackage -Name 'Microsoft.minecraftUWP_*' -ErrorAction SilentlyContinue | ForEach-Object { \"$($_.Name)|$($_.Version)\" }; " +
      "Get-AppxPackage -Name 'Microsoft.MinecraftWindowsBeta_*' -ErrorAction SilentlyContinue | ForEach-Object { \"$($_.Name)|$($_.Version)\" }",
  );
  const out: MinecraftInstallation[] = [];
  for (const line of lines) {
    const [name, version] = line.split('|');
    const inst = detectInstallationFromPackageEntry({ name, version });
    if (inst) out.push(inst);
  }
  return out;
}

export function extractVersionFromFolderName(pkg: string): string | null {
  const m = pkg.match(/(?:^|_)(\d+\.\d+(?:\.\d+)*)_x64__/i);
  if (m) return m[1];
  return null;
}

function findExecutable(dir: string): string | null {
  for (const name of ['Minecraft.Windows.exe', 'minecraft.exe', 'Blockmap.xml']) {
    try {
      const p = join(dir, name);
      if (statSync(p).isFile()) return p;
    } catch {
      /* not found */
    }
  }
  try {
    for (const entry of readdirSync(dir)) {
      if (/\.exe$/i.test(entry)) return join(dir, entry);
    }
  } catch {
    /* ignore */
  }
  return null;
}

export async function detectViaFileSystem(): Promise<MinecraftInstallation[]> {
  const bases = [
    process.env['ProgramFiles'] ?? 'C:\\Program Files',
    process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)',
    process.env['LOCALAPPDATA'] ?? '',
  ];
  const installRoots: string[] = [];
  for (const b of bases) {
    if (b) {
      installRoots.push(join(b, 'WindowsApps'));
      installRoots.push(join(b, 'Packages'));
    }
  }

  const results: MinecraftInstallation[] = [];
  for (const root of installRoots) {
    let entries: string[] = [];
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
        const exe = findExecutable(dir);
        results.push({
          id: name,
          packageId: name,
          displayName: edition === 'preview' ? 'Minecraft Preview for Windows' : 'Minecraft for Windows',
          edition,
          version,
          executable: exe,
          installLocation: dir,
          source: 'filesystem-manifest',
          isPreview: edition === 'preview',
          detectedAt: new Date().toISOString(),
        });
      } catch {
        /* inaccessible */
      }
    }
  }
  return results;
}

export async function detectAll(): Promise<MinecraftInstallation[]> {
  const [ps, fs] = await Promise.all([detectViaPowerShell(), detectViaFileSystem()]);
  const map = new Map<string, MinecraftInstallation>();
  for (const i of ps) map.set(i.packageId.toLowerCase(), i);
  for (const i of fs) {
    const k = i.packageId.toLowerCase();
    const existing = map.get(k);
    if (!existing) map.set(k, i);
    else if (i.executable && !existing.executable) map.set(k, i);
  }
  return [...map.values()].sort((a, b) => b.version.localeCompare(a.version));
}

export function pickLatest(installations: MinecraftInstallation[]): MinecraftInstallation | null {
  const sorted = [...installations].sort((a, b) => b.version.localeCompare(a.version));
  return sorted[0] ?? null;
}
