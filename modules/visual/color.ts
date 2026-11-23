// LEX CLIENT — Módulo Visual: Color Adjustments.
// FUENTE: external-overlay — gamma/saturación/balance mediante engine de overlays (D2D).
import type { Module, ModuleMeta } from '@lex/module-sdk';

export interface ColorAdjustments { gamma: number; brightness: number; contrast: number; saturation: number }

/** Configuración de ajustes mediante engine de overlays (D2D), Fase 5. */
export const DEFAULT_COLORS: ColorAdjustments = { gamma: 1.0, brightness: 0, contrast: 0, saturation: 0 };

export const meta: ModuleMeta = {
  id: 'visual-colors',
  name: 'Color Adjustments',
  description: 'Gamma/saturación/balance mediante engine de overlays — aplicado en el canvas del engine.',
  category: 'visual',
  dataSource: 'external-overlay',
  overlayRenderer: 'visual-colors',
};

export const colorModule: Module = {
  meta,
  activate() {
    // Nothing.
  },
  deactivate() {
    // Nothing.
  },
};
