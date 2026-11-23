// LEX CLIENT — Sistema propio de cosméticos: Tipos.
export type CosmeticType = 'local' | 'resource-pack' | 'import';
export type CosmeticSource = 'private' | 'shared';

export interface Cosmetic {
  id: string;
  name: string;
  description?: string;
  /** Versión del cosmético (ej: "1.0.0"). */
  version?: string;
  /** Tipo: cosmético propio / resource pack / importado por archivo. */
  type: CosmeticType;
  /** Número de capas renderizables (para vista 3D). */
  layers?: number;
  /** Thumbnail URL/path (opcional). */
  preview?: string;
  /** Estado de visibilidad: private (solo cliente) vs shared (aplicable a otros con el RP). */
  source: CosmeticSource;
  /** Hash SHA256-ish para validación de import (Fase 7). */
  hash?: string;
}

export interface Gallery {
  items: Cosmetic[];
  total: number;
  privateCount: number;
  sharedCount: number;
}

export interface ImportValidationIssue {
  itemId: string;
  level: 'error' | 'warning';
  message: string;
}

export interface ImportResult {
  accepted: Cosmetic[];
  rejected: ImportValidationIssue[];
}
