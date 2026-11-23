// @lex/module-sdk — SDK para módulos de LEX CLIENT.
// Patrón: ModuleMeta (descriptor) + Module (implementación). Modulos activos renderizan por overlay externo.

import type { ModuleCategory, ModuleMeta, ModuleRuntimeState, IpcChannel } from '@lex/shared';

export interface ModuleContext {
  /** Estado activo actual (para evitar re-render innecesario). */
  isActive: boolean;
}

export interface RenderFrameContext {
  width: number;
  height: number;
  /** Timestamp del frame actual (rango loops). */
  timestamp: number;
  /** FPS real del overlay (propio del motor D2D), no FPS del juego. */
  overlayFps: number;
}

export interface GameStateSnapshot {
  fps?: number;
  tps?: number;
  coords?: { x: number; y: number; z: number };
}

export interface Module {
  meta: ModuleMeta;
  /** Se llama en activación. Nada debe inyectarse en el juego. */
  activate(ctx: ModuleContext): void;
  deactivate(): void;
}

export interface ModuleRegistryHandle {
  register(module: Module): void;
  unregister(id: string): void;
  get(id: string): Module | undefined;
  list(): ModuleMeta[];
  byCategory(category: ModuleCategory): ModuleMeta[];
  byKeyword(q: string): ModuleMeta[];
  setEnabled(id: string, enabled: boolean): void;
  isEnabled(id: string): boolean;
  state(): Record<string, ModuleRuntimeState>;
  activeIds(): string[];
  active(): Module[];
}

export class ModuleRegistry implements ModuleRegistryHandle {
  private modules = new Map<string, Module>();
  private states = new Map<string, ModuleRuntimeState>();

  register(module: Module): void {
    this.modules.set(module.meta.id, module);
    this.states.set(module.meta.id, this.states.get(module.meta.id) ?? { moduleId: module.meta.id, enabled: false });
  }

  unregister(id: string): void {
    this.modules.delete(id);
    this.states.delete(id);
  }

  get(id: string): Module | undefined {
    return this.modules.get(id);
  }

  list(): ModuleMeta[] {
    return [...this.modules.values()].map((m) => m.meta);
  }

  byCategory(category: ModuleCategory): ModuleMeta[] {
    return this.list().filter((m) => m.category === category);
  }

  byKeyword(q: string): ModuleMeta[] {
    const needle = q.toLowerCase();
    return this.list().filter(
      (m) => m.name.toLowerCase().includes(needle) || m.description.toLowerCase().includes(needle),
    );
  }

  setEnabled(id: string, enabled: boolean): void {
    const current = this.states.get(id) ?? { moduleId: id, enabled: false };
    this.states.set(id, { ...current, enabled });
  }

  isEnabled(id: string): boolean {
    return this.states.get(id)?.enabled ?? false;
  }

  state(): Record<string, ModuleRuntimeState> {
    return Object.fromEntries(this.states);
  }

  activeIds(): string[] {
    return [...this.states.entries()].filter(([, s]) => s.enabled).map(([id]) => id);
  }

  active(): Module[] {
    return this.activeIds().map((id) => this.modules.get(id)).filter((m): m is Module => !!m);
  }
}

export { IpcChannel };
