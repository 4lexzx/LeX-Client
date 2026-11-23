# Módulos de LEX CLIENT

Sistema de módulos modulares. Cada módulo tiene:
- `meta: ModuleMeta` — nombre, descripción, categoría, dataSource, overlayRenderer.
- `Module` — activate/deactivate.

`dataSource` (fuente honesta, no inventamos datos):
- `external-overlay` — datos del engine de overlays nativo (Fase 3).
- `game-memory-v1` — datos leídos de memoria del juego (adaptadores, Fase 5, pendiente validación real).
- `game-memory-v2` — versión 2 del adapter.
- `script-api-v1` — datos via Script API (per BoxingExecute unroll, pendiente Fase 5).
- `local-only` — datos del cliente/máquina.
- `reserved` — pendiente fuente fiable.

Pendientes: implementación de módulos asociados a estas fuentes (próxima fase).
