// LEX CLIENT — Índice de módulos: registro estático de todos los módulos (para Fase 4+).
import { ModuleRegistry } from '@lex/module-sdk';
import type { Module, ModuleMeta } from '@lex/module-sdk';

import { fpsModule } from './hud/fps.js';
import { cpsModule } from './hud/cps.js';
import { keystrokesModule } from './hud/keystrokes.js';
import { clockModule } from './hud/clock.js';
import { memoryModule } from './hud/memory.js';
import { crosshairModule } from './visual/crosshair.js';
import { fogModule } from './visual/fog.js';
import { colorModule } from './visual/color.js';
import { deathCounterModule } from './gameplay/death_counter.js';
import { keybindsModule } from './utilities/keybinds.js';

export const DEFAULT_MODULES: Module[] = [
  fpsModule, cpsModule, keystrokesModule, clockModule, memoryModule,
  crosshairModule, fogModule, colorModule,
  deathCounterModule, keybindsModule,
];

export function createModuleRegistry(): ModuleRegistry {
  const registry = new ModuleRegistry();
  for (const module of DEFAULT_MODULES) registry.register(module);
  return registry;
}

export const MODULE_METAS: ModuleMeta[] = DEFAULT_MODULES.map((m) => m.meta);
export { DEFAULT_MODULES, ModuleRegistry };
export type { Module, ModuleMeta };
