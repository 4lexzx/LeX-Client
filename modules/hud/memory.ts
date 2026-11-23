// LEX CLIENT — Módulo HUD: Memory Display.
// FUENTE: game-memory-v1 — punteros/offsets de Minecraft Bedrock (adapters Fase 5, Pendiente de pruebas reales).
import type { Module, ModuleMeta } from '@lex/module-sdk';

/** Fuente futura: adapters de memoria de juego (Fase 5) — actualmente no hay punteros validos listos. */
export function readMemory(): { total: number | null; usage: number | null } {
  return { total: null, usage: null };
}

export const meta: ModuleMeta = {
  id: 'hud-memory',
  name: 'Memory Display',
  description: 'RAM y uso de memoria del juego. Fuente: adapters v1 (memoria de juego, Offsets pendientes a establecer en Fase 5).',
  category: 'hud',
  dataSource: 'game-memory-v1',
  overlayRenderer: 'hud-memory',
  requiresMinVersion: '1.21.120.2',
};

export const memoryModule: Module = {
  meta,
  activate() {
    // Nothing — se consulta al tick; revisar en Fase 5 con datos reales.
  },
  deactivate() {
    // Nothing.
  },
};
