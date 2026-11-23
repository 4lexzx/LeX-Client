# FASE 4 — Módulos (SDK + Implementaciones)

**Estado:** Completada (Fase 5 pendiente).

## Entregables

| Archivo | Descripción |
|---|---|
| `packages/module-sdk/src/index.ts` | SDK: `Module` (meta+activate/deactivate), `ModuleRegistry` (registro dinámico, estado, busqueda, categoría) |
| `modules/hud/fps.ts`, `cps.ts`, `keystrokes.ts`, `clock.ts`, `memory.ts` | Implementaciones de módulos HUD |
| `modules/visual/crosshair.ts`, `fog.ts`, `color.ts` | Implementaciones de módulos visuales |
| `modules/gameplay/death_counter.ts` | Módulo gameplay (fuente reservada) |
| `modules/utilities/keybinds.ts` | Módulo utilidades (registro de keybinds locales) |
| `modules/index.ts` | Índice: `createModuleRegistry()` + `DEFAULT_MODULES` |
| `modules/README.md` | Documentación de dataSource y reglas de honestidad |
| `packages/shared/src/overlayStats.ts` | Lector real de `%APPDATA%\lex-client\overlay-stats.json` (escrito por native/overlay-engine engine.cpp) |
| `packages/compatibility/src/overlayVersions.ts` | Versionado de engine overlay + paths (Fase 5) |
| `apps/launcher/tests/modules-registry.test.mjs` | Tests de integridad del registry |

## SDK (package `@lex/module-sdk`)

```ts
export interface Module {
  meta: ModuleMeta;
  activate(ctx): void;
  deactivate(): void;
}

export class ModuleRegistry {
  register(m: Module): void;
  unregister(id: string): void;
  get(id: string): Module | undefined;
  list(): ModuleMeta[];
  byCategory(cat): ModuleMeta[];
  byKeyword(q): ModuleMeta[];
  setEnabled(id, enabled): void;
  isEnabled(id): boolean;
  state(): Record<string, ModuleRuntimeState>;
  activeIds(): string[];
}
```

## Data sources — honestidad (reglas cumplidas)

| Módulo | Fuente real | Nota |
|---|---|---|
| FPS Counter | overlay-stats.json — `overlayFps` del engine D2D (EMA del loop). | No es fps del juego. Revisado en Fase 5 con datos reales (TGA). |
| CPS Counter | overlay-stats.json — `cpsAvg` (ring de 4s). | Reales (hooks LLM mouse). |
| Keystrokes | overlay-stats.json — `keyLog` (ring de 8, LLM teclado passthrough). | Reales. |
| Clock | local-only. | Sistema time. |
| Memory Display | game-memory-v1 — null hasta Fase 5 (adapters con offsets reales y pruebas). | No inventado. |
| Death Counter | reservado. | Fuente: Script API v1 — Fase 5. |

## Novedad implementada real

- `readOverlayStats()` (TS, en packages/shared) lee `%APPDATA%\lex-client\overlay-stats.json`, escrito a cada tick del engine (5 ticks/sec) por `writeOverlayStatsFile()`.
- El engine escribe ahora: `overlayFps` (EMA del loop D2D), `gameFpsTga` (-1 si hook TGA no activo), `cpsAvg`, `cpsClickCount`, `keyCount` (ring).

## Tests realizados

- `apps/launcher/tests/modules-registry.test.mjs`: integridad de catalogo y registry (createModuleRegistry, byCategory, setEnabled/isEnabled, unregister).
- `packages/shared/tests/overlayStats.test.mjs`: parseOverlayStatsJson (JSON real escrito por engine-like writer, mocks).
- `node --test` en launcher: 13/13 pass.

## Limitaciones Pendientes

- Implementación de módulos HUD directo en el renderer (append-to-store via `lex:emit`). (Habilitado en Fase 3/4b.)
- FPS en-game real mediante TGA (Fase 5).
- Memory Display adicional mediante memoria del proceso (Fase 5, con mocks).

## Instrucciones

```powershell
node --test packages/shared/tests/*.mjs apps/launcher/tests/*.mjs
npx tsc --noEmit --project apps/launcher
```
