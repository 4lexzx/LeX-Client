// LEX CLIENT — Smoke funcional real sobre el renderer ya abierto (CDP).
// Uso: iniciar el launcher con --remote-debugging-port=9222 y ejecutar: node scripts/cdp-smoke.mjs
const targets = await (await fetch('http://127.0.0.1:9222/json/list')).json();
const page = targets.find((t) => t.type === 'page' && (t.title === 'LEX CLIENT' || t.url.includes('app.asar') || t.url.includes('resources/app/')));
if (!page) throw new Error('No se encontró la página del launcher en CDP');

const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => {
  ws.onopen = resolve;
  ws.onerror = reject;
});

let seq = 0;
const pending = new Map();
ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  if (msg.id && pending.has(msg.id)) {
    pending.get(msg.id)(msg);
    pending.delete(msg.id);
  }
};
function send(method, params = {}) {
  return new Promise((resolve) => {
    const id = ++seq;
    pending.set(id, resolve);
    ws.send(JSON.stringify({ id, method, params }));
  });
}
async function evaluate(expression) {
  const response = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (response.result?.exceptionDetails) {
    const d = response.result.exceptionDetails;
    throw new Error(d.exception?.description || d.text || 'Runtime.evaluate failed');
  }
  return response.result?.result?.value;
}

const results = [];
function check(name, ok, detail) {
  results.push({ name, ok, detail });
  console.log(`${ok ? 'OK  ' : 'FAIL'}  ${name}${detail ? ` — ${detail}` : ''}`);
}

try {
  const title = await evaluate('document.title');
  check('renderer cargado', title === 'LEX CLIENT', `title=${title}`);

  const rootChildren = await evaluate('document.querySelector("#root")?.children.length ?? 0');
  check('React montó la UI', rootChildren > 0, `children=${rootChildren}`);

  const bridge = await evaluate('typeof window.lex?.ipc?.detect');
  check('preload expone window.lex', bridge === 'function', `typeof=${bridge}`);

  const detection = await evaluate('window.lex.ipc.detect()');
  const installs = detection?.installations ?? [];
  check('detecta Minecraft real', installs.length > 0, `count=${installs.length}, latest=${detection?.latest?.version ?? 'null'}`);
  check('resuelve ejecutable', Boolean(installs[0]?.executable), installs[0]?.executable ?? 'sin exe');
  check('resuelve PackageFamilyName', Boolean(installs[0]?.packageFamilyName), installs[0]?.packageFamilyName ?? 'sin pfn');

  const catalog = await evaluate('(async()=>{const c=await window.lex.ipc.catalog(); return {count:c.modules.length, sample:c.modules[0]?.id};})()');
  check('catálogo de módulos', catalog?.count > 0, `count=${catalog?.count}, sample=${catalog?.sample}`);

  const modules = await evaluate('window.lex.ipc.modulesGet()');
  check('estado de módulos', typeof modules === 'object' && modules !== null, `keys=${Object.keys(modules ?? {}).length}`);

  const setModule = await evaluate('window.lex.ipc.modulesSetState({id:"hud-fps", enabled:true})');
  check('toggle de módulo', setModule?.ok === true, JSON.stringify(setModule));
  const modulesAfter = await evaluate('window.lex.ipc.modulesGet()');
  check('persiste toggle', modulesAfter?.['hud-fps']?.enabled === true, `enabled=${modulesAfter?.['hud-fps']?.enabled}`);

  const diagnostics = await evaluate('window.lex.ipc.diagnostics()');
  check('diagnostics', Boolean(diagnostics?.platform), `issues=${diagnostics?.issues?.length ?? '?'}`);

  const updates = await evaluate('window.lex.ipc.updatesCheck()');
  check('updates check', typeof updates?.launcherVersion === 'string', `launcher=${updates?.launcherVersion}`);

  const overlay = await evaluate('window.lex.ipc.overlaySetState({visible:true, clickThrough:false})');
  check('overlay bridge', overlay?.ok === true, `ok=${overlay?.ok}`);

  if (process.argv.includes('--launch') && installs[0]?.id) {
    const launch = await evaluate(`window.lex.ipc.launch(${JSON.stringify(installs[0].id)})`);
    check('lanza Minecraft', launch?.ok === true, JSON.stringify(launch));
  }
} finally {
  ws.close();
}

const failed = results.filter((r) => !r.ok).length;
console.log(failed === 0 ? 'ALL OK' : `${failed} FALLOS`);
process.exitCode = failed === 0 ? 0 : 1;
