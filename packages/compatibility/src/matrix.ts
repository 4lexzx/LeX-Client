// @lex/compatibility — Matriz de compatibilidad (plantilla para pruebas reales).

import type { MinecraftEdition } from '@lex/shared';

export interface CompatibilityCell {
  feature: 'launcher' | 'overlays' | 'modules';
  supported: boolean | 'partial' | 'planned';
  note?: string;
}

export interface CompatibilityRow {
  minecraftVersion: string;
  edition: MinecraftEdition | 'custom';
  launcher: boolean | 'partial';
  overlays: boolean | 'partial' | 'planned';
  modules: boolean | 'partial' | 'planned';
  note?: string;
}

/** Fila base de la matriz: una de las versiones observadas realmente. */
export const MATRIX: CompatibilityRow[] = [
  { minecraftVersion: '1.21.120+', edition: 'retail', launcher: true, overlays: 'planned', modules: 'planned', note: 'Última estable retail detectada via Get-AppxPackage. Verificar en Fase 5.' },
  { minecraftVersion: '1.26.30.5', edition: 'retail', launcher: true, overlays: 'planned', modules: 'planned', note: 'Per changelog de Flarial (junio 2026). Estado a confirmar en Fase 5.' },
  { minecraftVersion: '1.21.120.21', edition: 'preview', launcher: true, overlays: 'planned', modules: 'planned', note: 'Preview per wiki. Añadir cuando se mida con detección real.' },
  { minecraftVersion: '1.20.0.0', edition: 'retail', launcher: true, overlays: 'planned', modules: 'planned', note: 'Adaptadores legacy: divididos por rango, sin guarantee.' },
  { minecraftVersion: 'Education', edition: 'education', launcher: false, overlays: 'planned', modules: 'planned', note: 'Excluido por diseño (no aplicaba mods seguros).' },
];
