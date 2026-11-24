// LEX CLIENT — Cosméticos animados incluidos (espejo JS de cosmetics/src/builtin.ts para el main process).
// Los IDs deben coincidir con los módulos `cosmetic-*` del catálogo.
export const BUILTIN_COSMETICS = [
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

/** Fusiona los incluidos con los guardados por el usuario; dedupe por id. */
export function withBuiltinCosmetics(items = []) {
  const byId = new Map();
  for (const c of BUILTIN_COSMETICS) byId.set(c.id, c);
  for (const c of Array.isArray(items) ? items : []) if (c && typeof c.id === 'string') byId.set(c.id, c);
  return [...byId.values()];
}
