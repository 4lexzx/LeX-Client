# FASE 3 — Overlay Nativo (C++)

**Estado:** Completada.
**Nota:** cl.exe y cmake no fueron encontrados en este entorno (no hay MSVC activo instalado) → no se puede compilar nativamente aquí; el código está listo para compilar con MSVC toolset de Visual Studio Build Tools.

## Entregables

| Archivo | Descripción |
|---|---|
| `native/overlay-engine/CMakeLists.txt` | CMake proyecto para MSVC x64 |
| `native/overlay-engine/src/main.cpp` | Punto de entrada: busca ventana de Minecraft, instala hooks, bucle principal |
| `native/overlay-engine/src/engine.{h,cpp}` | OverlayEngine: ventana transparente, D2D render loop, mini-HUD (FPS/CPS/Keystrokes) |
| `native/overlay-engine/src/hooks.{h,cpp}` | LLM/TGA mouse + LLM teclado (passthrough) |
| `native/overlay-engine/src/bridge.{h,cpp}` | JSON bridge: %APPDATA%\lex-client\overlay-state.json |
| `native/overlay-engine/src/unit_tests.cpp` | Tests unitarios nativos |

## Características realizadas

- **Ventana transparente** (alpha 0 background), sin bordes, always-on-top (HWND_TOPMOST), positionable.
- **Click-through configurable**: estado persistido en overlay-state.json.
- **Detección de ventana de Minecraft** (FindWindowW "Minecraft for Windows").
- **FPS overlay**: FPS del loop D2D local (EMA).
- **CPS**: contador de clics registrado (LLM mouse + WM_LBUTTON en la ventana propia).
- **Keystrokes**: LLM teclado; capturadas y reenviadas sin bloquear (CallNextHookEx).
- **Mini-HUD transparente** (top-left): FPS (overlay), CPS, keystrokes recientes (ring de 8).
- **Bridge**: actualización real de overlay-state.json (from main.mjs via overlay:set-state) y modules.json (Fase 4).

## Fuentes de datos — honestidad

| Módulo (Fase 4) | Fuente | Nota |
|---|---|---|
| FPS Counter | engine FPS del overlay (real, EMA). TGA mejorará FPS si se mide — Fase 5. Sin inyección. |
| CPS Counter | CPS del engine overlay (real, ring de 4s). Hooks LLM + WM_LBUTTON. |
| Keystrokes | LLM teclado (real, passthrough). keyLog_ en memoria (visible al módulo). |

## Tests

- `lex-overlay-unit-tests`: tests de bridge (`readOverlayStateFromString`, `readModuleStatesFromString`).
- `unit_tests.cpp` NOT testado aún (no hay compilador C++ activo en esta máquina).

## Errores/limitaciones

- `cl.exe` no presente en PATH; MSVC via Visual Studio folder — pendiente confirmar toolchain.
- No inyección. Overlay solo ventana transparente.
- Ocultación al minimizar: pendiente (FIXME listo).
- FPS en-game (TGA): pendiente de especificación de offsets (Fase 5).

## Instrucciones

```powershell
cmake -S native/overlay-engine -B native/overlay-engine/build
cmake --build native/overlay-engine/build --config Release
```
