// LEX CLIENT — Catálogo estático de módulos (para el launcher: Mods screen).
// Versiones reales (phase 4): lista emigrada desde aquí hacia packages/module-sdk + modules/*.
// Estado: Fase 2 — metadatos del catálogo generales; datos de juego: ver `dataSource`.

/** @type {import('@lex/shared').ModuleMeta[]} */
export const MODULES_CATALOG = [
  // ----- HUD -----
  {
    id: 'hud-fps', name: 'FPS Counter', description: 'FPS reales del juego: contados en IDXGISwapChain::Present por el puente inyectado cada 500 ms. Sin puente muestra OV (solo overlay).', category: 'hud', dataSource: 'injected-present', overlayRenderer: 'hud-fps',
  },
  {
    id: 'hud-cps', name: 'CPS Counter', description: 'CPS exactos 1:1: cada clic físico cuenta 1, con ventanas separadas de 1000 ms para izquierdo/derecho (patrón Flarial).', category: 'hud', dataSource: 'local-hook', overlayRenderer: 'hud-cps',
  },
  {
    id: 'hud-keystrokes', name: 'Keystrokes', description: 'W/A/S/D, SPACE y clics LMB/RMB en vivo, sin SHIFT ni CTRL. Tamaño, color, opacidad y fondo configurables por módulo.', category: 'hud', dataSource: 'local-hook', overlayRenderer: 'hud-keystrokes',
  },
  {
    id: 'hud-armor', name: 'Armor Status', description: 'Iconos de armadura y durabilidad desde el bridge local armor.json. Sin dato muestra ranuras vacías, nunca porcentajes falsos.', category: 'hud', dataSource: 'external-bridge', overlayRenderer: 'hud-armor',
  },
  {
    id: 'hud-clock', name: 'Clock', description: 'Reloj de pared tiempo real, configurable formato 24h.', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-clock',
  },
  {
    id: 'hud-memory', name: 'Memory Display', description: 'Uso de RAM del sistema en tiempo real, medido por el overlay externo (sin leer memoria del juego).', category: 'hud', dataSource: 'external-overlay', overlayRenderer: 'hud-memory',
  },
  // ----- VISUAL -----
  {
    id: 'visual-crosshair', name: 'Custom Crosshair', description: '24 presets de mira dibujados por el overlay externo. Tamaño, color, opacidad y fondo configurables en sus ajustes.', category: 'visual', dataSource: 'external-overlay', overlayRenderer: 'custom-crosshair',
  },
  {
    id: 'visual-motionblur', name: 'Motion Blur', description: 'Blur aplicado por el shader inyectado al framebuffer del juego (D3D11/D3D12). No es un dibujo del overlay.', category: 'visual', dataSource: 'injected-shader', overlayRenderer: 'visual-motionblur',
  },
  {
    id: 'visual-fog', name: 'Custom Fog', description: 'Niebla personalizada aplicada por el shader inyectado al frame del juego (color y intensidad controlados por LEX).', category: 'visual', dataSource: 'injected-shader', overlayRenderer: 'visual-fog',
  },
  {
    id: 'visual-colors', name: 'Color Adjustments', description: 'Saturación de color ajustada por el shader inyectado en el framebuffer del juego.', category: 'visual', dataSource: 'injected-shader', overlayRenderer: 'visual-colors',
  },
  {
    id: 'visual-zoom', name: 'Zoom', description: 'Zoom de cámara por escalado UV del frame en el shader inyectado (1.35x), sin tocar archivos del juego.', category: 'visual', dataSource: 'injected-shader', overlayRenderer: 'visual-zoom',
  },
  // ----- COSMETICS (texturas animadas propias, renderizadas por el overlay) -----
  {
    id: 'cosmetic-dragon-wings', name: 'Dragon Wings', description: 'Alas de dragón animadas: 8 frames de textura propia LEXTEX1 dibujados sobre la ventana del juego (client-side).', category: 'cosmetics', dataSource: 'animated-texture', overlayRenderer: 'cosmetic-dragon-wings',
  },
  {
    id: 'cosmetic-ember-cape', name: 'Ember Cape', description: 'Capa de brasas animada: 8 frames de textura propia LEXTEX1 dibujados sobre la ventana del juego (client-side).', category: 'cosmetics', dataSource: 'animated-texture', overlayRenderer: 'cosmetic-ember-cape',
  },
  // ----- UTILITIES -----
  {
    id: 'util-keybinds', name: 'Keybind Manager', description: 'F8 abre y cierra el ClickGUI. Dentro del menú todo se controla con clic: pestañas, toggles, búsqueda y movimiento del HUD.', category: 'utilities', dataSource: 'local-only',
  },
  {
    id: 'util-session', name: 'Session Statistics', description: 'Estadísticas de sesión local (tiempo, visitas, etc. — datos de cliente).', category: 'utilities', dataSource: 'local-only',
  },
];

export function filterCatalog(search, category) {
  const needle = (search ?? '').toLowerCase();
  return MODULES_CATALOG.filter(
    (m) => (!category || m.category === category) && (!needle || m.name.toLowerCase().includes(needle) || m.description.toLowerCase().includes(needle)),
  );
}
