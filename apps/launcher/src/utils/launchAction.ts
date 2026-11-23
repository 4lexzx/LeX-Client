import type { MinecraftInstallation } from '@lex/shared';

/** Resultado del lanzamiento + datos de renderer para el Home. */
export interface LaunchOutcome {
  ok: boolean;
  displayName: string | null;
  version: string | null;
  pid: number | null;
}

export async function launchGame(installation: MinecraftInstallation): Promise<LaunchOutcome> {
  const r = await window.lex.ipc.launch(installation.id);
  return { ok: r.ok, displayName: r.displayName ?? null, version: r.version ?? null, pid: r.pid ?? null };
}
