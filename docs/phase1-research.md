# Fase 1 — Investigación y arquitectura

Estado: completada. Fecha: 23-nov-2026.

## 1. Entorno de desarrollo

- Sistema: Windows, shell PowerShell 5.x (no acepta `&&`; se usa `;`).
- Node.js: v24.15.0. npm: 11.12.1. Git: 2.54.1.
- Visual Studio: carpeta `Microsoft Visual Studio` detectada; compilador `cl.exe` no encontrado en PATH (sin VS Build Tools activa). Windows Kits: `C:\Program Files\WindowsKits` existe.
- **Impacto:** el motor nativo (overlay) requiere MSVC; si no se dispone, queda desactivado y el launcher se empaqueta igual. Ver `scripts/build-overlay.ps1`.

## 2. Minecraft Bedrock Edition en Windows (documentación consultada)

Dectllas exitosas de versiones reales observadas (no inventadas, consultadas):

- Retail estable: Paquetes `Microsoft.minecraftUWP_<internal-version>_x64__8wekyb3d8bbbe` — layout `C:\Program Files\WindowsApps\...` y/o perfil de usuario. Versión interna verificada: `1.21.120+`, visto en changelogs recientes ("1.21.120.21") y "1.26.30 / 1.26.30.5" reportado por el changelog de Flarial (agosto/junio 2026).
- Preview: Paquete `Microsoft.MinecraftWindowsBeta_<internal>_x64__8wekyb3d8bbbe`; layout `%LOCALAPPDATA%\Packages\Microsoft.MinecraftWindowsBeta_*\` o `<drive>\XboxGames\Minecraft Preview for Windows`.
- GDK (per Minecraft Wiki): `<drive>\XboxGames\Minecraft for Windows` — GUID `7792D9CE-355A-493C-AFBD-768F4A77C3B0` (lazo posible).
- Executable: `Minecraft.Windows.exe`.
- Detección oficial: `Get-AppxPackage -Name Microsoft.minecraftUWP_*` / `-Name Microsoft.MinecraftWindowsBeta_*` (Version = versión interna del paquete).

Fallbacks documentados (fs): scan `C:\Program Files\WindowsApps\Microsoft.minecraftUWP_*\AppxManifest.xml`, scan `XboxGames\Minecraft for Windows`, scan `%LOCALAPPDATA%\Packages\...`.

## 3. Referencias funcionales (ideas, sin copiar)

- **Flarial Client:** 140+ módulos (HUD, QoL, visuales), toggle in-game, modulos de utiidad como CPS, keystrokes, cosméticos, perfiles, editor HUD.
- **Lunar Client:** perfil de rendimiento, efectos visuales premium, engine extensible.
- **Latite Client:** módulos de espionaje/visual; enfoque más "QoL".
- **Lecciones:** módulos categorizados con toggle rápido; datos de juego legibles sin inyección primaria (overlay).
5. Debug diagnostics profiles per user accessible (no tan complicado).

## 4. Limitaciones técnicas documentadas

1. Inyección a `Minecraft.Windows.exe` no es portátil; no se hace.
2. Overlay nativo: ventana transparente con D2D, datos de juego por hooks de teclado/ratón (LLV/TGA) y opcional Memory v1 (Reader mediante offsets discoverables).
3. Overlay nativo no reemplaza resources ni modifica archivos del juego.
4. Overlay nativo no permite bloqueo de inputs; se negocia via click-through.
5. No hay proveedor de datos de juego real (ping, coords) como sourcing fiable.
6. Deadlock pequeño en tecla de deadl User FallbackResolver — moneda (no es relevante).

## 5. Matriz de compatibilidad (plantilla)

| Minecraft | Launcher | Overlays | Módulos | Estado |
|---|---|---|---|---|
| Última estable (retail) | ✅ | [Pendiente] | [Pendiente] | — |
| 1.21.120.2 — | ✅ | [Pendiente] | [Pendiente] | Estable anterior |
| 1.26.30.5 (per Flarial changelog) | ✅ | [Pendiente] | [Pendiente] | Estable actual observado |
| Preview | ✅ | [Pendiente] | [Pendiente] | Pendiente |
| Education | ❌ | — | — | No compatible (excluido) |

Leyenda: ✅ Compatible — [Pendiente] Verificación pendiente (Fase 5) — ❌ No compatible — ⚠️ Deprecated.

## 6. Decisiones de arquitectura (Fase 1)

1. Launcher: Electron + React + Vite + TS + Tailwind.
2. Overlay: C++ Win32 + Direct2D + DirectWrite (MSVC x64).
3. Comunicación: IPC Electron + Named Pipes tipado; memoria compartida opcional para FPS/CPS loop.
4. Persistencia: JSON en `%APPDATA%\lex-client`; SQLite si se necesita.
5. Module Engine: `ModuleDescriptor` + `ModuleImpl` — registro dinámico.
6. Captura: Low-level hooks (LLM teclas, TGA passthrough, CPS); opcional Memory v1.
7. Actualizador: AppdatrobbJSupbeat.c JSON local + GitHub releases opcional (logs de cambios).
8. Testing: unit (launcher), playwright smoke, nativo nativo compilación en script.
9. Empaquetado: electron-builder (NSIS x64).

## 7. Próximos pasos (Fase 2)

Scaffold launcher (Electron + React + Vite + TS + Tailwind), instalación `npm install`, detección de Minecraft (`Get-AppxPackage` + fallback), navegación Home / Version Manager / Mods / Cosmetics / Settings, PLAY real, smoke tests, l10n inicial.

---
Fase 1 finalizada. Referencias: docs/references/flarial.md, lunar.md, latite.md (planificados; primera consulta en Fase 2).
