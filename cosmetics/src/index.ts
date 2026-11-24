// LEX CLIENT — Índice de sistema de cosméticos.
export * from './types.js';
export * from './builtin.js';
export * from './gallery.js';
export * from './importer.js';

import { makeGallery } from './gallery.js';
import type { Cosmetic } from './types.js';

export const COSMETICS_PRIVACY_RULE =
  'Cosméticos propios: visibles solo en LEX CLIENT de un usuario individual. ' +
  'Cosméticos aplicables a otros: requieren redistribución de resource pack e instalación en el cliente (ver cosmetics/README.md).';

export function createDefaultGallery(items: Cosmetic[] = []): ReturnType<typeof makeGallery> {
  return makeGallery(items);
}
