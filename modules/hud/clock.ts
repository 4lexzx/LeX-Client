// LEX CLIENT — Módulo HUD: Clock.
// FUENTE: local-only — datos del reloj de la máquina (trivial).
import type { Module, ModuleMeta } from '@lex/module-sdk';

export const meta: ModuleMeta = {
  id: 'hud-clock',
  name: 'Clock',
  description: 'Reloj de pared tiempo real (24h). Fuente: local (system time).',
  category: 'hud',
  dataSource: 'local-only',
  overlayRenderer: 'hud-clock',
};

export const clockModule: Module = {
  meta,
  activate() {
    // Nothing — el reloj actualiza cada tick del engine.
  },
  deactivate() {
    // Nothing.
  },
};
