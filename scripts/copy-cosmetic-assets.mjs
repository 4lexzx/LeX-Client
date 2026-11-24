// LEX CLIENT — Copia las texturas animadas al launcher (preview IPC) y al motor nativo (renderer).
import { copyFileSync, mkdirSync, readdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const srcDir = resolve(root, 'cosmetics/assets');
const dests = [
  resolve(root, 'apps/launcher/assets/cosmetics'),
  resolve(root, 'native/overlay-engine/assets/cosmetics'),
];

mkdirSync(srcDir, { recursive: true });
const files = readdirSync(srcDir).filter((f) => /\.(png|lextex)$/i.test(f));
for (const destDir of dests) {
  mkdirSync(destDir, { recursive: true });
  for (const file of files) {
    copyFileSync(resolve(srcDir, file), resolve(destDir, file));
  }
  console.log(`[LEX] texturas copiadas: ${files.length} -> ${destDir}`);
}
