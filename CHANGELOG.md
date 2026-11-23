# Changelog — LEX CLIENT

## [0.2.0] — Integración con Bedrock, Cosméticos, Verificación (Fase 5, Fase 6)

### Added
- Integraciones: `integrations/bedrock/src/detect.ts` (Get-AppxPackage, PS), `integrations/resource-packs/src/index.ts` (scanner real), `integrations/script-api/types.ts` (esquemas reservados Fase 5b).
- Cosméticos: `cosmetics/{types,gallery,importer,index}.ts` + README, visor 3D real en `apps/launcher/src/screens/Cosmetics.tsx` (canvas 2D, camino real), importador real (validación schema + resultados reales).


## [0.3.0] — Integración con Bedrock, Cosméticos, Verificación (Fase 5, Fase 6)

### Added
- Integraciones: integrations/bedrock/src/detect.ts (Get-AppxPackage, PS), integrations/resource-packs/src/index.ts (scanner real), integrations/script-api/types.ts (esquemas reservados Fase 5b).
- Cosméticos: cosmetics/{types,gallery,importer,index}.ts + cosmetics/README.md, visor 3D real en pps/launcher/src/screens/Cosmetics.tsx (canvas 2D, animado), importador real (validación schema + resultados reales).

## [0.1.0] — Launcher funcional (Fase 1 + Fase 2 completadas)

### Added
- Estructura del proyecto: apps/launcher, packages/*, native/overlay-engine, integrations/*, modules/*, cosmetics/, docs/.
- Launcher Electron + React + Vite + TypeScript + Tailwind: detección real de Minecraft Bedrock, navegación real (Home, Version Manager, Mods, Cosmetics, Settings), play real, diagnostics, perfiles.
- Detección de versiones retail/preview/education (Get-AppxPackage + filesystem fallback).
- Documentación: ARCHITECTURE.md, README.md, docs/phase1-research.md, docs/phase2-launcher.md (Fase 2 finalizada).

---

## [0.0.1] — Investigación y arquitectura (Fase 1)

### Added
- Investigación de Minecraft Bedrock detection paths (retail, preview, education).
- Arquitectura propia: Electron + React (launcher), C++ Win32 + Direct2D (overlay).
- Matriz de compatibilidad base.
