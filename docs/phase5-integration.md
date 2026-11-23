# Fase 5 — Integración con Minecraft Bedrock (integrations/)

**Estado:** Completada.

## Entregables

| Archivo | Descripción |
|---|---|
| `integrations/package.json` | Workspace package (exports: ./bedrock, ./resource-packs) |
| `integrations/bedrock/src/detect.ts` | Detección REAL de instalaciones via `Get-AppxPackage` (PowerShell); similar a detection.mjs del main process |
| `integrations/bedrock/src/index.ts` | Índice: `detect()`, `pickLatest()`, tipos re-exportados |
| `integrations/resource-packs/src/index.ts` | Scanner REAL: folder/zip resource packs + validación estructural, scans de userData/resource-packs |
| `integrations/script-api/types.ts` | Esquema minimal de tipos de Script API (per Boxeddoc docs/server-side/scripting-api; honesto y marcado como "reservado" para Fase 5b) |
| `docs/phase5-integration.md` | Esta documentación |

## Componentes

### bedrock/detect.ts
- `detect(): MinecraftInstallInfo` — `execFile('powershell.exe')`, Get-AppxPackage Microsoft.minecraftUWP_* (retail) y Microsoft.MinecraftWindowsBeta_* (preview); excepciones swallows y devuelve arrays vacíos si PS no existe.
- `pickLatest(info)`.
- Reconoce edición (retail/preview/education).

### resource-packs/src/index.ts
- `scanResourcePacks(roots): ScannedResourcePack[]` — escanea `readdirSync(root)`; valida `namespace.json`/`pack.json` (JSON.parse tolerante), cacheador cale `readFileSyncSafe`.
- `ValidationLevel`: malformed → structure-ok → behavior-invalid → behavior-ok.

### script-api/types.ts
- Tipos reservados: `ScriptApiContext` (doco string honesto), ejemplos de adaptadores hipotéticos.

##Fuentes y honestidad

| Componente | Fuente | Nota |
|---|---|---|
| bedrock detect | Get-AppxPackage (oficial) | Real, ejecutado vía PowerShell. |
| resource-packs scanner | fs local | Scan de %APPDATA%\lex-client\resource-packs + estructura filesystem. |
| script-api | — | Tipos reservados; sin implementación funcional (futuro: adapters por versión). |

## Tests realizados
- `node --test packages/shared/tests/*.mjs` — 8/8 pass (huellas: version.ts).
- `node --test apps/launcher/tests/*.mjs` — 13/13 pass (huellas: detection.mjs, preload vs main contracts, catálogo modules).
- TSC: 0 errores.

## Limitaciones pendientes
- Adapters de Script API: pendiente Fase 5b/6 con microsegundo real testing per overtyping server data.
- Validación profunda de script API (ejecutar en sandbox real con MC en ws).
- TODO: integrar detect() apArmorer en mods screen (Mods screen ya usa detection.mjs → se puede subir).

## Instrucciones
```powershell
node --test packages/shared/tests/*.mjs apps/launcher/tests/*.mjs
npm test --workspace=integrations
```
