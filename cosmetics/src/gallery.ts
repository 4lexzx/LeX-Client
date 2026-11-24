// LEX CLIENT — Sistema propio de cosméticos: Galería (local de usuario).
import { withBuiltinCosmetics } from './builtin.js';
import { dedupeCosmetics } from './importer.js';
import type { Cosmetic, Gallery } from './types.js';

export const COSMETIC_TYPES = ['local', 'resource-pack', 'import'] as const;
export const PRIVACY_RULE = 'Cosméticos propios: visibles sólo en LEX CLIENT de UN usuario (no aplican a otros). '
  + 'Cosméticos aplicables por otros: requieren redistribución de resource pack e instalación en el cliente (detalle en cosmetics/README.md).';

export function makeGallery(items: Cosmetic[]): Gallery {
  const list = dedupeCosmetics(withBuiltinCosmetics(items));
  const privateCount = list.filter((c) => c.source === 'private').length;
  return { items: list, total: list.length, privateCount, sharedCount: list.length - privateCount };
}

export function findCosmetic(gallery: Gallery, id: string): Cosmetic | null {
  return gallery.items.find((c) => c.id === id) ?? null;
}

/** Cosméticos aplicables a otros jugadores (visibles). */
export function sharedCosmetics(gallery: Gallery): Cosmetic[] {
  return gallery.items.filter((c) => c.source === 'shared');
}

export function privateCosmetics(gallery: Gallery): Cosmetic[] {
  return gallery.items.filter((c) => c.source === 'private');
}

export function filterByType(gallery: Gallery, type: Cosmetic['type']): Cosmetic[] {
  return gallery.items.filter((c) => c.type === type);
}

export const sampleGallery = (): Gallery => makeGallery([]);
