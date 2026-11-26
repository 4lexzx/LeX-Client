// Vista 3D del personaje con WebGL. Un solo contexto dibuja el escenario y, por
// turnos, las tarjetas visibles: nunca hay mas de una escena a la vez en la GPU.
import { bonesOf } from '../shared/cosmetics.mjs';

const RAD = Math.PI / 180;

// Mismas convenciones que el cliente (client/platform/windows/gui/Preview.cpp).
function rotate(p, pivot, [rx, ry, rz]) {
  let x = p[0] - pivot[0];
  let y = p[1] - pivot[1];
  let z = p[2] - pivot[2];
  if (rx) { const c = Math.cos(rx * RAD); const s = Math.sin(rx * RAD); [y, z] = [y * c + z * s, z * c - y * s]; }
  if (rz) { const c = Math.cos(rz * RAD); const s = Math.sin(rz * RAD); [x, y] = [x * c - y * s, x * s + y * c]; }
  if (ry) { const c = Math.cos(ry * RAD); const s = Math.sin(ry * RAD); [x, z] = [x * c - z * s, z * c + x * s]; }
  return [x + pivot[0], y + pivot[1], z + pivot[2]];
}

const sin = Math.sin;
const motion = (bone, t) => {
  const m = bone.motion;
  if (!m?.rot) return null;
  // Ejes Y y Z del juego van al reves que en esta vista.
  return m.rot.map((w, k) => (Array.isArray(w) ? w[0] + w[1] * Math.sin((w[2] * t + w[3]) * RAD) + w[4] * t : w) * (k ? -1 : 1));
};
const lift = (bone, t) => {
  const w = bone.motion?.pos?.[1];
  return Array.isArray(w) ? w[0] + w[1] * Math.sin((w[2] * t + w[3]) * RAD) + w[4] * t : 0;
};

function pose(bone, t) {
  if (bone.startsWith('lexCape')) {
    const i = Number(bone.at(-1));
    const wave = sin(t * 2.6 - i * 0.73) * 2.2;
    return [i === 0 ? 9 + wave : wave, 0, 0];
  }
  const flap = sin(t * 2) * 8;
  switch (bone) {
    case 'lexWingL': case 'lexDragonL': return [0, 25 + flap, 0];
    case 'lexWingR': case 'lexDragonR': return [0, -25 - flap, 0];
    case 'lexWingL2': case 'lexDragonL2': return [0, 14 + flap, 0];
    case 'lexWingR2': case 'lexDragonR2': return [0, -14 - flap, 0];
    case 'lexMiniL': return [0, 30 + flap, 0];
    case 'lexMiniR': return [0, -30 - flap, 0];
    case 'lexHalo': return [8, t * 30, 0];
    case 'lexAura': return [0, t * 36, 0];
    case 'lexAuraWall': return [0, -t * 22, 0];
    case 'lexAuraOrbs': return [0, t * 60, 0];
    case 'lexBandA': case 'lexBandB': return [14 + sin(t * 2.4) * 6, 0, 0];
    case 'lexPetSpiritOrbs': return [0, t * 130, 0];
    default:
  }
  if (!bone.startsWith('lexPet')) return null;
  if (bone.startsWith('lexPetDragon_')) {
    const part = bone.slice(13);
    const side = part.endsWith('R') ? -1 : 1;
    const beat = t * 5.2;
    if (part.startsWith('wing')) return [0, 0, side * (-8 + sin(beat) * 34)];
    if (part.startsWith('tip')) return [0, 0, side * (6 + sin(beat - 0.96) * 46)];
    if (part.startsWith('fronttip')) return [-34, 0, 0];
    if (part.startsWith('frontfoot')) return [36, 0, 0];
    if (part.startsWith('front')) return [58, 0, 0];
    if (part.startsWith('reartip')) return [28, 0, 0];
    if (part.startsWith('rearfoot')) return [38, 0, 0];
    if (part.startsWith('rear')) return [62, 0, 0];
    if (part.startsWith('neck')) return [sin(t * 1.4 - part[4] * 0.5) * 4, sin(t * 0.9 - part[4] * 0.4) * 5, 0];
    if (part === 'head') return [6 + sin(t * 1.2) * 6, sin(t * 0.9) * 12, 0];
    if (part === 'jaw') return [8 + Math.max(0, sin(t)) * 14, 0, 0];
    if (part.startsWith('tail')) return [sin(t * 1.7 - part[4] * 0.55) * 5, sin(t * 2.1 - part[4] * 0.62) * 8, 0];
    return null;
  }
  const sit = Math.min(1, Math.max(0, sin(t * 0.55) * 3));
  const walk = sin(t * 6) * 30 * (1 - sit);
  switch (bone) {
    case 'lexPetCat_body': case 'lexPetFox_body': return [-26 * sit, 0, 0];
    case 'lexPetCat_head': case 'lexPetFox_head': return [24 * sit + sin(t) * 5, sin(t * 0.65) * 22, 0];
    case 'lexPetCat_frontLegL': case 'lexPetFox_leg3': return [walk + 26 * sit, 0, 0];
    case 'lexPetCat_frontLegR': case 'lexPetFox_leg2': return [-walk + 26 * sit, 0, 0];
    case 'lexPetCat_backLegL': case 'lexPetFox_leg1': return [-walk - 42 * sit, 0, 0];
    case 'lexPetCat_backLegR': case 'lexPetFox_leg0': return [walk - 42 * sit, 0, 0];
    case 'lexPetCat_tail1': return [62 - 20 * sit + sin(t * 1.4) * 8, sin(t * 1.2) * 16, 0];
    case 'lexPetCat_tail2': return [26 + sin(t * 1.9) * 18, sin(t * 1.6) * 20, 0];
    case 'lexPetFox_tail': return [78 + sin(t * 1.6) * 8, sin(t * 2.1) * 18, 0];
    case 'lexPetParrot_wing0': return [0, 0, 52 - sin(t * 15) * 46];
    case 'lexPetParrot_wing1': return [0, 0, -52 + sin(t * 15) * 46];
    case 'lexPetParrot_body': return [24, 0, 0];
    case 'lexPetParrot_head': return [sin(t * 1.6) * 8, sin(t) * 26, 0];
    case 'lexPetParrot_tail': return [30, 0, 0];
    case 'lexPetBee_rightwing_bone': return [15, 15, -sin(t * 40) * 34];
    case 'lexPetBee_leftwing_bone': return [15, -15, sin(t * 40) * 34];
    case 'lexPetAllay_left_wing': return [0, -40 - sin(t * 12) * 28, 0];
    case 'lexPetAllay_right_wing': return [0, 40 + sin(t * 12) * 28, 0];
    case 'lexPetAllay_body': return [10, 0, 0];
    default: return null;
  }
}

