// LEX CLIENT — Integraciones: Detección de Minecraft Bedrock (TS).
// Wrapper REAL around PowerShell (Get-AppxPackage) — similar a detection.mjs de Electron main.
import { execFile } from 'node:child_process';
import type { MinecraftInstallation, MinecraftEdition } from '@lex/shared';

export interface MinecraftInstallInfo {
  installations: MinecraftInstallation[];
  latest: MinecraftInstallation | null;
  source: 'powershell-appxpackage';
}

interface RawPackage {
  Name: string;
  Version: string;
}

function normalizeInternalVersion(v: string): string {
  const parts = v.split('.');
  if (parts.length === 0) return v;
  return v;
}

function detectEditionByPackageName(pkg: string): MinecraftEdition {
  const p = pkg.toLowerCase();
  if (p.startsWith('microsoft.minecraftwindowsbeta_')) return 'preview';
  if (p.startsWith('microsoft.minecrafteducationedition_')) return 'education';
  return 'retail';
}

function parsePackageEntry(entry: RawPackage): MinecraftInstallation {
  const version = normalizeInternalVersion(entry.Version);
  const edition = detectEditionByPackageName(entry.Name);
  return {
    id: entry.Name,
    packageId: entry.Name,
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
  return new Promise((resolve, reject) => {
    execFile('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], { encoding: 'utf8', timeout: 20000 }, (err, stdout) => {
      if (err) {
        // PowerShell o la herramienta de herramientas CUIDADO: resolvemos vacío para no matar.
        resolve([]);
        return;
      }
      const lines = (stdout ?? '')
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter((l) => l.includes('|'));
      resolve(lines);
    });
  });
}

export async function detect(): Promise<MinecraftInstallInfo> {
  const lines = await runPowerShell(
    "Get-AppxPackage -Name 'Microsoft.minecraftUWP_*' -ErrorAction SilentlyContinue | ForEach-Object { \"$($_.Name)|$($_.Version)\" }; " +
      "Get-AppxPackage -Name 'Microsoft.MinecraftWindowsBeta_*' -ErrorAction SilentlyContinue | ForEach-Object { \"$($_.Name)|$($_.Version)\" }",
  );
  const installations: MinecraftInstallation[] = [];
  for (const line of lines) {
    const [name, version] = line.split('|');
    if (name && version) installations.push(parsePackageEntry({ Name: name, Version: version }));
  }
  const sorted = [...installations].sort((a, b) => b.version.localeCompare(a.version));
  return { installations: sorted, latest: sorted[0] ?? null, source: 'powershell-appxpackage' };
}

export function pickLatest(info: MinecraftInstallInfo): MinecraftInstallation | null {
  return info.latest;
}

export type { MinecraftInstallation };
