// LEX CLIENT — Integración Script API: Esquemas de tipos (Fase 5b — Pendiente de implementación funcional).
// Fuente honesta: reservado. Per Klein docs/server-side/scripting-api, lo que se muestra:
// nombres tentativos solo para conexión futura con adapters por versión.
import type { MinecraftInstallation } from '@lex/shared';

export interface ScriptApiContext {
  /** Versión del adapter (0 = sin adapter). */
  adapterVersion: 0 | 1 | 2;
  /** Booleano de disponibilidad real. */
  available: boolean;
  /** Notas: "Pendiente Fase 5b" */
  note: 'adapters por versión — pendiente Fase 5b';
}

export interface ScriptApiGameState {
  worldId?: string;
  username?: string;
  position?: { x: number; y: number; z: number };
  /** Ticker de servidor (GameTicks, resumeable). */
  worldTick?: number;
}

export function getScriptApiContext(): ScriptApiContext {
  return { adapterVersion: 0, available: false, note: 'adapters por versión — pendiente Fase 5b' };
}

export type { MinecraftInstallation };
export type { ScriptApiContext, ScriptApiGameState };
