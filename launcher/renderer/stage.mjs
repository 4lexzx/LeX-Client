// Personaje 3D con transformaciones CSS. Usa la misma geometria y texturas que
// carga el juego. Modelo: Y arriba, frente en -Z. CSS: Y abajo, frente en +Z.
import { bonesOf } from '../shared/cosmetics.mjs';

const K = 9; // pixeles por unidad de modelo

// cara: [eje ancho, eje alto, normal, rotacion CSS]
const FACES = {
  north: (w, h, d) => [w, h, `translateZ(${(d / 2) * K}px)`, 1],
  south: (w, h, d) => [w, h, `rotateY(180deg) translateZ(${(d / 2) * K}px)`, 0.95],
  east: (w, h, d) => [d, h, `rotateY(90deg) translateZ(${(w / 2) * K}px)`, 0.78],
  west: (w, h, d) => [d, h, `rotateY(-90deg) translateZ(${(w / 2) * K}px)`, 0.78],
  up: (w, h, d) => [w, d, `rotateX(90deg) translateZ(${(h / 2) * K}px)`, 1.08],
  down: (w, h, d) => [w, d, `rotateX(-90deg) translateZ(${(h / 2) * K}px)`, 0.62],
};

function box(host, { origin, size, uv, pivot, rotation }, item) {
  let parent = host;
  if (rotation) {
    // Giro propio del cubo alrededor de su pivote.
    parent = document.createElement('div');
    parent.className = 'bone';
    parent.style.transformOrigin = `${pivot[0] * K}px ${-(pivot[1] - 16) * K}px ${-pivot[2] * K}px`;
    parent.style.transform = `rotateZ(${rotation[2]}deg) rotateY(${-rotation[1]}deg) rotateX(${-rotation[0]}deg)`;
    host.append(parent);
  }
  const [w, h, d] = size;
  const at = [(origin[0] + w / 2) * K, -(origin[1] + h / 2 - 16) * K, -(origin[2] + d / 2) * K];
  const faces = [];
  for (const [name, spec] of Object.entries(uv)) {
    const [fw, fh, transform, light] = FACES[name](w, h, d);
    if (fw <= 0 || fh <= 0) continue;
    const f = document.createElement('div');
    f.className = size.includes(0) ? 'face flat' : 'face';
    f.style.width = `${fw * K}px`;
    f.style.height = `${fh * K}px`;
    if (typeof spec === 'string') {
      f.style.background = spec;
      f.style.transform = `translate3d(${at.map((v) => `${v}px`).join(',')}) ${transform} translate(-50%, -50%)`;
    } else {
      const [uw, uh] = spec.uv_size;
      const sx = fw / Math.abs(uw);
      const sy = fh / Math.abs(uh);
      const u = Math.min(spec.uv[0], spec.uv[0] + uw) - item.rect[0];
      const v = Math.min(spec.uv[1], spec.uv[1] + uh) - item.rect[1];
      f.style.transform = `translate3d(${at.map((n) => `${n}px`).join(',')}) ${transform} ${uw < 0 ? 'scaleX(-1) ' : ''}translate(-50%, -50%)`;
      f.style.backgroundImage = `url(${item.url})`;
      f.style.backgroundSize = `${item.rect[2] * K * sx}px ${item.rect[3] * item.frames * K * sy}px`;
      faces.push([f, -u * K * sx, -v * K * sy, item.rect[3] * K * sy]);
      f.style.backgroundPosition = `${-u * K * sx}px ${-v * K * sy}px`;
    }
    f.style.filter = `brightness(${light})`;
    parent.append(f);
  }
  return faces;
}

const all = (color) => Object.fromEntries(Object.keys(FACES).map((f) => [f, color]));
const BODY = [
  [[-4, 24, -4], [8, 8, 8], '#c89878'],
  [[-4.2, 29, -4.2], [8.4, 3.4, 8.4], '#1d2a44'],
  [[-4, 12, -2], [8, 12, 4], '#24406e'],
  [[-8, 18, -2], [4, 6, 4], '#24406e'],
  [[-8, 12, -2], [4, 6, 4], '#c89878'],
  [[4, 18, -2], [4, 6, 4], '#24406e'],
  [[4, 12, -2], [4, 6, 4], '#c89878'],
  [[-4, 0, -2], [4, 12, 4], '#18223a'],
  [[0, 0, -2], [4, 12, 4], '#1b2742'],
  [[-3, 27, -4.1], [2, 1, 0.1], '#60c0ff'],
  [[1, 27, -4.1], [2, 1, 0.1], '#60c0ff'],
];

// Skin clasica 64x64: [origen, tamano, u, v, capa exterior]
const SKIN = [
  [[-4, 24, -4], [8, 8, 8], 0, 0, 0], [[-4, 24, -4], [8, 8, 8], 32, 0, 0.5],
  [[-4, 12, -2], [8, 12, 4], 16, 16, 0], [[-4, 12, -2], [8, 12, 4], 16, 32, 0.25],
  [[-8, 12, -2], [4, 12, 4], 40, 16, 0], [[-8, 12, -2], [4, 12, 4], 40, 32, 0.25],
  [[4, 12, -2], [4, 12, 4], 32, 48, 0], [[4, 12, -2], [4, 12, 4], 48, 48, 0.25],
  [[-4, 0, -2], [4, 12, 4], 0, 16, 0], [[-4, 0, -2], [4, 12, 4], 0, 32, 0.25],
  [[0, 0, -2], [4, 12, 4], 16, 48, 0], [[0, 0, -2], [4, 12, 4], 0, 48, 0.25],
];
function boxUv([w, h, d], u, v) {
  const f = (x, y, a, b) => ({ uv: [x, y], uv_size: [a, b] });
  return {
    north: f(u + d, v + d, w, h), south: f(u + 2 * d + w, v + d, w, h),
    east: f(u + d + w, v + d, d, h), west: f(u, v + d, d, h),
    up: f(u + d, v, w, d), down: f(u + d + w, v, w, d),
  };
}

