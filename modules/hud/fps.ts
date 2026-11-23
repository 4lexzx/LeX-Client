// LEX CLIENT — Módulo HUD: FPS Counter.
// FUENTE: engine de overlays externo nativo (native/overlay-engine) — overlay-stats.json.
import type { Module, ModuleMeta } from '@lex/module-sdk';
import { readOverlayStats } from '@lex/shared';

/** Datos reais: FPS del engine de overlays nativo (best effort, sin inyección). */
export function readFps(): number | null {
  const s = readOverlayStats();
  return s && !s.isStale ? s.overlayFps : null;
}

export const meta: ModuleMeta = {
  id: 'hud-fps',
  name: 'FPS Counter',
  description: 'Contador de frames por segundo. Fuente: overlay externo nativo (native/overlay-engine) — stats escritos a overlay-stats.json. Resolución mediante polling: TODO (revisar Fase 5).',
  category: 'hud',
  dataSource: 'external-overlay',
  overlayRenderer: 'hud-fps',
};

export const fpsModule: Module = {
  meta,
  activate() {
    // Nothing.
  },
  deactivate() {
    // Nothing.
  },
};
