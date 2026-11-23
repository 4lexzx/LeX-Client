// @lex/compatibility — Lista de versiones del Overlay Engine (Fase 3) usado por adapters.
// Nota: guardia contra modulo: engine.cpp funciona solo con MSVC activo — guards como fallback.
export const OVERLAY_ENGINE_VERSION = '0.1.0';
export const OVERLAY_SUPPORTS = {
  transparentWindow: true,
  clickThrough: true,
  fpsAvg: true,
  cpsAvg: true,
  keystrokeRing: true,
  memoryReading: false, // Sin integración de memoria (Fase 5).
  ingameInjection: false, // Prohibido por diseño.
};
export const OVERLAY_ENGINE_PATH = (appData: string) => `${appData}\\lex-client\\overlay-engine.exe`;
export const OVERLAY_STATE_PATH = (appData: string) => `${appData}\\lex-client\\overlay-state.json`;
export const OVERLAY_STATS_PATH = (appData: string) => `${appData}\\lex-client\\overlay-stats.json`;
export const OVERLAY_MODULES_PATH = (appData: string) => `${appData}\\lex-client\\modules.json`;
