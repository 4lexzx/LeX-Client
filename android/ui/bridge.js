// window.lex del launcher de PC, pero contra Java (LexNative). Cada llamada
// corre en un hilo de Java y vuelve con window.__lexDone(id, json).
(() => {
  const native = window.LexNative;
  const waiting = new Map();
  let next = 1;
  window.__lexDone = (id, text) => {
    const entry = waiting.get(id);
    if (!entry) return;
    waiting.delete(id);
    const value = JSON.parse(text);
    if (value && value.__error) entry.bad(new Error(value.__error));
    else entry.ok(value);
  };
  const call = (name) => (...args) => new Promise((ok, bad) => {
    const id = next++;
    waiting.set(id, { ok, bad });
    native.call(id, name, JSON.stringify(args));
  });
  const names = ['status', 'snapshot', 'files', 'play', 'modules', 'setModule', 'loadout', 'saveLoadout', 'skin', 'freshSkin', 'uninstall', 'shareLog', 'pickSkin', 'thumbsNeeded', 'saveThumb'];
  window.lex = Object.fromEntries(names.map((n) => [n, call(n)]));
  Object.assign(window.lex, {
    platform: 'android',
    models: async () => [],
    available: async () => [],
    texture: async (id) => `/lex/cosmetics/${encodeURIComponent(id)}.png`,
    icon: async (name) => `icons/${name}.png`,
    setSelfTest: async () => null,
    optimizer: async () => { throw new Error('El optimizador es solo para PC'); },
    openFolder: call('shareLog'),
  });
  // Sin skin propia todavia: la de LeX para que se vea el personaje.
  for (const n of ['skin', 'freshSkin']) {
    const real = window.lex[n];
    window.lex[n] = async () => (await real()) ?? 'default-skin.png';
  }
  // Java avisa cuando cambia algo fuera de la pagina (skin elegida, vuelta del juego).
  window.__lexEvent = (name, value) => dispatchEvent(new CustomEvent(`lex:${name}`, { detail: value }));
})();

// Raton en Android (teclado y raton conectados): la rueda y el arrastre mueven
// la lista que haya debajo, aunque el WebView no lo haga solo.
(() => {
  const scrollable = (node, axis) => {
    for (; node && node !== document.documentElement; node = node.parentElement) {
      const style = getComputedStyle(node);
      if (axis === 'y' && /(auto|scroll)/.test(style.overflowY) && node.scrollHeight > node.clientHeight + 1) return node;
      if (axis === 'x' && /(auto|scroll)/.test(style.overflowX) && node.scrollWidth > node.clientWidth + 1) return node;
    }
    return null;
  };
  addEventListener('wheel', (e) => {
    const dy = e.deltaMode === 1 ? e.deltaY * 40 : e.deltaY;
    const dx = e.deltaMode === 1 ? e.deltaX * 40 : e.deltaX;
    const row = (Math.abs(dx) > Math.abs(dy) || e.shiftKey) ? scrollable(e.target, 'x') : null;
    const target = row ?? scrollable(e.target, 'y') ?? scrollable(e.target, 'x');
    if (!target) return;
    e.preventDefault();
    if (target === row || scrollable(e.target, 'y') !== target) target.scrollLeft += dx || dy;
    else target.scrollTop += dy;
  }, { passive: false });

  let drag = null;
  addEventListener('pointerdown', (e) => {
    if (e.pointerType !== 'mouse' || e.button !== 0 || e.target.closest('#stage')) return;
    drag = { x: e.clientX, y: e.clientY, moved: false, ny: scrollable(e.target, 'y'), nx: scrollable(e.target, 'x') };
  });
  addEventListener('pointermove', (e) => {
    if (!drag) return;
    const dx = e.clientX - drag.x;
    const dy = e.clientY - drag.y;
    if (!drag.moved && Math.hypot(dx, dy) < 8) return;
    drag.moved = true;
    if (drag.ny) drag.ny.scrollTop -= dy;
    if (drag.nx && Math.abs(dx) > Math.abs(dy)) drag.nx.scrollLeft -= dx;
    drag.x = e.clientX;
    drag.y = e.clientY;
  });
  addEventListener('pointerup', () => {
    if (drag && drag.moved) {
      // Un arrastre no es un toque: se anula el clic que viene detras.
      addEventListener('click', (c) => { c.stopPropagation(); c.preventDefault(); }, { capture: true, once: true });
    }
    drag = null;
  });
})();
