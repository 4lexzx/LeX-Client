# Rediseño del Mod Menu — LEX CLIENT

Estado: implementado. Fecha: 23-nov-2026.

## Objetivo

Sustituir la tabla genérica del apartado MODS por un **Minecraft-style Mod Menu con tarjetas ilustradas**,
equiparable en presentación a los menús de clientes Java (Lunar, Badlion, LabyMod, Flarial).
Se estudia el concepto visual de esos clientes; **no se copia ningún recurso propietario**.

## Familia gráfica original

- `apps/launcher/src/components/ModuleArt.tsx`: 18 ilustraciones SVG originales en rejilla de 64×64,
  pixel-art con paleta limitada (tinta, acero, plata, oro, verde, rojo) y `shapeRendering="crispEdges"`.
  Cada módulo tiene una ilustración temática propia: monitor FPS, ratón gaming CPS, teclado WASD,
  reloj, barras de ping, chip RAM, brújula, crosshair, bloque con estelas, niebla, paleta, lupa,
  calavera, engranaje TPS, bota alada, keycaps, reloj de arena y cámara.
- `apps/launcher/src/components/InterfaceIcons.tsx`: iconos de control y categorías dibujados a mano
  (16×16), misma familia que las ilustraciones. Sin Lucide/Tabler y sin emojis como iconos definitivos.
  El sidebar del launcher también usa esta familia.

## Diseño de la tarjeta

- Ilustración grande en la parte superior con fondo de rejilla de píxel y degradado por categoría.
- Nombre del módulo + engranaje de opciones.
- Descripción corta (la descripción completa del catálogo se conserva en el modal).
- Pie con etiqueta de categoría canónica, indicador de fuente e interruptor ON/OFF estilo Minecraft.
- Favoritos (estrella), etiqueta "PRÓXIMAMENTE" en módulos reservados, borde luminoso si está activo.
- Esquinas redondeadas, paneles biselados, elevación al hover y entrada escalonada animada
  (`animation-delay` por tarjeta, respetando `prefers-reduced-motion`).

## Organización y controles

- Categorías UI: **All Mods, HUD, PvP, Visual, Performance, Utility, Cosmetics**.
  Un módulo puede pertenecer a varias (p. ej. CPS aparece en HUD y PvP). La categoría canónica
  del `ModuleMeta.category` sigue mostrándose en la tarjeta y en el modal.
- Búsqueda por nombre, descripción o ID.
- Filtro por fuente de datos y chips de estado: Todos / Activados / Desactivados / Favoritos.
- Contador de módulos activos sobre el total, con barra de progreso.
- Vista de tarjetas y vista compacta (filas), ambas con la misma información esencial.
- Modal de opciones desde el engranaje: descripción completa, ficha técnica del módulo,
  nota honesta sobre su fuente de datos, favorito, copiar ID e interruptor.
- Favoritos persistidos en `localStorage` (`lex-client:mod-favorites`).

## Conexión con el sistema real

- Los interruptores siguen escribiendo en `window.lex.ipc.modulesSetState({ id, enabled })`
  → `modules:setState` → `%APPDATA%\lex-client\modules.json` (sin cambios de contrato IPC).
- El estado inicial se lee con `modulesGet()` y el catálogo con `catalog()`.
- Si falla la escritura IPC se revierte el cambio optimista y se muestra un error en la barra de filtros.
- No se simulan datos de juego: los módulos `reserved` muestran su etiqueta y la nota
  "Esperando fuente nativa fiable".

## Verificación

- `npm run typecheck --workspace=apps/launcher` — sin errores.
- `npm run test:all` — 21/21 pruebas.
- `npm run build --workspace=apps/launcher` — build correcto.
- Comprobación funcional en renderer con IPC simulado: filtros, búsqueda, categorías, favoritos,
  vista compacta, contador activo, interruptor y modal (apertura y cierre con Escape).
