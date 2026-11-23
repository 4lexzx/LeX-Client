// LEX CLIENT — Módulo HUD: CPS Counter.
// FUENTE: engine de overlays externo nativo — CPS registrado por hooks (mouse/teclado passthrough).
import type { Module, ModuleMeta } from '@lex/module-sdk';
import { readOverlayStats } from '@lex/shared';

/** Datos reales: CPS promedio del engine de overlays nativo (ring de 4s). */
export function readCps(): number | null {
  const s = readOverlayStats();
  return s && !s.isStale ? s.cpsAvg : null;
}

export const meta: ModuleMeta = {
  id: 'hud-cps',
  name: 'CPS Counter',
  description: 'Clicks por segundo (clics del ratón). Fuente: engine externo — ring de CPS (4s) escrito a overlay-stats.json.',
  category: 'hud',
  dataSource: 'external-overlay',
  overlayRenderer: 'hud-cps',
};

export const cpsModule: Module = {
  meta,
  activate() {
    // Nothing — datos se leen por el engine al tick.
  },
  deactivate() {
    // Nothing.
  },
};
