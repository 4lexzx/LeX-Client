const targets = await (await fetch('http://127.0.0.1:9222/json/list')).json();
console.log(JSON.stringify(targets.map(t => ({ type: t.type, title: t.title, url: t.url })), null, 2));
const page = targets.find(t => t.type === 'page');
if (!page) process.exit(1);
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(r => { ws.onopen = r; });
let id = 0;
const pending = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
async function ev(expr) {
  const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true });
  return r.result?.result?.value;
}
console.log('TITLE:', await ev('document.title'));
console.log('HTML:', (await ev('document.documentElement.outerHTML'))?.slice(0, 800));
console.log('SCRIPTS:', await ev('[...document.scripts].map(s=>s.src)'));
console.log('RES:', await ev('performance.getEntriesByType("resource").map(r=>r.name).slice(0,10)'));
ws.close();
