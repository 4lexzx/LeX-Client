# Fase 2 — Hook DXGI + ImGui + ClickGUI base

Estado: implementada y verificada. Rama: `cpp-client`.

## Alcance

- Hook de `Present`/`ResizeBuffers` por **vtable** de `IDXGISwapChain`, sin
  signature scanning ni offsets del juego.
- Backend ImGui para **D3D11 y D3D12**, con deteccion automatica del backend
  segun el dispositivo del swapchain.
- ClickGUI funcional: categorias, tarjetas, iconos propios embebidos, toggles,
  busqueda, favoritos, panel de ajustes por tipo de Setting, tooltips, tema,
  animaciones y atajo RShift.
- **Sin motion blur ni ningun post-procesado de blur.** El fondo se oscurece
  con un rectangulo alpha (`dimBackground`); se puede desactivar en Ajustes.

`core/` sigue sin incluir Windows.h, DirectX ni ImGui.

## Evidencia de los indices de vtable

Los indices **no se inventaron**: se leyeron del header real de DXGI presente
en el toolchain (`x86_64-w64-mingw32/include/dxgi.h`). Orden de la vtable de
`IDXGISwapChain`:

```
0 QueryInterface  1 AddRef  2 Release
3 SetPrivateData  4 SetPrivateDataInterface  5 GetPrivateData  6 GetParent
7 GetDevice
8 Present  9 GetBuffer  10 SetFullscreenState  11 GetFullscreenState
12 GetDesc  13 ResizeBuffers  14 ResizeTarget  15 GetContainingOutput
16 GetFrameStatistics  17 GetLastPresentCount
```

`DxgiHook::kPresentIndex = 8`, `kResizeBuffersIndex = 13`. El test
`lex-hook-tests` verifica ademas que **dos swapchains distintos** pasan por el
mismo detour (vtable compartida de `dxgi.dll`).

Hallic observado: `QueryInterface` a `IDXGISwapChain1/2/3/4` devuelve OK, pero
las cinco interfaces comparten **un unico puntero de vtable** en esta
implementacion de DXGI. Por eso basta con enganchar una vtable y eso cubre
cualquier interface usada por el juego. El log de instalacion lo confirma.

## Archivos nuevos

### Nucleo portable
- `core/gui/GuiTheme.{h,cpp}` — acento, modo oscuro/claro, opacidad, blur, animaciones, velocidad, tecla de menu. JSON.
- `core/gui/ClickGuiModel.{h,cpp}` — estado open/close con animacion, tabs, busqueda, favoritos, seleccion de modulo, tooltips, reset y persistencia.

### Windows
- `platform/windows/hooks/HookSink.h` — interfaz entre el hook y el render.
- `platform/windows/hooks/VtableHook.{h,cpp}` — swap de entrada de vtable con `VirtualProtect` (no parchea codigo).
- `platform/windows/hooks/DxgiHook.{h,cpp}` — device/swapchain dummy para obtener la vtable compartida; detours de Present y ResizeBuffers.
- `platform/windows/gui/VectorIcons.{h,cpp}` — iconos vectoriales de respaldo dibujados con `ImDrawList` (se usan en tests headless sin GPU).
- `platform/windows/gui/IconAtlas.{h,cpp}` — carga los 28 iconos embebidos y los sube a GPU en DX11/DX12.
- `platform/windows/resources/{IconResource.h,icons.rc.in}` — IDs RCDATA y plantilla del recurso.
- `scripts/make-icons.mjs` — genera 28 iconos lineales originales en 32/64 PNG, RGBA embebido y `assets/icons/preview.png`. No copia assets de otros clientes.
- `platform/windows/gui/SettingEditor.{h,cpp}` — `SettingVisitor` que dibuja bool/int/float/color/enum/keybind con ImGui.
- `platform/windows/gui/ClickGuiRenderer.{h,cpp}` — sidebar, tarjetas, interruptor, gear, favoritos, busqueda, panel de ajustes, panel de tema, tooltips, fade/slide.
- `platform/windows/gui/ImGuiLayer.{h,cpp}` — init/shutdown ImGui, WndProc forwarding, deteccion DX11/DX12, captura de teclas, cursor, render y restauracion de render targets.

### Modificados
- `CMakeLists.txt` — ImGui 1.91.9 por FetchContent con hash; target `lex-imgui`.
- `core/CMakeLists.txt`, `platform/windows/CMakeLists.txt`, `tests/CMakeLists.txt`.
- `platform/windows/dllmain.cpp` — arranca/para el hook, carga los iconos, guarda el modelo en `config/root["clickgui"]`, exports nuevos: `lexToggleGui`, `lexGuiOpen`, `lexHookPresentCalls`, `lexIconCount`; version 0.2.0.

### Tests y docs
- `tests/hook_tests_main.cpp` — 18 comprobaciones: render headless del ClickGUI, intercepcion en dos swapchains, ResizeBuffers, restauracion tras shutdown.
- `docs/phase2-cpp-clickgui.md`.

## Compilacion

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel 4
ctest --test-dir build-mingw --output-on-failure
```

Dependencias por FetchContent (con hash fijado): nlohmann/json 3.11.3 e ImGui 1.91.9.
Librerias del sistema: `user32`, `dwmapi`, `d3d11`, `d3d12`, `d3dcompiler`, `dxgi`, `dxguid`.

## Como probarlo

1. `ctest`: 2/2 tests, `Fase 1: 71 comprobaciones, 0 fallos` y
   `Fase 2 hooks: 18 comprobaciones, 0 fallos`.
2. El test de hooks crea una ventana oculta, dos swapchains D3D11 y comprueba
   que `Present` se intercepta en ambas y que tras `shutdown()` deja de hacerlo.
3. El test de render crea un contexto ImGui 1280x720, abre el modelo, llama a
   `ClickGuiRenderer::draw` y verifica que se generan command lists y vertices.
4. DLL: `lexVersion() = "0.2.0"`, `lexToggleGui()` alterna `lexGuiOpen()`.

## Honestidad tecnica / limitaciones actuales

1. **Sin prueba dentro de Minecraft Bedrock.** El hook y el render se validan
   con swapchains propias. La inyeccion y el comportamiento real in-game son
   Fase 7 (launcher/injector). No afirmo que ya funcione dentro del juego.
2. **Blur:** NO se añade motion blur ni post-procesado de blur. Solo existe la
   opcion "Oscurecer fondo" (`dimBackground`), un rectangulo alpha.
3. **Iconos:** 28 iconos originales generados por `scripts/make-icons.mjs`
   (linea blanca sobre transparente), guardados como PNG 32/64 en `assets/icons/`
   y embebidos como RCDATA en la DLL. Se suben como texturas ImGui en DX11 y
   DX12; si no hay GPU (tests headless) se cae a los iconos vectoriales.
   `assets/icons/preview.png` muestra la serie completa.
   Regenerar: `node scripts/make-icons.mjs` (Node solo en desarrollo, no en build).
4. **D3D12:** code path implementado y compilado, pero no validado en una
   swapchain D3D12 real aqui. El detector se basa en `GetDevice` + QI.
5. **Cuadricula/scroll del HUD editor** y las tarjetas con muchos modulos
   necesitaran scroll real en Fase 3/5; hoy hay pocos modulos y se dibujan en
   una rejilla fija.
6. No se han tocado firmas, offsets ni nombres internos del motor del juego.

## Siguiente fase

Fase 3: editor de HUD + modulos FPS, CPS, Coordenadas y Keystrokes. Aqui
aparecera el primer dato dependiente de ingenieria inversa (coordenadas); no se
inventara ningun valor.
