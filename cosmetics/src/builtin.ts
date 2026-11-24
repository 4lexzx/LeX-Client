// LEX CLIENT — Cosméticos animados propios (assets generados por scripts/make-cosmetic-textures.mjs).
// No copiamos assets de otros clientes: estas texturas son generadas por LEX CLIENT.
import type { Cosmetic } from './types.js';
import { IMPORTED_COSMETICS } from './imported.js';

export const BUILTIN_COSMETICS: Cosmetic[] = [
  {
    id: 'dragon-wings',
    name: 'Alas de dragón',
    description: 'Alas animadas de 8 frames, renderizadas por el overlay sobre la ventana del juego. Sin tocar archivos del juego.',
    version: '1.0.0',
    type: 'local',
    source: 'private',
    animated: true,
    frames: 8,
    frameMs: 110,
    layers: 2,
    preview: 'dragon-wings.png',
    atlas: 'dragon-wings.lextex',
  },
  {
    id: 'ember-cape',
    name: 'Capa de brasas',
    description: 'Capa animada de 8 frames con textura propia de LEX CLIENT. Efecto visual en el cliente, sin modificar el skin del servidor.',
    version: '1.0.0',
    type: 'local',
    source: 'private',
    animated: true,
    frames: 8,
    frameMs: 140,
    layers: 1,
    preview: 'ember-cape.png',
    atlas: 'ember-cape.lextex',
  },
];

/** Los cosméticos integrados siempre están en la galería, aunque cosmetics.json no exista. */
export function withBuiltinCosmetics(items: Cosmetic[] = []): Cosmetic[] {
  const merged = new Map<string, Cosmetic>();
  for (const c of BUILTIN_COSMETICS) merged.set(c.id, c);
  for (const c of IMPORTED_COSMETICS) merged.set(c.id, c);
  for (const c of Array.isArray(items) ? items : []) merged.set(c.id, c); // el guardado del usuario prevalece
  return [...merged.values()];
}
