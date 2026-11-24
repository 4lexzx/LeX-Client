# Fase 1 — CMake + nucleo portable + IPlatform + modulo de prueba

Estado: implementada y verificada. Rama: `cpp-client`.

## Alcance

Esta fase **no** inyecta en el juego, **no** hace hooks y **no** dibuja nada en
pantalla. Construye las bases que las siguientes fases reutilizan sin cambios:

- `core/` sin una sola linea de `Windows.h`, DirectX ni ImGui.
- Una DLL `LeXClient.dll` x64 con un ciclo de vida exportado.
- Un ejecutable de tests que valida 51 comprobaciones del nucleo.

La implementacion Electron/TypeScript preexistente (`apps/`, `packages/`,
`native/`, `modules/*.ts`) queda intacta en `main`. Esta rama convive con ella
sin modificarla.

## Archivos creados

### Build
- `CMakeLists.txt` — proyecto C++20, x64 obligatorio, FetchContent de nlohmann/json 3.11.3 con hash SHA256.
- `core/CMakeLists.txt` — libreria estatica `lex-core`.
- `platform/windows/CMakeLists.txt` — libreria estatica `lex-platform-windows`.
- `modules/CMakeLists.txt` — OBJECT library `lex-modules-objects` (autorregistro fiable).
- `tests/CMakeLists.txt` — `lex-core-tests` + `add_test`.

### Nucleo portable (`core/`)
- `platform/IPlatform.h` — frontera unica con la plataforma (Vec2, Rect, KeyState, PlayerSnapshot, primitivos de dibujo).
- `events/Events.h` — TickEvent, RenderEvent, KeyEvent, MouseEvent, WorldJoinEvent, ShutdownEvent.
- `events/EventBus.{h,cpp}` — bus tipado con `Subscription` RAII, snapshot antes de publicar, cancelacion.
- `settings/Types.{h,cpp}` — `Color` (ARGB) y `Keybind` (codigo portable + modificadores) con JSON.
- `settings/Setting.h` — `SettingBase` + `Setting<SettingType, T>` y los 6 alias concretos.
- `settings/SettingGroup.h` — grupo con `add<...>()`, `find()`, `addOwned()`.
- `settings/SettingVisitor.{h,cpp}` — dispatch sin RTTI para futuro ClickGUI.
- `module/Category.h` — HUD, Visual, Utilidad, Cosmeticos, Ajustes + ids de icono.
- `module/Module.{h,cpp}` — metadatos, enabled, keybind, settings y 8 hooks virtuales. Se suscribe al bus **solo mientras esta activo**.
- `module/ModuleManager.{h,cpp}` — singleton, macro `LEX_REGISTER_MODULE`, save/load de config.
- `config/ConfigManager.{h,cpp}` — perfiles JSON, autoguardado, `findModule`.
- `util/Logger.{h,cpp}`, `util/Math.h`, `util/Strings.h` — portable.
- `ClientContext.{h,cpp}` — compone plataforma + bus + config + manager.

### Capa Windows (`platform/windows/`)
- `Exports.h` — macro `LEX_API`.
- `WindowsPlatform.{h,cpp}` — implementacion concreta de IPlatform. `GetSystemMetrics`, `GetAsyncKeyState`, `ShowCursor` y FPS por rolling average. Dibujo = no-op (Fase 2).
- `dllmain.cpp` — exports C: `lexInitialize`, `lexTick`, `lexShutdown`, `lexModuleCount`, `lexSetModuleEnabled`, `lexVersion`, `lexFps`.

### Modulo y tests
- `modules/test/TestModule.{h,cpp}` — los 6 tipos de Setting y ciclo de vida. Se registra solo con la macro.
- `tests/NullPlatform.{h,cpp}`, `tests/test_main.cpp` — 51 comprobaciones.
- `platform/android/README.md`, `core/gui/README.md`, `assets/README.md`, `launcher/README.md` — marcadores de fases futuras.

## Compilacion

Workspace verificado: Windows x64, CMake 4.4.4, MinGW-w64 GCC 16.2.0. MSVC
no estaba en PATH en este entorno, así que el CMake Detecta MSVC si existe y
usa MinGW si no.

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel 4
ctest --test-dir build-mingw --output-on-failure
```

Con Visual Studio 2022:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release --parallel
ctest --test-dir build-vs -C Release --output-on-failure
```

Salida: `build-mingw/LeXClient.dll` (o `build-vs/Release/LeXClient.dll`).

## Como probarlo

1. `ctest` debe dar `100% tests passed` y el ejecutable imprimir
   `Fase 1: 51 comprobaciones, 0 fallos.`
2. Listar exports:
   ```powershell
   objdump -p .\build-mingw\LeXClient.dll | Select-String 'lex'
   ```
   Deben aparecer `lexInitialize`, `lexTick`, `lexShutdown`, `lexModuleCount`,
   `lexSetModuleEnabled`, `lexVersion`, `lexFps`.
3. Smoke test de la DLL cargandola con P/Invoke desde PowerShell: llamar a
   `lexVersion()` → `"0.1.0"`, `lexModuleCount()` → `1`,
   `lexInitialize(tempDir, "default")` → `true`, `lexTick(0.016f)` y
   `lexShutdown()`. Debe generarse `<tempDir>/default.json`.

## Honestidad tecnica — TODO con datos que necesito de ti

1. **Datos del jugador** (`WindowsPlatform::playerSnapshot`): devuelve un
   snapshot vacio. Para coords, rotacion, bioma, ping y "en mundo" necesito de
   una version concreta del juego: el patron de bytes de la funcion que lee la
   camara/posicion del jugador (o el offset del objeto Player dentro del Level
   del modulo), y el punto de tick del world para `OnWorldJoin`. No inventar
   firmas: dime la version exacta (por ejemplo 1.21.120.x) y con que herramienta
   quieres obtenerlas (x64dbg, IDA, o un escaneo en runtime comparando valores).
2. **Hooks DX11/DX12 e ImGui**: Fase 2. Necesito confirmar si quieres que el
   hook se haga sobre `Present`/`ResizeBuffers` de vtable (lo recomendable) o
   sobre funciones de `d3d11.dll`/`d3d12.dll` por signature scanning.
3. **Firma de render del HUD del juego**: aun no hay ninguna. No se inventa
   ningun offset en este proyecto.
4. **Cosmeticos**: Fase 6. Antes de escribir codigo analizare que enfoque es
   viable con el sistema de skins/geometria de Bedrock y te dire que datos de
   ingenieria inversa hacen falta.

## Siguiente fase

Fase 2: hook DX11/DX12 + ImGui + ClickGUI base con categorias, tarjetas,
iconos y toggles. Sin tocar `core/`.
