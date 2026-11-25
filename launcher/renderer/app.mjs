import { COSMETICS, SLOTS, find } from '../shared/cosmetics.mjs';
import { createStage } from './stage.mjs';

const page = document.querySelector('#page');
const toastEl = document.querySelector('#toast');
const stage = createStage(document.querySelector('#stage'), document.querySelector('#model'));

let loadout = {};
let busy = false;
let current = 'play';
const textures = new Map();

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

async function texture(id) {
  if (!textures.has(id)) textures.set(id, await lex.texture(id));
  return textures.get(id);
}

// preview: cosmetico que se prueba al pasar el raton.
async function wear(preview = null) {
  const chosen = preview ? { ...loadout, [preview.slot]: preview.id } : loadout;
  const items = Object.values(chosen).filter(Boolean).map(find);
  stage.wear(await Promise.all(items.map(async (i) => ({ ...i, url: await texture(i.id) }))));
}

// Miniatura: primer fotograma; en capas solo la cara exterior.
async function thumbnail(item) {
  const url = await texture(item.id);
  const w = item.slot === 'cape' ? 10 : item.slot === 'trail' ? item.rect[2] / 8 : item.rect[2];
  const k = Math.min(112 / item.rect[3], 120 / w);
  const node = el('div', { className: 'thumb' });
  node.style.cssText = `width:${w * k}px;height:${item.rect[3] * k}px;background:url(${url}) 0 0/${item.rect[2] * k}px ${item.rect[3] * item.frames * k}px no-repeat`;
  return el('div', { className: 'thumb-box' }, node);
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
    const status = await lex.status();
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
        box.append(tile(el('span', { className: 'about', textContent: m.description }), m.name, m.enabled, 'Activado', 'Desactivado', async (node, next) => {
          if (await attempt(() => lex.setModule(m.name, next))) node.set(next);
        }));
      }
      out.push(el('h2', { textContent: category }), box);
    }
    return out;
  },

  async cosmetics() {
    const out = [el('h1', { textContent: 'Cosméticos' }),
      el('p', { textContent: 'Se equipan al instante, también dentro del juego. Solo tú los ves y funcionan con skins clásicas.' })];
    for (const slot of SLOTS) {
      const grid = el('div', { className: 'grid' });
      for (const item of COSMETICS.filter((c) => c.slot === slot.id)) {
        const card = tile(await thumbnail(item), item.name, loadout[slot.id] === item.id, 'Equipado', 'Sin equipar', async (node, next) => {
          const saved = await attempt(() => lex.saveLoadout({ ...loadout, [slot.id]: next ? item.id : null }));
          if (!saved) return;
          loadout = saved;
          for (const other of grid.children) other.set(false);
          node.set(loadout[slot.id] === item.id);
          wear();
          stage.turn(slot.id === 'cape' || slot.id === 'wings' ? 205 : 25);
        });
        card.onpointerenter = () => {
          wear(item);
          stage.turn(slot.id === 'cape' || slot.id === 'wings' ? 205 : 25);
        };
        card.onpointerleave = () => wear();
        grid.append(card);
      }
      out.push(el('h2', { textContent: slot.label }), grid);
    }
    return out;
  },

  async optimizer() {
    const state = await lex.optimizer();
    const active = state.pending && state.pending !== 'restore' ? state.pending : state.preset;
    const run = async (id) => {
      const result = await attempt(() => lex.optimize(id));
      if (result) toast(result);
      if (current === 'optimizer') show('optimizer');
    };
    const list = el('div', { className: 'presets' });
    for (const p of state.presets) {
      const on = active === p.id;
      list.append(el('article', { className: on ? 'preset on' : 'preset' },
        el('h2', { textContent: p.label }),
        el('p', { textContent: p.description }),
        el('button', { className: 'primary small', textContent: on ? (state.pending ? 'Pendiente' : 'Aplicado') : 'Aplicar', disabled: on, onclick: () => run(p.id) })));
    }
    return [el('h1', { textContent: 'Optimizador' }),
      el('p', { textContent: 'Un clic cambia los gráficos de Minecraft. Tus ajustes originales se guardan y puedes recuperarlos cuando quieras.' }),
      list,
      el('div', { className: 'actions' }, el('button', {
        className: 'secondary', textContent: 'Restaurar mis gráficos', disabled: !state.preset && !state.pending, onclick: () => run('restore'),
      }))];
  },

  async settings() {
    const s = await lex.status();
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
    return [el('h1', { textContent: 'Estado' }), list,
      el('div', { className: 'actions' }, el('button', { className: 'secondary', textContent: 'Abrir carpeta de datos', onclick: () => lex.openFolder() }), remove)];
  },
};

async function show(name) {
  current = name;
  for (const b of document.querySelectorAll('nav button')) {
    if (b.dataset.page === name) b.setAttribute('aria-current', 'page');
    else b.removeAttribute('aria-current');
  }
  const content = await attempt(() => pages[name]());
  if (current !== name || !content) return;
  page.replaceChildren(...content);
  page.scrollTop = 0;
}

for (const b of document.querySelectorAll('nav button')) b.onclick = () => show(b.dataset.page);
lex.status().then((s) => { document.querySelector('#version').textContent = `v${s.client}`; });
// Si cambias un cosmetico dentro del juego, el launcher lo refleja al volver.
addEventListener('focus', async () => {
  loadout = (await attempt(() => lex.loadout())) ?? loadout;
  wear();
});
loadout = (await attempt(() => lex.loadout())) ?? {};
stage.skin(await attempt(() => lex.skin()));
wear();
show('play');