const FACES = ['north', 'south', 'east', 'west', 'up', 'down'];
const SHADE = [1, 0.92, 0.78, 0.78, 1, 0.6];
function corners(face, a, b) {
  switch (face) {
    case 'north': return [[a[0], b[1], a[2]], [b[0], b[1], a[2]], [b[0], a[1], a[2]], [a[0], a[1], a[2]]];
    case 'south': return [[b[0], b[1], b[2]], [a[0], b[1], b[2]], [a[0], a[1], b[2]], [b[0], a[1], b[2]]];
    case 'east': return [[b[0], b[1], a[2]], [b[0], b[1], b[2]], [b[0], a[1], b[2]], [b[0], a[1], a[2]]];
    case 'west': return [[a[0], b[1], b[2]], [a[0], b[1], a[2]], [a[0], a[1], a[2]], [a[0], a[1], b[2]]];
    case 'up': return [[a[0], b[1], b[2]], [b[0], b[1], b[2]], [b[0], b[1], a[2]], [a[0], b[1], a[2]]];
    default: return [[a[0], a[1], a[2]], [b[0], a[1], a[2]], [b[0], a[1], b[2]], [a[0], a[1], b[2]]];
  }
}

const SKIN = [
  [[-4, 24, -4], [8, 8, 8], 0, 0, 0], [[-4, 24, -4], [8, 8, 8], 32, 0, 0.5],
  [[-4, 12, -2], [8, 12, 4], 16, 16, 0], [[-4, 12, -2], [8, 12, 4], 16, 32, 0.25],
  [[-8, 12, -2], [4, 12, 4], 40, 16, 0], [[-8, 12, -2], [4, 12, 4], 40, 32, 0.25],
  [[4, 12, -2], [4, 12, 4], 32, 48, 0], [[4, 12, -2], [4, 12, 4], 48, 48, 0.25],
  [[-4, 0, -2], [4, 12, 4], 0, 16, 0], [[-4, 0, -2], [4, 12, 4], 0, 32, 0.25],
  [[0, 0, -2], [4, 12, 4], 16, 48, 0], [[0, 0, -2], [4, 12, 4], 0, 48, 0.25],
];
const BODY = [];
for (const [o, [w, h, d], u, v, grow] of SKIN) {
  const a = o.map((n) => n - grow);
  const b = [o[0] + w + grow, o[1] + h + grow, o[2] + d + grow];
  const rects = [[u + d, v + d, w, h], [u + 2 * d + w, v + d, w, h], [u + d + w, v + d, d, h], [u, v + d, d, h], [u + d, v, w, d], [u + d + w, v, w, d]];
  FACES.forEach((f, i) => {
    const [x, y, rw, rh] = rects[i].map((n) => n / 64);
    BODY.push({ p: corners(f, a, b), uv: [x, y, x + rw, y + rh], shade: SHADE[i] });
  });
}

