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
  const names = ['status', 'snapshot', 'files', 'play', 'modules', 'setModule', 'loadout', 'saveLoadout', 'skin', 'freshSkin', 'uninstall', 'shareLog', 'pickSkin'];
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
