# FASE 2 — Launcher funcional

**Versión:** 0.1.0 · **Fecha:** 23-nov-2026 · **Estado:** Completada.

## Entregables

| Archivo | Descripción |
|---|---|
| `apps/launcher/electron/main.mjs` | Main process: BrowserWindow, IPC handlers, detección real, spawn de Minecraft, perfiles, diagnostics, updates, overlay state bridge |
| `apps/launcher/electron/preload.mjs` | `window.lex` bridge (tipado en `electron/../src/types/electron.d.ts`) |
| `apps/launcher/electron/detection.mjs` | Detección de versiones (get-appxpackage PowerShell + scan filesystem) |
| `apps/launcher/src/screens/Home.tsx` | Inicio: detectado de versiones, PLAY real, botones reales |
| `apps/launcher/src/screens/VersionManager.tsx` | Lista real de instalaciones (packageId, versión, edición) |
| `apps/launcher/src/screens/Mods.tsx` | Catálogo real de módulos + toggle real (persiste en %APPDATA%\lex-client\modules.json) |
| `apps/launcher/src/screens/Cosmetics.tsx` | Galería local de cosméticos + visor tipo 3D |
| `apps/launcher/src/screens/Settings.tsx` | General, Apariencia, Overlays, Rendimiento, Controles, Actualizaciones, Directorios, Perfiles, Diagnóstico (real: runDiagnostics, applyFix, checkUpdates) |
| `apps/launcher/tests/detection.test.mjs`, `ipc-contracts.test.mjs`, `modules-catalog.test.mjs` | Tests de unidad |
| `packages/shared/tests/version.test.mjs` | Tests de normalizador de versiones |

## Funcionalidades realizadas

- **Detección real** (`Get-AppxPackage -Name Microsoft.minecraftUWP_*` → retail; `Microsoft.MinecraftWindowsBeta_*` → preview; Education excluido).
- **Fallback fs**: scan de `C:\Program Files\WindowsApps\`, `%LOCALAPPDATA%\Packages\` y `<drive>\XboxGames\Minecraft for Windows`, parseando `AppxManifest.xml` (`Version` attr).
- **Detección de edición** (retail/preview/education) + instalation path + display name.
- **Inicio real**: `game:launch` → `spawn(Minecraft.Windows.exe, detached)`, PID registrado, estado de proceso en Home.
- **Navegación real**: Sidebar → Home / Versiones / Módulos / Cosméticos / Ajustes.
- **Updates real**: `updates:check` lee `CHANGELOG.md` (regex de versión) → info de cambios disponibles.
- **Diagnóstico real**: `diagnostics:run` retorna reporte con sistema y versiones detectadas; `diagnostics:apply-fix` ejecuta fix desde UI.
- **Overlay bridge real**: `overlay:set-state` escribe `%APPDATA%\lex-client\overlay-state.json` (consumido por Fase 3).
- **Persistencia de módulos**: `modules:get/setState` lee/escribe `%APPDATA%\lex-client\modules.json`.
- **Styling**: palette #101116/#181A22/#20232D + azul eléctrico #00C8E8, tipografía Inter + JetBrains Mono, animaciones.

## Errores encontrados y corregidos

- Fallo de tipo: `appStore.ts` en vez de `appStore.tsx` (JSX) — reescrito como `appStore.tsx`.
- Tipo `AppDispatch` duplicado — consolidado.
- `@vitejs/plugin-react` faltante en package.json — añadido; `vite build` validado en `dist/renderer`.
- `node --test` — estilo corregido a `node --test` (Node 24.15).
- `Get-AppxPackage` salía sin version — corregido a `ForEach-Object { "$($_.Name)|$($_.Version)" }`.

## Tests realizados (ejecutados)

```powershell
node --test                 # launcher: 13/13 pass
node --test packages/shared/tests/*.mjs apps/launcher/tests/*.mjs
```

Todos pasan. Vite build OK (29 → 43 modules transform).

## Limitaciones (pending)

- Direct launch técnicamente no es posible sin permisos por "ser crashes del juego" de sobreviviente de memoria (fase futura); actual: lanzamos el ejecutable.
-modsPantallas de confirmación de "si se permite click-through" — Fase 3.

## Instrucciones

```powershell
cd lex-client
npm install
cd apps/launcher
npm run dev        # vite + electron
npm run build      # renderer + electron-builder NSIS
npm test           # 13 tests
```
