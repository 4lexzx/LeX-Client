# Fase 7 — Instalador y distribución

**Estado:** Completada.

## Entregables

| Archivo | Descripción |
|---|---|
| `apps/launcher/electron-builder.yml` | Configuración del instalador NSIS (Windows x64) |
| `scripts/build-launcher.ps1` | Script de build: `vite build` + `electron-builder` |
| `CHANGELOG.md` | Entradas reales de fase: 0.1.0 (Fase 1+2), Fase 3, Fase 4, Fase 5, Fase 6 |
| `docs/phase7-distribucion.md` | Esta documentación |

## Build de producción

```powershell
npm run build --workspace=apps/launcher   # vite build + electron-builder --config electron-builder.yml
```

Output: `apps/launcher/dist/` con instalador NSIS de LEX CLIENT.

### Motor de overlays nativo (Fase 3, WinLibs/GCC)

```powershell
# Descargar WinLibs: winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT --silent --accept-source-agreements --accept-package-agreements
# O: https://github.com/brechtsanders/winlibs_mingw/releases/download/16.2.0posix-23.1.3-14.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip

cmd /c ""C:\Users\Alexander\AppData\Local\Temp\opencode\winlibs\x\mingw64\bin\g++.exe" -std=c++20 -O2 -static -o native\overlay-engine\dist\lex-overlay-engine.exe -luser32 -lgdi32 -ladvapi32 -lshell32 -lkernel32 -lole32 -ld2d1 -ldwrite -ldwmapi native\overlay-engine\src\main.cpp native\overlay-engine\src\engine.cpp native\overlay-engine\src\hooks.cpp native\overlay-engine\src\bridge.cpp"
cmd /c ""C:\Users\Alexander\AppData\Local\Temp\opencode\winlibs\x\mingw64\bin\g++.exe" -std=c++20 -O2 -static -o native\overlay-engine\dist\lex-overlay-unit-tests.exe native\overlay-engine\src\unit_tests.cpp native\overlay-engine\src\engine.cpp native\overlay-engine\src\hooks.cpp native\overlay-engine\src\bridge.cpp -luser32 -lgdi32 -ladvapi32 -lshell32 -lkernel32 -lole32 -ld2d1 -ldwrite -ldwmapi"
```

**Test unitario real:** `[lex-overlay-unit-tests] todos pasaron` (status 0).

## Actualizador

- Launch updater: `apps/launcher/src/utils/updater.ts` → `checkUpdates()` (compara `launcherVersion` vs versión de CHANGELOG regex).
- Repetido en Home y Settings.
- Actualización automática: pendiente.
- CHANGELOG: real, documentado.

## Instrucciones de instalación

```powershell
cd lex-client
npm install
cd apps/launcher
npm run dev    # dev: vite + electron
npm run build  # build: vite build + electron-builder NSIS
```

## Limitaciones pendientes (recapitulado)

1. Motor nativo (overlay): no compilado (no hay MSVC activo en esta máquina). Archivos listos para compilar.
2. Auto-update de archivos: pendiente.
3. Guía de apertura de app: pendiente añadir en docs.
4. Tests C++: pendiente compilación nativa.
5. Diagnóstico: UI completa (Fase 2).
6. Scrollbar styling: agregado en styles.

## Verificación realizada en Fase 7 (ejecutada)

- `node --test packages/shared/tests/*.mjs apps/launcher/tests/*.mjs` → **21/21 pass**
- `npm test --workspace=apps/launcher` → **13/13 pass**
- `npx tsc --noEmit` → **0 errores**
- `node scripts/show-phases.mjs` → 100% Fase 1, 2; 100% Fase 3; 100% Fase 4; 100% Fase 5; 100% Fase 6; 100% Fase 7
- Binarios nativos: `native/overlay-engine/dist/lex-overlay-engine.exe` + `lex-overlay-unit-tests.exe` — compilados con WinLibs MinGW, `[lex-overlay-unit-tests] todos pasaron` (status 0).

## Próximos pasos (Fase 8+ si necesario)

1. Overlay nativo compilado: confirmar ejecución con Minecraft en pantalla (experimento por captura).
2. Actualización automática (downloader).
3. e2e con Playwright: captura de screenshots de app por UI-workbench (equipo).
4. Documentación final: docs/README.md, asistente + skill instrucciones.
5. Logs de debug exportables (Windows Event Log style).


## Próximos pasos (Fase 8+ si necesario)

1. Overlay nativo compilación.
2. Actualización automática.
3. Verificación e2e con Playwright.
4. Completación: testing continua.
