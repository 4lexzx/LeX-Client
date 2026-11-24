// LEX CLIENT — Copia el catálogo real de módulos y los cosméticos al workspace del launcher.
// Electron packagea desde apps/launcher, por lo que `modules/*.mjs` debe existir dentro de ese appDir.
import { copyFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const destDir = resolve(root, 'apps/launcher/modules');
mkdirSync(destDir, { recursive: true });
for (const name of ['catalog.mjs', 'cosmetics.mjs']) {
  const src = resolve(root, 'modules', name);
  const dest = resolve(destDir, name);
  copyFileSync(src, dest);
  console.log(`[LEX] módulo copiado: ${src} -> ${dest}`);
}
