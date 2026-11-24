// LEX CLIENT — afterPack: copia el motor nativo y las texturas animadas al paquete final.
import { copyFileSync, mkdirSync, readdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');

function copyAssets(srcDir, destDir) {
  mkdirSync(destDir, { recursive: true });
  let count = 0;
  for (const file of readdirSync(srcDir)) {
    if (!/\.(png|lextex)$/i.test(file)) continue;
    copyFileSync(join(srcDir, file), join(destDir, file));
    count++;
  }
  console.log(`[LEX] afterPack: ${count} texturas -> ${destDir}`);
}

export default async function afterPack(context) {
  const src = join(root, 'native/overlay-engine/dist/lex-overlay-engine.exe');
  const dest = join(context.appOutDir, 'resources/overlay/lex-overlay-engine.exe');
  mkdirSync(dirname(dest), { recursive: true });
  copyFileSync(src, dest);
  console.log(`[LEX] afterPack: ${src} -> ${dest}`);

  const assets = join(root, 'native/overlay-engine/assets/cosmetics');
  copyAssets(assets, join(context.appOutDir, 'resources/overlay/assets/cosmetics'));
  copyAssets(assets, join(context.appOutDir, 'resources/assets/cosmetics'));
}
