// LEX CLIENT — Muestra el estado de las fases (npm run docs).
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const phases = [
  ['FASE 1 — Investigación y arquitectura', ['docs/phase1-research.md', 'ARCHITECTURE.md', 'README.md']],
  ['FASE 2 — Launcher funcional', ['docs/phase2-launcher.md', 'apps/launcher/electron/main.mjs', 'apps/launcher/electron/preload.mjs', 'apps/launcher/electron/detection.mjs', 'apps/launcher/electron/main.mjs', 'apps/launcher/src/screens/Home.tsx', 'apps/launcher/src/screens/VersionManager.tsx', 'apps/launcher/src/screens/Mods.tsx', 'apps/launcher/src/screens/Cosmetics.tsx', 'apps/launcher/src/screens/Settings.tsx', 'packages/shared/tests/version.test.mjs']],
  ['FASE 3 — Overlay nativo (Win32+Direct2D)', ['native/overlay-engine/CMakeLists.txt', 'native/overlay-engine/src/main.cpp']],
  ['FASE 4 — Módulos (SDK + registry)', ['packages/module-sdk/src/index.ts', 'modules/hud/fps.ts', 'packages/ui-components/src/index.tsx']],
  ['FASE 5 — Integración con Bedrock', ['integrations/package.json', 'integrations/bedrock/src/detect.ts', 'integrations/bedrock/src/index.ts', 'integrations/resource-packs/src/index.ts', 'integrations/script-api/types.ts', 'docs/phase5-integration.md']],
  ['FASE 6 — Cosméticos (3D viewer)', ['cosmetics/package.json', 'cosmetics/README.md', 'cosmetics/src/types.ts', 'cosmetics/src/gallery.ts', 'cosmetics/src/importer.ts', 'cosmetics/src/index.ts', 'apps/launcher/src/screens/Cosmetics.tsx']],
  ['FASE 7 — Instalador y distribución', ['scripts/build-launcher.ps1', 'apps/launcher/electron-builder.yml', 'CHANGELOG.md', 'docs/phase7-distribucion.md']],
];

for (const [name, files] of phases) {
  const total = files.length;
  const done = files.filter((f) => existsSync(join(root, f))).length;
  const pct = Math.round((done / total) * 100);
  console.log(`${pct}%  ${name}  (${done}/${total} archivos creados)`);
}
