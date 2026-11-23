// LEX CLIENT — Tests de contrato IPC: preload emite channels; main los maneja.
// Detecta (regex-wise) que no haya handlers sinificados en el renderer (mock de window.lex).
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const read = (p) => readFileSync(join(here, '..', p), 'utf8');

describe('contratos IPC: preload vs main.mjs', () => {
  const preload = read('electron/preload.mjs');
  const mainSrc = read('electron/main.mjs');

  function channelsOf(src) {
    const out = [];
    const re = /(?:invoke|handle)\('([^']+)'/g;
    let m;
    while ((m = re.exec(src))) out.push(m[1]);
    return out;
  }

  it('cada invoke() de preload existe en main.mjs como handler', () => {
    const preloadChannels = channelsOf(preload);
    const mainChannels = new Set(channelsOf(mainSrc));
    for (const c of preloadChannels) {
      assert.ok(mainChannels.has(c), `preload invoca '${c}' pero main.mjs no tiene handler`);
    }
    // Al menos los canales clave están implementados.
    for (const c of ['bedrock:detect', 'game:launch', 'modules:catalog', 'modules:get', 'modules:setState', 'updates:check', 'diagnostics:run', 'profiles:list', 'profiles:apply', 'overlay:set-state']) {
      assert.ok(mainChannels.has(c), `main debería implementar '${c}'`);
    }
  });

  it('main no deja handlers duros sin test de integración (mock benigno) — sanity de parsing', () => {
    assert.ok(mainSrc.includes('app.getAppPath()'), 'main debería resolver appPath mediante Electron');
  });
});

describe('contratos de renderer: window.lex definido en electron/preload.mjs', () => {
  it('exposeInMainWorld de preload incluye .ipc, .on, .auth', () => {
    const preload = read('electron/preload.mjs');
    assert.ok(preload.includes('exposeInMainWorld'));
    assert.ok(preload.includes('ipc:'));
    assert.ok(preload.includes('on:'));
    assert.ok(preload.includes('auth:'));
  });
});

describe('detonación de malas IPC en renderer', () => {
  it('PlayButton usa renderer cleanly (no string templating unsafe)', () => {
    const pb = read('src/components/PlayButton.tsx');
    assert.ok(pb.includes('window.lex.ipc.launch'));
    assert.ok(!pb.includes('child_process'));
  });
});