const VERTEX = `attribute vec3 p; attribute vec2 t; attribute float s; uniform mat4 m; varying vec2 uv; varying float k;
void main() { gl_Position = m * vec4(p, 1.0); uv = t; k = s; }`;
const FRAGMENT = `precision mediump float; uniform sampler2D tex; varying vec2 uv; varying float k;
void main() { vec4 c = texture2D(tex, uv); if (c.a < 0.5) discard; gl_FragColor = vec4(c.rgb * k, 1.0); }`;

export function createViewer(load) {
  const canvas = document.createElement('canvas');
  const gl = canvas.getContext('webgl', { alpha: true, antialias: true, premultipliedAlpha: true, preserveDrawingBuffer: true });
  const program = gl.createProgram();
  for (const [type, code] of [[gl.VERTEX_SHADER, VERTEX], [gl.FRAGMENT_SHADER, FRAGMENT]]) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, code);
    gl.compileShader(shader);
    gl.attachShader(program, shader);
  }
  gl.linkProgram(program);
  gl.useProgram(program);
  gl.enable(gl.DEPTH_TEST);
  gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
  for (const [name, size, at] of [['p', 3, 0], ['t', 2, 12], ['s', 1, 20]]) {
    const place = gl.getAttribLocation(program, name);
    gl.enableVertexAttribArray(place);
    gl.vertexAttribPointer(place, size, gl.FLOAT, false, 24, at);
  }
  const matrix = gl.getUniformLocation(program, 'm');

  let extra = [];
  let skin = null;
  const textures = new Map(); // id -> { texture, image } | 'cargando'

  // Las animaciones son tiras muy altas (hasta 6912 px); muchas GPU de movil
  // no pasan de 4096 y la textura sale negra. Si no cabe, se reduce entera
  // (las coordenadas son relativas, asi que los fotogramas siguen cuadrando).
  const maxSize = gl.getParameter(gl.MAX_TEXTURE_SIZE) || 4096;
  function fit(image) {
    const w = image.naturalWidth ?? image.width;
    const h = image.naturalHeight ?? image.height;
    const k = Math.min(1, maxSize / Math.max(w, h));
    if (k >= 1) return image;
    const small = document.createElement('canvas');
    small.width = Math.max(1, Math.floor(w * k));
    small.height = Math.max(1, Math.floor(h * k));
    const g = small.getContext('2d');
    g.imageSmoothingEnabled = false;
    g.drawImage(image, 0, 0, small.width, small.height);
    return small;
  }
  function upload(image) {
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, fit(image));
    for (const [k, v] of [[gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE], [gl.TEXTURE_MIN_FILTER, gl.LINEAR], [gl.TEXTURE_MAG_FILTER, gl.NEAREST]]) gl.texParameteri(gl.TEXTURE_2D, k, v);
    return texture;
  }
  async function decode(url) {
    const image = new Image();
    image.src = url;
    await image.decode();
    return image;
  }
  // De una en una para no parar la ventana.
  let queue = Promise.resolve();
  function texture(id) {
    const found = textures.get(id);
    if (found) return found.texture ? found : null;
    textures.set(id, {});
    queue = queue.then(async () => {
      try {
        const image = await decode(await load(id));
        textures.set(id, { texture: upload(image), image });
      } catch { /* sin textura: no se dibuja */ }
      await new Promise((r) => setTimeout(r, 4));
    });
    return null;
  }

  const links = new Map();
  function bones(id) {
    if (!links.has(id)) {
      const list = bonesOf(id, extra);
      const map = new Map(list.map((b) => [b.name, b]));
      links.set(id, list.map((bone) => {
        const chain = [];
        for (let b = bone; b && chain.length < 24; b = map.get(b.parent)) chain.push(b);
        return { bone, chain };
      }));
    }
    return links.get(id);
  }

  function quads(item, t) {
    const out = [];
    const frame = item.frames > 1 ? Math.floor(t * item.fps) % item.frames : 0;
    const [rx, ry, rw, rh] = item.rect;
    // Cosmeticos por archivos: su animacion ya calculada, fotograma a fotograma.
    const anim = item.anim;
    const f = anim ? Math.floor(t * anim.fps) % anim.frames : 0;
    const sample = (b) => anim?.bones[b.name]?.[f];
    for (const { bone, chain } of bones(item.id)) {
      const turns = chain.map((b) => {
        const s = sample(b);
        if (s) return [b.pivot, [s[0], -s[1], -s[2]], 0, [s[3], s[4], s[5]], [s[6], s[7], s[8]]];
        return [b.pivot, motion(b, t) ?? pose(b.name, t), lift(b, t)];
      }).filter(([, r, y, p, sc]) => r || y || p || sc);
      if (chain.some((b) => { const s = sample(b); return s ? s[6] === 0 || s[7] === 0 : b.motion?.scale === 0; })) continue;
      for (const c of bone.cubes ?? []) {
        const b = c.origin.map((n, i) => n + c.size[i]);
        for (const [face, spec] of Object.entries(c.uv)) {
          const p = corners(face, c.origin, b).map((v) => {
            let q = c.rotation ? rotate(v, c.pivot, [c.rotation[0], -c.rotation[1], -c.rotation[2]]) : v;
            for (const [pivot, r, y, pos, sc] of turns) {
              if (sc) q = q.map((n, i) => pivot[i] + (n - pivot[i]) * sc[i]);
              if (r) q = rotate(q, pivot, r);
              if (y) q = [q[0], q[1] + y, q[2]];
              if (pos) q = [q[0] + pos[0], q[1] + pos[1], q[2] + pos[2]];
            }
            return q;
          });
          const x0 = (spec.uv[0] - rx) / rw;
          const x1 = (spec.uv[0] + spec.uv_size[0] - rx) / rw;
          const y0 = ((spec.uv[1] - ry) / rh + frame) / item.frames;
          const y1 = ((spec.uv[1] + spec.uv_size[1] - ry) / rh + frame) / item.frames;
          out.push({ p, uv: [x0, y0, x1, y1], shade: SHADE[FACES.indexOf(face)] });
        }
      }
    }
    return out;
  }

  let data = new Float32Array(6 * 6 * 512);
  function batch(list, tex) {
    if (!list.length) return;
    if (data.length < list.length * 36) data = new Float32Array(list.length * 36);
    let n = 0;
    for (const q of list) {
      const [u0, v0, u1, v1] = q.uv;
      const uv = [[u0, v0], [u1, v0], [u1, v1], [u0, v1]];
      for (const i of [0, 1, 2, 0, 2, 3]) {
        data[n++] = q.p[i][0]; data[n++] = q.p[i][1]; data[n++] = q.p[i][2];
        data[n++] = uv[i][0]; data[n++] = uv[i][1]; data[n++] = q.shade;
      }
    }
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.bufferData(gl.ARRAY_BUFFER, data.subarray(0, n), gl.STREAM_DRAW);
    gl.drawArrays(gl.TRIANGLES, 0, n / 6);
  }

  // Camara en perspectiva suave: igual que la vista previa del juego.
  function camera(yaw, pitch, centerY, scale, w, h, cx = 0, cz = 0) {
    const c = Math.cos(yaw * RAD);
    const s = Math.sin(yaw * RAD);
    const pc = Math.cos(pitch * RAD);
    const ps = Math.sin(pitch * RAD);
    const sx = (2 * scale) / w;
    const sy = (2 * scale) / h;
    const D = 90;
    // x' = x c + z s ; z0 = -x s + z c ; y' = (y-cy) pc + z0 ps ; z' = z0 pc - (y-cy) ps ; w = (D + z') / D
    const near = -80;
    const far = 90;
    const zs = 2 / (far - near);
    // (cx, cz): punto del suelo que queda en el centro (por defecto, el jugador).
    const k0 = cx * s - cz * c;
    const row = [
      [c * sx, 0, s * sx, -(cx * c + cz * s) * sx],
      [-s * ps * sy, pc * sy, c * ps * sy, (-centerY * pc + k0 * ps) * sy],
      [-s * pc, -ps, c * pc, centerY * ps + k0 * pc],
    ];
    const z = row[2];
    const depth = [z[0] * zs, z[1] * zs, z[2] * zs, z[3] * zs - (far + near) / (far - near)];
    const persp = [z[0] / D, z[1] / D, z[2] / D, 1 + z[3] / D];
    const out = new Float32Array(16);
    [row[0], row[1], depth, persp].forEach((r, i) => r.forEach((v, j) => { out[j * 4 + i] = v; }));
    return out;
  }

  return {
    setExtra(list) { extra = list; links.clear(); },
    async setSkin(url) {
      if (!url) return;
      try { skin = upload(await decode(url)); } catch { /* skin ilegible */ }
    },
    hasSkin: () => Boolean(skin),
    texture,
    // Dibuja y copia en el canvas 2D de destino. Falso si aun falta una textura.
    draw(target, items, { yaw = 25, head = false, wide = false, time = 0, focus = false } = {}) {
      const w = target.width;
      const h = target.height;
      if (canvas.width < w || canvas.height < h) {
        canvas.width = Math.max(canvas.width, w);
        canvas.height = Math.max(canvas.height, h);
      }
      gl.viewport(0, canvas.height - h, w, h);
      gl.enable(gl.SCISSOR_TEST);
      gl.scissor(0, canvas.height - h, w, h);
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      let scale = Math.min(w / (head ? 17 : wide ? 42 : 19), h / (head ? 18 : 40));
      let view = [head ? 30 : 18, 0, 0];
      if (focus) {
        // El cosmetico solo, centrado y a su tamano (mascotas, animaciones de muerte).
        const lo = [Infinity, Infinity, Infinity];
        const hi = [-Infinity, -Infinity, -Infinity];
        for (const item of items) for (const { bone } of bones(item.id)) for (const c of bone.cubes ?? []) {
          for (let i = 0; i < 3; i++) {
            lo[i] = Math.min(lo[i], c.origin[i]);
            hi[i] = Math.max(hi[i], c.origin[i] + c.size[i]);
          }
        }
        if (lo[0] < Infinity) {
          const span = Math.max(hi[0] - lo[0], hi[2] - lo[2]);
          scale = Math.min(w / (span * 1.05 + 3), h / (hi[1] - lo[1] + span * 0.25 + 4));
          view = [(lo[1] + hi[1]) / 2, (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2];
        }
      }
      gl.uniformMatrix4fv(matrix, false, camera(yaw, head ? 10 : focus ? 26 : 12, view[0], scale, w, h, view[1], view[2]));
      if (skin && !focus) batch(BODY, skin);
      let ready = Boolean(skin) || focus;
      for (const item of items) {
        const entry = texture(item.id);
        if (!entry) { ready = false; continue; }
        batch(quads(item, time), entry.texture);
      }
      const ctx = target.getContext('2d');
      ctx.clearRect(0, 0, w, h);
      ctx.drawImage(canvas, 0, 0, w, h, 0, 0, w, h);
      return ready;
    },
    // Particulas: el dibujo de 8 fotogramas subiendo en bucle.
    sparks(target, item, time) {
      const entry = texture(item.id);
      if (!entry) return false;
      const ctx = target.getContext('2d');
      ctx.clearRect(0, 0, target.width, target.height);
      for (let i = 0; i < 14; i++) {
        const t = (time * 0.7 + i * 0.618) % 1;
        const x = target.width * (0.14 + 0.72 * (Math.abs(Math.sin(i * 12.9898) * 437.58) % 1));
        const y = target.height * (0.94 - 0.82 * t);
        const k = target.width * (0.13 + 0.05 * ((i * 0.53) % 1));
        ctx.drawImage(entry.image, Math.min(7, Math.floor(t * 8)) * 33, 0, 33, 36, x - k / 2, y - k / 2, k, k * 1.09);
      }
      return true;
    },
  };
}
