// LEX CLIENT — Módulo HUD: Keystrokes.
// FUENTE: engine externo — keyLog (ring de teclas, capturado vía hooks LLM; passthrough real).
import type { Module, ModuleMeta } from '@lex/module-sdk';
import { readOverlayStats } from '@lex/shared';

/** Datos reales: número de keystrokes registrados en el ring del engine (best effort). */
export function readKeystrokeCount(): number | null {
  const s = readOverlayStats();
  return s && !s.isStale ? s.keyCount : null;
}

export const meta: ModuleMeta = {
  id: 'hud-keystrokes',
  name: 'Keystrokes',
  description: 'Teclas presionadas/soltones en tiempo real. Fuente: engine externo — keyLog (teclado LLM, passthrough).',
  category: 'hud',
  dataSource: 'external-overlay',
  overlayRenderer: 'hud-keystrokes',
};

export const keystrokesModule: Module = {
  meta,
  activate() {
    // Nothing — registro de keybinds vía modules/utilities/keybinds.
  },
  deactivate() {
    // Nothing.
  },
};
