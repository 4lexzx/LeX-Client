// LEX CLIENT — Módulo Visual: Custom Crosshair.
// FUENTE: external-overlay — crosshair estilizable via engine D2D (datos de render, del lado visible del engine).
import type { Module, ModuleMeta } from '@lex/module-sdk';

export interface CrosshairConfig {
  style: 'dot' | 'cross' | 'line' | 'tri' | 'default';
  size: number;
  thickness: number;
  color: [number, number, number, number];
}

/** Configuración que el engine de overlays consumirá cuando esté listo (Fase 5). */
export const DEFAULT_CROSSHAIR: CrosshairConfig = { style: 'dot', size: 8, thickness: 2, color: [255, 255, 255, 255] };

export const meta: ModuleMeta = {
  id: 'visual-crosshair',
  name: 'Custom Crosshair',
  description: 'Crosshair configurable por estilo/tamaño/color — renderizable mediante el engine de overlays nativo.',
  category: 'visual',
  dataSource: 'external-overlay',
  overlayRenderer: 'visual-crosshair',
};

export const crosshairModule: Module = {
  meta,
  activate() {
    // Nothing — engine renderiza con datos persistidos de forma visible.
  },
  deactivate() {
    // Nothing.
  },
};
