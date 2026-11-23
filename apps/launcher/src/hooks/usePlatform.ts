// LEX CLIENT — Info de plataforma (OS, arch, node) para diagnóstico.
export interface PlatformInfo { os: string; arch: string; node: string; electron: string; launcherVersion: string }

export async function getPlatform(): Promise<PlatformInfo> {
  const r = await window.lex.ipc.diagnostics();
  return {
    os: (r as { platform?: { os: string; arch: string; node: string } }).platform?.os ?? 'unknown',
    arch: (r as { platform?: { arch: string } }).platform?.arch ?? 'x64',
    node: (r as { platform?: { node: string } }).platform?.node ?? 'unknown',
    electron: (r as { platform?: { electron: string } }).platform?.electron ?? 'unknown',
    launcherVersion: (r as { launcherVersion?: string }).launcherVersion ?? '0.1.0',
  };
}
