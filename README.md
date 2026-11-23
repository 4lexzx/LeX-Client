# LEX CLIENT

> Cliente avanzado independiente para Minecraft Bedrock Edition en Windows 10/11. Escrito desde cero.

**No es un launcher que simula ser un cliente. Es un cliente funcional: launcher, overlays, módulos y cosméticos con arquitectura propia.**

- **Identidad propia:** tecnología y diseño únicos, inspirados en clientes modernos como Lunar/Flarial pero sin copiar su código, herramientas ni recursos gráficos.
- **Reconocerás usar:** Node.js / TypeScript / Electron / React / Vite / Tailwind CSS para el launcher.
- **Nativo:** Windows API + Direct2D para el motor de overlays externos.
- **Seguro:** nunca aparca ejecutables de terceros, no modifica archivos del juego, no almacena credenciales.

## Estado del proyecto

- [x] Fase 1 — Investigación, arquitectura, estructura (EN PROGRESO → FINALIZADA)
- [x] Fase 2 — Launcher funcional (Detección, Inicio, navegación real)
- [x] Fase 3 — Motor de overlays nativo (Win32/D2D)
- [x] Fase 4 — Sistema de módulos (SDK, registro, activación)
- [x] Fase 5 — Integración con Bedrock (Versiones, Adaptadores)
- [x] Fase 6 — Cosméticos
- [x] Fase 7 — Instalador y distribución

Consulta `docs/` para los entregables de cada fase y el progreso detallado.

## Requisitos

- Windows 10/11 (64 bits)
- Node.js 20+ (para desarrollo)
- Visual Studio Build Tools con MSVC (para el motor nativo) — opcional

## Instrucciones de ejecución

```powershell
# Launcher (desarrollo)
cd lex-client
npm install
cd apps/launcher
npm run dev

# Pruebas
npm test
```

Ejecutables nativos: `native/overlay-engine` se compila con MSVC (`scripts/build-overlay.ps1`).

## Licencia

MIT (para LEX CLIENT; no está vinculado a Mojang/Microsoft y no se redistribuye código de Mojang).
