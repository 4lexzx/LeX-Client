// LEX CLIENT — Catálogo estático de módulos (para el launcher: Mods screen).
// Versiones reales (phase 4): lista emigrada desde aquí hacia packages/module-sdk + modules/*.
// Estado: Fase 2 — metadatos del catálogo generales; datos de juego: ver `dataSource`.

/** @type {import('@lex/shared').ModuleMeta[]} */
export const MODULES_CATALOG = [
  // ----- HUD -----
  {
    id: 'hud-fps', name: 'FPS Counter', description: 'Contador de frames por segundo. Fuente: navegador del propio overlay (fuente principal: la ventana externa D2D del motor de overlays). En alternativa: hardware native vía última columna del shader (pending adapter).', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-fps',
  },
  {
    id: 'hud-cps', name: 'CPS Counter', description: 'Clicks por segundo (clics del ratón). Capturación mediante low-level mouse hooks; sin bloquear el ratón.', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-cps',
  },
  {
    id: 'hud-keystrokes', name: 'Keystrokes', description: 'Teclas presionadas/soltones en tiempo real (encerradas en una ruta de streams).', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-keystrokes',
  },
  {
    id: 'hud-clock', name: 'Clock', description: 'Reloj de pared tiempo real, configurable formato 24h.', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-clock',
  },
  {
    id: 'hud-ping', name: 'Ping Display', description: 'Ping de servidor. Fuente: adaptador Script API (requiere respuesta de servidor; pendiente Fase 5).', category: 'hud', dataSource: 'reserved',
  },
  {
    id: 'hud-memory', name: 'Memory Display', description: 'RAM y uso de juego. Fuente: adaptador nativo (memoria del proceso MCBE, adapter v1).', category: 'hud', dataSource: 'game-memory-v1', requiresMinVersion: '1.21.120.2',
  },
  {
    id: 'hud-direction', name: 'Direction HUD', description: 'Dirección del jugador y y subconjunto (N/S/E/W). Fuente: adaptador Script API (perodo de ejecución ServerLoop tick; pendiente Fase 5).', category: 'hud', dataSource: 'reserved',
  },
  // ----- VISUAL -----
  {
    id: 'visual-crosshair', name: 'Custom Crosshair', description: 'Crosshair huidor — próspera al recorrer distintos estilos, tamaños y colores (resources: formato personalizado del motor D2D).', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'custom-crosshair',
  },
  {
    id: 'visual-motionblur', name: 'Motion Blur', description: 'Blur de movimiento suave (engine de overlays).', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'visual-motionblur',
  },
  {
    id: 'visual-fog', name: 'Custom Fog', description: 'Fog variables (engine de overlays).', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'visual-fog',
  },
  {
    id: 'visual-colors', name: 'Color Adjustments', description: 'Gamma/saturación/balance (engine de overlays).', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'visual-colors',
  },
  {
    id: 'visual-zoom', name: 'Zoom', description: 'Zoom de cámara estilo DOTA (engine de overlays).', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'visual-zoom',
  },
  // ----- GAMEPLAY -----
  {
    id: 'game-drift', name: 'Death Counter', description: 'Contador de muertes (adaptador Script API opcional).', category: 'gameplay', dataSource: 'reserved',
  },
  {
    id: 'game-tps', name: 'TPS Counter', description: 'Ticks por segundo del servidor (adaptador Script API; pendiente Fase 5).', category: 'gameplay', dataSource: 'reserved',
  },
  {
    id: 'game-haste', name: 'Speed Display', description: 'Velocidad del jugador (adaptador Script API; pendiente Fase 5).', category: 'gameplay', dataSource: 'reserved',
  },
  // ----- UTILITIES -----
  {
    id: 'util-keybinds', name: 'Keybind Manager', description: 'Registro y gestión de atajos de teclado de cliente (llave del overlay). Datos de cliente: management local.', category: 'utilities', dataSource: 'local-only',
  },
  {
    id: 'util-session', name: 'Session Statistics', description: 'Estadísticas de sesión local (tiempo, visitas, etc. — datos de cliente).', category: 'utilities', dataSource: 'local-only',
  },
  {
    id: 'util-screens', name: 'Screenshot Manager', description: 'Toma capturas (pantalla de propagación exportada; sep 2022).', category: 'utilities', dataSource: 'local-only',
  },
];

export function filterCatalog(search, category) {
  const needle = (search ?? '').toLowerCase();
  return MODULES_CATALOG.filter(
    (m) => (!category || m.category === category) && (!needle || m.name.toLowerCase().includes(needle) || m.description.toLowerCase().includes(needle)),
  );
}
