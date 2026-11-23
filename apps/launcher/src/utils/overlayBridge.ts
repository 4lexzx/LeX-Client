// LEX CLIENT — Bridge de estado del overlay externo (Fase 3).
export async function setOverlayState(state: Record<string, unknown>): Promise<{ ok: boolean }> {
  return window.lex.ipc.overlaySetState(state);
}
