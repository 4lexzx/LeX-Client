// LEX CLIENT — Copia el motor nativo al workspace del launcher para empaquetado.
import { copyFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const src = resolve(root, 'native/overlay-engine/dist/lex-overlay-engine.exe');
const dest = resolve(root, 'apps/launcher/overlay/lex-overlay-engine.exe');
mkdirSync(dirname(dest), { recursive: true });
copyFileSync(src, dest);
console.log(`[LEX] overlay copiado: ${src} -> ${dest}`);
