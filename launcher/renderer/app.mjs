import { COSMETICS, SLOTS, find, registerFiles } from '../shared/cosmetics.mjs';

// Cosmeticos por archivos: el proceso principal los lee del disco.
const fileCosmetics = await window.lex.files().catch(() => null);
if (fileCosmetics) registerFiles(fileCosmetics);
import { createViewer } from './viewer.mjs';

const page = document.querySelector('#page');
const toastEl = document.querySelector('#toast');
const view = document.querySelector('#view');
const viewer = createViewer((id) => lex.texture(id));
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

let loadout = {};
let busy = false;
let current = 'play';
let slotTab = SLOTS[0].id;
let available = null; // mascotas del juego que existen en esta version

// --- escenario: tu personaje con lo equipado ---
let yaw = 25;
let goal = null;
let drag = null;
view.onpointerdown = (e) => { drag = { x: e.clientX, yaw }; goal = null; view.setPointerCapture(e.pointerId); };
view.onpointermove = (e) => { if (drag) yaw = drag.yaw - (e.clientX - drag.x) * 0.6; };
view.onpointerup = view.onpointercancel = () => { drag = null; };
const turn = (angle) => { goal = angle + Math.round((yaw - angle) / 360) * 360; };
const wornItems = () => Object.values(loadout).filter(Boolean).map(find).filter((i) => i && i.geo !== 'trail' && i.geo !== 'dfx' && (!i.source || !available || available.has(i.id)));

// Tarjetas visibles: se dibujan por turnos dentro de un presupuesto por fotograma.
const cards = new Set();
let last = 0;
let order = 0;
function frame(now) {
  requestAnimationFrame(frame);
  if (document.hidden || now - last < 30) return;
  last = now;
  const time = still ? 1 : now / 1000;
  const box = view.getBoundingClientRect();
  const ratio = Math.min(devicePixelRatio, 1.5);
  const [w, h] = [Math.round(box.width * ratio), Math.round(box.height * ratio)];
  if (w > 0 && h > 0) {
    if (view.width !== w || view.height !== h) { view.width = w; view.height = h; }
    if (goal !== null) { yaw += (goal - yaw) * 0.14; if (Math.abs(goal - yaw) < 0.3) goal = null; }
    else if (!drag && !still) yaw += 0.25;
    viewer.draw(view, wornItems(), { yaw, wide: true, time });
  }
  const frameBox = page.getBoundingClientRect();
  const list = [...cards].filter((c) => {
    const b = c.getBoundingClientRect();
    return b.height > 0 && b.bottom > frameBox.top && b.top < frameBox.bottom;
  });
  const limit = performance.now() + 9;
  for (let i = 0; i < list.length && performance.now() < limit; i++) {
    const c = list[order++ % list.length];
    c.paint(time);
  }
}
requestAnimationFrame(frame);

function el(tag, props = {}, ...children) {
  const node = Object.assign(document.createElement(tag), props);
  for (const [k, v] of Object.entries(props)) if (k.startsWith('aria-') || k === 'role') node.setAttribute(k, v);
  node.append(...children);
  return node;
}

let toastTimer;
function toast({ ok, message }) {
  toastEl.textContent = message;
  toastEl.className = ok ? 'show' : 'show bad';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toastEl.className = ''; }, 7000);
}

