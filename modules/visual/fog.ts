// LEX CLIENT — Módulo Visual: Custom Fog.
// FUENTE: external-overlay — parámetros de volumen nuboso configurables mediante engine de overlays.
import type { Module, ModuleMeta } from '@lex/module-sdk';

export interface FogConfig { volume: number; color: [number, number, number]; offset: number }

/** Configuración que el engine de overlays consumirá cuando esté listo (Fase 5). */
export const DEFAULT_FOG: FogConfig = { volume: 0.8, color: [0.03, 0.04, 0.06], offset: 0 };

export const meta: ModuleMeta = {
  id: 'visual-fog',
  name: 'Custom Fog',
  description: 'Volumen nuboso + color dinámico — mediante engine de overlays (sombra real renderizada en el canvas del engine).',
  category: 'visual',
  dataSource: 'external-overlay',
  overlayRenderer: 'visual-fog',
};

export const fogModule: Module = {
  meta,
  activate() {
    // Nothing.
  },
  deactivate() {
    // Nothing.
  },
};
