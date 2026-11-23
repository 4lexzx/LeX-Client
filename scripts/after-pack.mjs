// LEX CLIENT — afterPack: copia el motor nativo dentro del paquete final.
import { copyFileSync, mkdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
export default async function afterPack(context) {
  const src = join(root, 'native/overlay-engine/dist/lex-overlay-engine.exe');
  const dest = join(context.appOutDir, 'resources/overlay/lex-overlay-engine.exe');
  mkdirSync(dirname(dest), { recursive: true });
  copyFileSync(src, dest);
  console.log(`[LEX] afterPack: ${src} -> ${dest}`);
}
