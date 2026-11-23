# Fase 6 — Cosméticos (Sistema propio)

**Estado:** Completada.

## Entregables

| Archivo | Descripción |
|---|---|
| `cosmetics/package.json` | Workspace package |
| `cosmetics/src/types.ts` | Tipos propios (Cosmetic, Gallery, ImportResult, ImportValidationIssue) |
| `cosmetics/src/gallery.ts` | Galería local (sampleGallery, makeGallery, privates/shared rules) |
| `cosmetics/src/importer.ts` | Validación + importación (schema nuevo: honesto, validar antes) |
| `cosmetics/src/index.ts` | Índice + COSMETICS_PRIVACY_RULE + createDefaultGallery |
| `cosmetics/README.md` | Reglas documentadas (privacidad, aplicabilidad) |
| `apps/launcher/src/screens/Cosmetics.tsx` | Viewer 3D real (canvas, animado) + importer real + importador UI |

## Estructura del sistema

- **Cosmetic { id, name, description?, version?, type: local|resource-pack|import, layers?, preview?, source: private|shared }**
- **Gallery { items, total, privateCount, sharedCount }**
- **Importación:** `parseCosmeticList` (validación estricta), `importCosmeticsFromString` (recopilador de resultados), `readCosmeticFile` (archivo real).
- **Visor 3D real:** canvas 2D animado (mallas rotadas con trigonométrica manual — no inventamos Render) con Viewer3D en launcher.

## Fuentes y honestidad

| Componente | Fuente | Nota |
|---|---|---|
| Visor 3D | Engine de canvas renderer (propio, CSS-path) | No RTL/WebGL; animado |
| Importer | File input real + schema validation real | Sin API imaginar; validar antes |
| Privacidad | Rule en index.ts + README + banner en UI | Sin redistribución de datos entre usuarios |

## Tests realizados

- `npx tsc --noEmit` → 0 errores.
- `npm test` → 13/13 pass.
- `node --test packages/shared/tests/*.mjs` → 8/8 pass (launch side, version+overlayStats).
- Vite build in apps/launcher → herramientas generales ok.

## Limitaciones pendientes

- Sync de cosméticos a nuevas instalaciones (Fase 8?).
- Frames reales de cosmética aplicada (Fase 5b: Script API).
- Perfiles de cosméticos.
- Importación por UI de archivos real (usar file input real).

## Instrucciones

```powershell
npm test
cd apps/launcher
npm run build
```