async function attempt(action) {
  try {
    return await action();
  } catch (error) {
    toast({ ok: false, message: String(error.message).replace(/^Error invoking remote method '[^']+': (Error: )?/, '') });
    return null;
  }
}

const slug = (name) => name.normalize('NFD').replace(/[^\x00-\x7f]/g, '').toLowerCase().replace(/[^a-z0-9]/g, '-');
const icons = new Map();
function moduleIcon(name) {
  const img = el('img', { className: 'mod-icon', alt: '' });
  const key = slug(name);
  if (!icons.has(key)) icons.set(key, lex.icon(key));
  icons.get(key).then((url) => { if (url) img.src = url; });
  return img;
}

// Tarjeta 3D de un cosmetico, igual que dentro del juego.
function preview(item, index) {
  const node = el('canvas', { className: 'card3d', width: 172, height: 190 });
  const back = item.slot === 'cape' || item.slot === 'wings';
  const base = back ? 180 : item.slot === 'pet' ? -24 : 0;
  node.paint = (time) => {
    if (item.geo === 'trail') viewer.sparks(node, item, time);
    else viewer.draw(node, [item], { yaw: base + Math.sin(time * 0.9 + index) * 30, head: item.slot === 'head', wide: (item.slot === 'wings' && item.geo !== 'mini') || item.slot === 'aura' || item.slot === 'pet', time, focus: ['pet', 'kill', 'death', 'hit'].includes(item.slot) });
  };
  cards.add(node);
  return node;
}

function tile(media, name, on, yes, no, onToggle, hint) {
  const bar = el('span', { className: 'bar', textContent: on ? yes : no });
  const node = el('button', { className: 'tile', 'aria-pressed': String(on), title: hint ?? '' }, media, el('strong', { textContent: name }), bar);
  node.set = (value) => {
    node.setAttribute('aria-pressed', String(value));
    bar.textContent = value ? yes : no;
  };
  node.onclick = () => onToggle(node, node.getAttribute('aria-pressed') !== 'true');
  return node;
}

const pages = {
  async play() {
    const status = await lex.snapshot();
    const label = () => (busy ? 'Abriendo' : status.running ? 'Activar LeX' : 'Jugar');
    const button = el('button', { className: 'primary', textContent: label(), disabled: busy });
    button.onclick = async () => {
      busy = true;
      button.disabled = true;
      button.textContent = label();
      const result = await attempt(() => lex.play());
      busy = false;
      if (result) toast(result);
      if (current === 'play') show('play');
    };
    return [el('div', { className: 'hero' },
      el('img', { src: 'logo-mark.png', alt: '' }),
      el('h1', { textContent: 'Juega a tu manera' }),
      el('p', { textContent: 'Todo se prepara solo: módulos, cosméticos y rendimiento.' }),
      button,
      el('p', {
        className: 'note',
        textContent: status.installed ? `Minecraft ${status.version}. Dentro de un mundo, pulsa L para abrir LeX.` : 'No se encontró Minecraft for Windows.',
      }))];
  },

  async modules() {
    const list = await lex.modules();
    const out = [el('h1', { textContent: 'Módulos' }),
      el('p', { textContent: 'Se aplican al momento si estás jugando. Colores, RGB y posición se ajustan dentro del juego.' })];
    for (const category of [...new Set(list.map((m) => m.category))]) {
      const box = el('div', { className: 'modules' });
      for (const m of list.filter((x) => x.category === category)) {
        box.append(tile(moduleIcon(m.name), m.name, m.enabled, 'Activado', 'Desactivado', async (node, next) => {
          node.set(next);
          if (!(await attempt(() => lex.setModule(m.name, next)))) node.set(!next);
        }, m.description));
      }
      out.push(el('h2', { textContent: category }), box);
    }
    return out;
  },

  async cosmetics() {
    const tabs = el('div', { className: 'tabs', role: 'tablist' });
    const grid = el('div', { className: 'grid' });
    const fill = () => {
      cards.clear();
      grid.replaceChildren();
      for (const b of tabs.children) b.setAttribute('aria-selected', String(b.dataset.slot === slotTab));
      const items = COSMETICS.filter((c) => c.slot === slotTab && (!c.source || !available || available.has(c.id)));
      items.forEach((item, i) => {
        const card = tile(preview(item, i), item.name, loadout[slotTab] === item.id, 'Equipado', 'Equipar', async (node, next) => {
          // Primero lo que se ve; el guardado va detras.
          loadout = { ...loadout, [item.slot]: next ? item.id : null };
          for (const other of grid.children) other.set(false);
          node.set(next);
          turn(item.slot === 'cape' || item.slot === 'wings' ? 200 : 25);
          const saved = await attempt(() => lex.saveLoadout(loadout));
          if (saved) loadout = saved;
        });
        grid.append(card);
      });
      page.scrollTop = 0;
    };
    for (const slot of SLOTS) {
      const b = el('button', { textContent: slot.label, role: 'tab', onclick: () => { slotTab = slot.id; fill(); } });
      b.dataset.slot = slot.id;
      tabs.append(b);
    }
    fill();
    return [el('h1', { textContent: 'Cosméticos' }), tabs, grid];
  },

  async optimizer() {
    const state = await lex.optimizer();
    const active = state.pending && state.pending !== 'restore' ? state.pending : state.preset;
    const run = async (id, node) => {
      node.disabled = true;
      node.textContent = 'Aplicando';
      const result = await attempt(() => lex.optimize(id));
      if (result) toast(result);
      if (current === 'optimizer') show('optimizer');
    };
    const name = state.presets.find((p) => p.id === active)?.label;
    const banner = el('div', { className: active ? 'applied' : 'applied none' },
      el('strong', { textContent: active ? (state.pending ? `${name}: se aplicará al pulsar Jugar` : `Aplicado ahora: ${name}`) : state.pending === 'restore' ? 'Tus gráficos se restaurarán al pulsar Jugar' : 'Ningún preajuste aplicado' }),
      el('span', { textContent: state.ingame ? 'El mod Optimizador también está activo dentro del juego.' : 'El mod Optimizador está apagado dentro del juego.' }));
    const list = el('div', { className: 'presets' });
    for (const p of state.presets) {
      const on = active === p.id;
      const button = el('button', { className: 'primary small', textContent: on ? (state.pending ? 'Pendiente' : 'Aplicado') : 'Aplicar', disabled: on });
      button.onclick = () => run(p.id, button);
      list.append(el('article', { className: on ? 'preset on' : 'preset' }, el('h2', { textContent: p.label }), el('p', { textContent: p.description }), button));
    }
    const now = el('dl', { className: 'now' });
    for (const o of state.current) now.append(el('dt', { textContent: o.label }), el('dd', { textContent: o.value, className: o.good ? 'good' : '' }));
    const back = el('button', { className: 'secondary', textContent: 'Restaurar mis gráficos', disabled: !state.preset && !state.pending });
    back.onclick = () => run('restore', back);
    return [el('h1', { textContent: 'Optimizador' }), banner, list,
      el('h2', { textContent: 'Lo que Minecraft usa ahora' }), now,
      el('div', { className: 'actions' }, back)];
  },

  async settings() {
    const s = await lex.snapshot();
    const rows = [
      ['Minecraft', s.installed ? s.version : 'No instalado', s.installed],
      ['Carpeta del juego', s.dir ?? 'No encontrada', s.dir],
      ['Juego abierto', s.running ? 'Sí' : 'No', true],
      ['Cliente nativo', s.native ? 'Listo' : 'Sin compilar', s.native],
      ['Datos en Minecraft', s.ready ? 'Preparados' : 'Se crean al pulsar Jugar', true],
      ['Versión de LeX', s.client, true],
    ];
    const list = el('dl');
    for (const [k, v, ok] of rows) list.append(el('dt', { textContent: k }), el('dd', { textContent: v, className: ok ? '' : 'bad' }));
    const remove = el('button', {
      className: 'secondary',
      textContent: 'Quitar LeX de Minecraft',
      onclick: async () => {
        const result = await attempt(() => lex.uninstall());
        if (result) toast(result);
        show('settings');
      },
    });
    const test = el('button', {
      className: s.selfTest ? '' : 'secondary',
      textContent: s.selfTest ? 'Modo prueba: activado' : 'Modo prueba: desactivado',
      title: 'Muestra la vida, los golpes y las kills también sobre ti. Se aplica la próxima vez que pulses Jugar con Minecraft cerrado.',
      onclick: async () => {
        await attempt(() => lex.setSelfTest(!s.selfTest));
        toast(s.selfTest ? 'Modo prueba desactivado' : 'Modo prueba activado: cierra Minecraft y pulsa Jugar');
        show('settings');
      },
    });
    return [el('h1', { textContent: 'Estado' }), list,
      el('p', { className: 'hint', textContent: 'Modo prueba: ves sobre tu personaje la vida, los golpes y las kills, para comprobarlos sin otro jugador (por ejemplo, con daño de caída en tu mundo).' }),
      el('div', { className: 'actions' }, test),
      el('div', { className: 'actions' }, el('button', { className: 'secondary', textContent: 'Abrir carpeta de datos', onclick: () => lex.openFolder() }), remove)];
  },
};

async function show(name, fresh = true) {
  current = name;
  for (const b of document.querySelectorAll('nav button')) {
    if (b.dataset.page === name) b.setAttribute('aria-current', 'page');
    else b.removeAttribute('aria-current');
  }
  const content = await attempt(() => pages[name]());
  if (current !== name || !content) return;
  if (name !== 'cosmetics') cards.clear();
  const top = fresh ? 0 : page.scrollTop;
  page.replaceChildren(...content);
  page.scrollTop = top;
}

// El estado se muestra al instante con lo ultimo conocido y se corrige solo.
let known = '';
async function refresh() {
  const s = await attempt(() => lex.status());
  const text = JSON.stringify(s);
  if (!s || text === known) return;
  known = text;
  document.querySelector('#version').textContent = `v${s.client}`;
  if (!busy && (current === 'play' || current === 'settings')) show(current, false);
}

for (const b of document.querySelectorAll('nav button')) {
  b.onclick = () => {
    show(b.dataset.page);
    if (b.dataset.page === 'settings') refresh();
  };
}
// Si cambias un cosmetico dentro del juego, el launcher lo refleja al volver.
addEventListener('focus', async () => {
  loadout = (await attempt(() => lex.loadout())) ?? loadout;
  refresh();
});

// La skin sale de la ultima conocida antes de pintar nada.
const saved = localStorage.getItem('skin');
if (saved) await viewer.setSkin(saved);
loadout = (await attempt(() => lex.loadout())) ?? {};
show('play');
const quick = await attempt(() => lex.skin());
if (quick && quick !== saved) await viewer.setSkin(quick);
refresh();
setInterval(refresh, 15000);
viewer.setExtra((await attempt(() => lex.models())) ?? []);
available = new Set((await attempt(() => lex.available())) ?? []);
const newest = (await attempt(() => lex.freshSkin())) ?? quick;
if (newest) {
  if (newest !== quick) await viewer.setSkin(newest);
  try { localStorage.setItem('skin', newest); } catch { /* sin espacio */ }
}
