# Sistema propio de cosméticos (LEX CLIENT)

Diferenciación de cosméticos:

- **Cosméticos locales:** representaciones propias de nuestro sistema, visibles solo en el cliente del usuario individual. Alta integración interna (no requiere nada fuera del cliente).
- **Cosméticos aplicables por otros:** cosméticos que otros jugadores verían (ej: skin en mallas compatibles). Distribución real: resource packs redistribuidos (publicados en Minecraft Marketplace / oficial). Requiere instalación en el cliente del otro usuario — LEX CLIENT **no modifica ni redistribuye archivos del juego**.
- **Cosméticos importados:** propios definidos por el usuario (schema validado, copia local segura).

## Datos de galería

- `cosmetics/types.ts` — los datos (Cosmetic, Gallery, ImportValidationIssue).
- `cosmetics/gallery.ts` — galería local (sampleGallery).
- `cosmetics/importer.ts` — importación + validación schema (sin inventar).
- `cosmetics/index.ts` — índice y privacy rule.
- Visor 3D: `apps/launcher/src/screens/Cosmetics.tsx` (UI de usuario con view real).

## Reglas (verificables)

1. **Nunca alterar cuentas ni sistemas de auth de Minecraft.**
2. **Nunca aparcar cosméticos de otros en servidores sin rastro.**
3. **Validar schema antes de importar** (importer.ts).
4. **Copia de seguridad antes de reemplazar config (Fase 7 — péndente).**
5. **Cosmetic personalization local: solo en cliente del usuario.**

## limitaciones/pendientes

- Visor 3D (CSS 3D simple) — Fase 6.
- Importador de archivos (Fase 6 — con file dialog real).
- Sync de cosméticos (Fase 7).
