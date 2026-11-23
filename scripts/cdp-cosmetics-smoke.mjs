// LEX CLIENT — Smoke funcional de cosméticos sobre el renderer abierto por CDP.
// Uso: node scripts/cdp-cosmetics-smoke.mjs (con el launcher en --remote-debugging-port=9222)
import { writeFileSync } from 'node:fs';

const targets = await (await fetch('http://127.0.0.1:9222/json/list')).json();
const page = targets.find((t) => t.type === 'page' && (t.title === 'LEX CLIENT' || t.url.includes('app.asar') || t.url.includes('resources/app/')));
if (!page) throw new Error('No se encontró la página del launcher en CDP');

const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });

let seq = 0;
const pending = new Map();
ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg); pending.delete(msg.id); }
};
const send = (method, params = {}) => new Promise((resolve) => {
  const id = ++seq;
  pending.set(id, resolve);
  ws.send(JSON.stringify({ id, method, params }));
});
async function evaluate(expression) {
  const r = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (r.result?.exceptionDetails) throw new Error(r.result.exceptionDetails.exception?.description || r.result.exceptionDetails.text);
  return r.result?.result?.value;
}
const wait = (ms) => evaluate(`new Promise(r=>setTimeout(r,${ms}))`);

const results = [];
const check = (name, ok, detail) => {
  results.push({ name, ok });
  console.log(`${ok ? 'OK  ' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`);
};

try {
  const clicked = await evaluate(`(()=>{const el=[...document.querySelectorAll('button')].find(b=>b.textContent?.includes('Cosméticos')); if(!el) return false; el.click(); return true;})()`);
  check('navega a Cosméticos', clicked === true);
  await wait(400);

  const text = await evaluate('document.body.innerText');
  check('pantalla de Cosméticos', String(text).includes('Cosméticos — LEX CLIENT'));

  const file = 'C:\\Users\\Alexander\\AppData\\Local\\Temp\\opencode\\lex-cosmetic-test.json';
  writeFileSync(file, JSON.stringify([{ id: 'smoke-cosmetic', name: 'Cosmético de prueba LEX', type: 'import', source: 'private', version: '1.0.0' }], null, 2));

  const doc = await send('DOM.getDocument');
  const found = await send('DOM.querySelector', { nodeId: doc.result.root.nodeId, selector: '#cosmetic-file-input' });
  check('input de importación presente', Boolean(found.result?.nodeId));
  if (found.result?.nodeId) {
    await send('DOM.setFileInputFiles', { files: [file], nodeId: found.result.nodeId });
    await wait(800);
    const after = await evaluate('document.body.innerText');
    check('importa cosméticos reales', String(after).includes('Cosmético de prueba LEX'));
    check('estado de importación', String(after).includes('Importados: 1'));
  }
} finally {
  ws.close();
}

const failed = results.filter((r) => !r.ok).length;
console.log(failed === 0 ? 'ALL OK' : `${failed} FALLOS`);
process.exitCode = failed === 0 ? 0 : 1;
