// LEX CLIENT — Copia el catálogo real de módulos al workspace del launcher.
// Electron packagea desde apps/launcher, por lo que `modules/catalog.mjs` debe existir dentro de ese appDir.
import { copyFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const src = resolve(root, 'modules/catalog.mjs');
const dest = resolve(root, 'apps/launcher/modules/catalog.mjs');
mkdirSync(dirname(dest), { recursive: true });
copyFileSync(src, dest);
console.log(`[LEX] catálogo copiado: ${src} -> ${dest}`);
