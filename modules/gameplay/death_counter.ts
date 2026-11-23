// LEX CLIENT — MóduloGameplay: Death Counter.
// FUENTE: reservado — pendiente de fuente fiable (script api v1 en Fase 5).
import type { Module, ModuleMeta } from '@lex/module-sdk';

export const meta: ModuleMeta = {
  id: 'game-drift',
  name: 'Death Counter',
  description: 'Contador de muertes. Fuente: adaptador Script API (pendiente Fase 5).',
  category: 'gameplay',
  dataSource: 'reserved',
};

export const deathCounterModule: Module = {
  meta,
  activate() {
    // Nothing.
  },
  deactivate() {
    // Nothing.
  },
};
