# ARCHITECTURE — LEX CLIENT

## 1. Visión general

LEX CLIENT es un cliente moderno para Minecraft Bedrock Edition en Windows. El proyecto tiene su propia arquitectura de tres capas:

1. **Launcher (GUI)** — aplicación Electron + React + Vite + TypeScript + Tailwind CSS.
2. **Motor de overlays nativo** — aplicación Win32 + Direct2D + DirectWrite compilada con MSVC.
3. **Capa de integración con Bedrock** — funciones nativas (Windows API, v8 de Node para el launcher) para detección de versiones, gestión de resource packs, adaptadores Script API por versión.

## 2. Estructura del repositorio

```
lex-client/
├── apps/
│   └── launcher/                 # Aplicación Electron + React (GUI del launcher)
├── packages/
│   ├── shared/                   # Tipos y utilitarios comunes (TS estricto)
│   ├── module-sdk/               # SDK para módulos (registro, metadatos, ciclo de vida)
│   ├── ui-components/            # Componentes de UI reutilizables (React)
│   └── compatibility/            # Normalizador de versiones + matriz de compatibilidad (TS)
├── native/
│   ├── overlay-engine/           # Motor nativo de overlays (Win32 + D2D)
│   └── windows-integration/      # Integración de Windows nativa (compilado estático)
├── integrations/
│   ├── bedrock/                  # Detección, adaptadores por versión, Script API
│   ├── resource-packs/           # Gestión de resource packs
│   └── script-api/               # Adaptadores de la Script API (esquema tipado)
├── modules/
│   ├── hud/                      # Módulos HUD (FPS, CPS, keystrokes, clock, ...)
│   ├── visual/                   # Módulos visuales (crosshair, fov, ...)
│   ├── gameplay/                 # Módulos gameplay
│   └── utilities/                # Herramientas útiles
├── cosmetics/                    # Sistema propio de cosméticos
├── assets/                       # Iconos, tipografías, recursos (NUNCA modificamos a Mojang)
├── configs/                      # Plantillas de configuración JSON
├── docs/                         # Entregables por fase y documentación
├── scripts/                      # scripts de build/test (PowerShell, Node)
└── tests/                        # Test unitarios y e2e
```

## 3. Arquitectura del launcher

```
Electron (main.mjs — Node.js, proceso principal)
   └── IPC (named pipe / Electron IPC) — contratos tipados (contracts.ts)
        └── Renderer (React + Vite + Tailwind — proceso renderer)
             ├── home/
             │   ├── VersionManager.tsx    (detección de instalaciones)
             │   ├── Mods.tsx              (catálogo y activación de módulos)
             │   ├── Cosmetics.tsx         (cosméticos, importación, 3D viewer)
             │   └── Settings.tsx          (general, actualizaciones, diagnóstico)
             └── hooks/ (useStore, useDetect, useSensitiveActions)
```

Proceso principal (main.mjs):
- `bedrock:detect` → detección de instalaciones (PowerShell Get-AppxPackage + escaneo de archivos).
- `game:launch <id>` → spawn de Minecraft.Windows.exe.
- `game:resolve` → ruta de ejecución resuelta.
- `overlays:toggle`, `profiles:*`, `diagnostics:run`, `updates:check`.
- Guard: modo sin credenciales, registry en CARGO de paths configurables.
- Sin servidores externos obligatorios: el actualizador compara versiones instaladas y notifica; feeds opcionales configurables.

## 4. Overlays (Motor Nativo, C++)

```
Win32 (message loop + D2D)
  ├── Transparente (WS_EX_LAYERED, color chroma -> alphas)
  ├── Always-on-top / click-through configurable
  ├── Detección de ventana del juego (FindWindowW "Minecraft for Windows")
  ├── Render loop propio (fps del overlay)
  └── Fuentes por hooks del juego:
        ├── GlobalLow (LLM)         -> teclas / CPS
        ├── Global(TGA)             -> FPS (fallback, alternativo al overlay loop)
        └── Game memory v1          -> Coordenadas, ciclo de Minecraft (opcional)
```

No inyecta en el juego: todo lo que se expone está en ventana transparente que captura eventos (marcha de teclado/ratón). No reemplaza recursos del juego: todos los recursos son propios.

## 5. Módulos y fonts de datos — honestidad obligatoria

Cada módulo declara su `dataSource`:

```ts
type DataSource =
  | 'external-overlay'   // Los datos los emite el overlay nativo externo
  | 'game-memory-v1'     // Litura de memoria del juego (adapter versionado)
  | 'game-memory-v2'     // Versión 2 del adapter (futuro)
  | 'script-api-v1'      // Sólo Script API (World.tsv, GameEvents, ...)
  | 'local-only'         // Datos del propio cliente/overlay (cpu, memoria local)
  | 'reserved';          // Pendiente de fuente fiable (fuera de escopo por ahora)
```

Regla del proyecto (documentada y probada): **un módulo no efectúa "simulación" de datos.** Si `dataSource` es `reserved`, su render marca "Esperando fuente nativa: <categoría>" en lugar de inventar números.

## 6. Actualizaciones

El updater es independiente del juego (`apps/launcher/src/updater.ts`):
- Compara versión local con la lista de versiones publicadas en `docs/releases/*.json`.
- Muestra changelog: `docs/CHANGELOG.md`.
- Soporta actualización sin pérdida de configuraciones (cada config es JSON validado).

## 7. Seguridad

- NO almacena credenciales de Microsoft. Autenticación de la cuenta: requiere sings in en la tienda de forma oficial.
- NO elimina ni modifica archivos del juego. Soporte solo delectores.
- Validación de imports (hash + schema).
- Copia de seguridad antes de importar/reemplazar configuraciones sensibles.
- Registry de acceso por paths configurables + validación de realizadores.
- Diagnóstico público: URLs, versiones, hashes de ejecutables; sin información de cuenta.

## 8. Pruebas y estándares

- `npm test` en launcher: test unitarios de detection, normalizador de versiones, matriz de compat.
- `scripts/build-overlay.ps1`: compilación del motor con MSVC (build tools de Visual Studio).
- Tests de renderer con Playwright (hover de verificación visual; estado de navegación).

## 9. Fases de desarrollo (consulta docs/)

1. Investigación y arquitectura — [x]
2. Launcher funcional — [ ]
3. Overlay nativo — [ ]
4. Módulos — [ ]
5. Integración Bedrock — [ ]
6. Cosméticos — [ ]
7. Instalador y distribución — [ ]

## 10. Referencias

Clientes consultados como referencia funcional (para ideas de negocio de funcionalidades): Lunar Client, Flarial Client (https://floreal.net / https://floreal.xyz/changelog — a consultado es lista de módulos, Clock, CPS, HUD ejemplo), Latite Client. **Ningún código, herramienta gráfica ni recurso se copia.**