export function createStage(host, model) {
  let yaw = 205;
  let goal = null;
  let drag = null;
  let nodes = new Map();
  let animated = [];
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

  const base = document.createElement('div');
  base.className = 'bone pixel';
  for (const [origin, size, color] of BODY) box(base, { origin, size, uv: all(color) });
  const worn = document.createElement('div');
  worn.className = 'bone';
  model.append(base, worn);

  host.onpointerdown = (e) => {
    drag = { x: e.clientX, yaw };
    goal = null;
    host.setPointerCapture(e.pointerId);
  };
  host.onpointermove = (e) => { if (drag) yaw = drag.yaw + (e.clientX - drag.x) * 0.6; };
  host.onpointerup = host.onpointercancel = () => { drag = null; };

  function pose(name, transform) {
    const node = nodes.get(name);
    if (node) node.style.transform = transform;
  }

  function frame(time) {
    if (goal !== null) {
      yaw += (goal - yaw) * 0.12;
      if (Math.abs(goal - yaw) < 0.2) goal = null;
    }
    model.style.transform = `rotateX(-8deg) rotateY(${yaw}deg)`;
    const t = still ? 0 : time / 1000;
    for (let i = 0; i < 8; i++) {
      const wave = Math.sin(t * 2.6 - i * 0.73) * 1.6;
      pose(`lexCape${i}`, `rotateX(${-(i === 0 ? 8 + wave : wave)}deg)`);
    }
    const flap = Math.sin(t * 2) * 7;
    pose('lexWingL', `rotateY(${26 + flap}deg) rotateZ(-6deg)`);
    pose('lexWingL2', `rotateY(${14 + flap}deg)`);
    pose('lexWingR', `rotateY(${-26 - flap}deg) rotateZ(6deg)`);
    pose('lexWingR2', `rotateY(${-14 - flap}deg)`);
    pose('lexDragonL', `rotateY(${26 + flap}deg)`);
    pose('lexDragonL2', `rotateY(${14 + flap}deg)`);
    pose('lexDragonR', `rotateY(${-26 - flap}deg)`);
    pose('lexDragonR2', `rotateY(${-14 - flap}deg)`);
    pose('lexAuraWall', `rotateY(${-t * 22}deg)`);
    pose('lexAuraOrbs', `translateY(${Math.sin(t * 1.6) * 8}px) rotateY(${t * 60}deg)`);
    for (const pet of ['Dragon', 'Lynx', 'Spirit']) pose(`lexPet${pet}`, `translateY(${Math.sin(t * 2.2) * 10}px)`);
    pose('lexPetDragonHead', `rotateY(${Math.sin(t * 0.9) * 14}deg)`);
    pose('lexPetDragonTail', `rotateY(${Math.sin(t * 2.4) * 16}deg)`);
    pose('lexPetDragonL', `rotateZ(${-12 - Math.sin(t * 8) * 30}deg)`);
    pose('lexPetDragonR', `rotateZ(${12 + Math.sin(t * 8) * 30}deg)`);
    pose('lexPetLynxHead', `rotateY(${Math.sin(t * 0.9) * 14}deg)`);
    pose('lexPetLynxTail', `rotateX(34deg) rotateY(${Math.sin(t * 2.6) * 20}deg)`);
    pose('lexPetSpiritOrbs', `rotateY(${t * 130}deg)`);
    pose('lexHalo', `translateY(${Math.sin(t * 2) * 3}px) rotateX(8deg) rotateY(${t * 30}deg)`);
    pose('lexAura', `rotateY(${t * 36}deg)`);
    for (const { item, faces } of animated) {
      const f = still ? 0 : Math.floor(t * item.fps) % item.frames;
      for (const [el, x, y, step] of faces) el.style.backgroundPosition = `${x}px ${y - f * step}px`;
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  return {
    skin(url) {
      if (!url) return;
      base.replaceChildren();
      const item = { rect: [0, 0, 64, 64], frames: 1, url };
      for (const [o, s, u, v, grow] of SKIN) {
        box(base, { origin: o.map((n) => n - grow), size: s.map((n) => n + grow * 2), uv: boxUv(s, u, v) }, item);
      }
    },
    turn(angle) { goal = angle + Math.round((yaw - angle) / 360) * 360; },
    // items: cosmeticos con su url de textura
    wear(items) {
      worn.replaceChildren();
      nodes = new Map();
      animated = [];
      for (const item of items) {
        const faces = [];
        for (const bone of bonesOf(item.id)) {
          const node = document.createElement('div');
          node.className = 'bone';
          const [px, py, pz] = bone.pivot;
          node.style.transformOrigin = `${px * K}px ${-(py - 16) * K}px ${-pz * K}px`;
          for (const c of bone.cubes) faces.push(...box(node, c, item));
          nodes.set(bone.name, node);
          (nodes.get(bone.parent) ?? worn).append(node);
        }
        if (item.frames > 1) animated.push({ item, faces });
      }
    },
  };
}
