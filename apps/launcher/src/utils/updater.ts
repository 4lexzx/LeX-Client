// LEX CLIENT — Updater: compara la versión del launcher con el CHANGELOG del repositorio (local o opt).
export interface UpdateInfo {
  launcher: string;
  latest: string;
  notes: string;
  changed: boolean;
}

export async function checkUpdates(): Promise<UpdateInfo> {
  const data = await window.lex.ipc.updatesCheck();
  return {
    launcher: data.launcherVersion ?? '0.1.0',
    latest: data.version,
    notes: data.notes,
    changed: !!data.changed,
  };
}
