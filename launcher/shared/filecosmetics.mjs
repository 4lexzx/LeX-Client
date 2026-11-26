// Cosmeticos por archivos. Cada uno vive en su carpeta (launcher/cosmetics/<id>/):
//
//   cosmetic.json      metadatos: ranura, nombre, posicion, estados, variantes
//   model.geo.json     geometria de Bedrock (Blockbench), UV por cara o caja
//   texture.png        textura en alta resolucion (px por texel libre)
//   animation.json     animaciones de Bedrock (keyframes y/o Molang)
//   particles/*.json   particulas de Bedrock + su flipbook .png (opcional)
//
// Aqui no se lee el disco: electron/filecosmetics.mjs lee las carpetas y pasa
// su contenido. compileAll() devuelve lo que el pack y la DLL necesitan: items
// del catalogo, huesos con UV ya dentro del atlas, animaciones, un controlador
// de estados por cosmetico y las particulas.

export const FILE_AREA = { x: 0, y: 112, w: 161, h: 48 }; // zona del atlas para estos cosmeticos
export const PARTICLE_SLOTS = 8; // texturas de particulas que la DLL puede pintar
export const PARTICLE_SIZE = { w: 512, h: 64, frame: 64 }; // alto real = h + canal
const DENSITY = 12;

const STATES = ['idle', 'walk', 'fly', 'sleep', 'breath'];
const FACES = ['north', 'south', 'east', 'west', 'up', 'down'];

// UV de caja (box UV) de Bedrock/Blockbench -> UV por cara.
export function boxUv([u, v], [w, h, d], mirror = false) {
  const [W, H, Dp] = [w, h, d].map((n) => Math.max(0, n));
  const faces = {
    east: { uv: [u, v + Dp], uv_size: [Dp, H] },
    north: { uv: [u + Dp, v + Dp], uv_size: [W, H] },
    west: { uv: [u + Dp + W, v + Dp], uv_size: [Dp, H] },
    south: { uv: [u + 2 * Dp + W, v + Dp], uv_size: [W, H] },
    up: { uv: [u + Dp, v], uv_size: [W, Dp] },
    down: { uv: [u + Dp + W, v + Dp], uv_size: [W, -Dp] },
  };
  if (mirror) {
    for (const f of ['north', 'south', 'up', 'down']) faces[f] = { uv: [faces[f].uv[0] + faces[f].uv_size[0], faces[f].uv[1]], uv_size: [-faces[f].uv_size[0], faces[f].uv_size[1]] };
    [faces.east, faces.west] = [
      { uv: [faces.west.uv[0] + Dp, faces.west.uv[1]], uv_size: [-Dp, H] },
      { uv: [faces.east.uv[0] + Dp, faces.east.uv[1]], uv_size: [-Dp, H] },
    ];
  }
  return faces;
}

// Coloca rectangulos (en unidades del atlas) por filas dentro de FILE_AREA.
function packer() {
  let x = FILE_AREA.x;
  let y = FILE_AREA.y;
  let row = 0;
  return (w, h) => {
    if (x + w > FILE_AREA.x + FILE_AREA.w) {
      x = FILE_AREA.x;
      y += row + 1;
      row = 0;
    }
    if (y + h > FILE_AREA.y + FILE_AREA.h) throw new Error(`No cabe en el atlas un cosmetico de ${w}x${h}`);
    const rect = [x, y, w, h];
    x += w + 1;
    row = Math.max(row, h);
    return rect;
  };
}

const round = (n) => Math.round(n * 10000) / 10000;

// Una carpeta: { meta, model, animations, particles: [{ name, json, frames, image }], texture: { width, height } }
export function compileAll(sources) {
  const place = packer();
  const items = [];
  const bones = [];
  const animations = {};
  const controllers = {};
  const entity = { animations: {}, animate: [], particles: {} };
  const particleFiles = [];
  let channel = 0;

  for (const src of sources) {
    const { meta, model } = src;
    const id = meta.id;
    if (!/^[a-z0-9]+$/.test(id)) throw new Error(`Id no valido: ${id}`);
    const geo = (model['minecraft:geometry'] ?? [])[0];
    if (!geo) throw new Error(`${id}: model.geo.json sin geometria`);
    const texW = geo.description.texture_width;
    const texH = geo.description.texture_height;
    // Texels -> unidades del atlas, a la resolucion real del PNG.
    const scaleX = src.texture.width / texW / DENSITY;
    const scaleY = src.texture.height / texH / DENSITY;
    const rect = place(Math.ceil(src.texture.width / DENSITY), Math.ceil(src.texture.height / DENSITY));
    const remap = (f) => ({ uv: [round(rect[0] + f.uv[0] * scaleX), round(rect[1] + f.uv[1] * scaleY)], uv_size: [round(f.uv_size[0] * scaleX), round(f.uv_size[1] * scaleY)] });

    // Huesos con nombre propio; la raiz del modelo cuelga de un hueso de LeX
    // colocado donde dice cosmetic.json (junto al hombro, a los pies...).
    const prefix = `lexF${id}`;
    const name = (bone) => `${prefix}_${bone}`;
    const offset = meta.offset ?? [0, 0, 0];
    // scale: el modelo se hace a tamano de mob y aqui se reduce (version mini).
    const S = meta.scale ?? 1;
    const at = (p) => p.map((n, i) => round(n * S + offset[i]));
    const size = (v) => v.map((n) => round(n * S));
    const locators = {};
    bones.push({ name: prefix, pivot: at([0, 0, 0]), cubes: [] });
    for (const b of geo.bones ?? []) {
      const out = {
        name: name(b.name),
        parent: b.parent ? name(b.parent) : prefix,
        pivot: at(b.pivot ?? [0, 0, 0]),
        cubes: (b.cubes ?? []).map((c) => {
          const faces = Array.isArray(c.uv) ? boxUv(c.uv, c.size, c.mirror ?? b.mirror) : c.uv;
          let list = FACES.filter((f) => faces[f]);
          // Un plano (grosor 0) con dos caras parpadea: el juego dibuja ambos lados con una.
          if (c.size.includes(0)) list = list.slice(0, 1);
          const uv = Object.fromEntries(list.map((f) => [f, remap(faces[f])]));
          const cube = { origin: at(c.origin), size: size(c.size), uv };
          if (c.inflate) cube.inflate = round(c.inflate * S);
          if (c.rotation) Object.assign(cube, { rotation: c.rotation, pivot: at(c.pivot ?? [0, 0, 0]) });
          return cube;
        }),
      };
      if (b.rotation) out.rotation = b.rotation;
      // Huesos ocultos en reposo (parpados): las vistas previas no los dibujan.
      if ((meta.hiddenAtRest ?? []).includes(b.name)) out.motion = { scale: 0 };
      if (b.locators) {
        out.locators = {};
        for (const [k, p] of Object.entries(b.locators)) {
          const pos = Array.isArray(p) ? p : p.offset;
          out.locators[`lexf_${id}_${k}`] = at(pos);
          locators[k] = `lexf_${id}_${k}`;
        }
      }
      bones.push(out);
    }

    // Particulas: cada una ocupa un canal que la DLL pinta solo si llevas puesto
    // el cosmetico (sin el, la textura esta vacia y no se ve nada).
    const effects = {};
    for (const p of src.particles ?? []) {
      if (channel >= PARTICLE_SLOTS) throw new Error('Demasiadas particulas de cosmeticos');
      const k = channel++;
      const json = structuredClone(p.json);
      const effect = json.particle_effect;
      effect.description.identifier = `lex:f_${id}_${p.name}`;
      effect.description.basic_render_parameters.texture = `textures/lex/fp/${k}`;
      const billboard = effect.components['minecraft:particle_appearance_billboard'];
      if (billboard) {
        billboard.uv = {
          texture_width: PARTICLE_SIZE.w, texture_height: PARTICLE_SIZE.h + k,
          flipbook: {
            base_UV: [0, 0], size_UV: [PARTICLE_SIZE.frame, PARTICLE_SIZE.frame], step_UV: [PARTICLE_SIZE.frame, 0],
            frames_per_second: p.fps ?? billboard.uv?.flipbook?.frames_per_second ?? 12,
            max_frame: p.frames, stretch_to_lifetime: billboard.uv?.flipbook?.stretch_to_lifetime ?? true, loop: billboard.uv?.flipbook?.loop ?? false,
          },
        };
      }
      const short = `lexf_${id}_${p.name}`;
      entity.particles[short] = effect.description.identifier;
      effects[p.name] = short;
      particleFiles.push({ id, name: p.name, channel: k, json, frames: p.frames });
    }

    // Animaciones: mismos datos con los huesos, particulas y localizadores renombrados.
    const animNames = {};
    for (const [full, anim] of Object.entries(src.animations?.animations ?? {})) {
      const copy = structuredClone(anim);
      // Las posiciones de la animacion estan en unidades del modelo: misma escala.
      const scalePos = (v) => {
        if (typeof v === 'number') return round(v * S);
        if (typeof v === 'string') return S === 1 ? v : `(${v}) * ${S}`;
        if (Array.isArray(v)) return v.map(scalePos);
        if (v && typeof v === 'object') return Object.fromEntries(Object.entries(v).map(([k, x]) => [k, k === 'lerp_mode' ? x : scalePos(x)]));
        return v;
      };
      copy.bones = Object.fromEntries(Object.entries(copy.bones ?? {}).map(([bone, v]) => [name(bone), v.position ? { ...v, position: scalePos(v.position) } : v]));
      if (copy.particle_effects) {
        for (const list of Object.values(copy.particle_effects)) {
          for (const e of Array.isArray(list) ? list : [list]) {
            if (!effects[e.effect]) throw new Error(`${id}: particula desconocida ${e.effect}`);
            e.effect = effects[e.effect];
            if (e.locator) e.locator = locators[e.locator] ?? e.locator;
          }
        }
      }
      const key = `animation.lexf.${id}.${full.split('.').pop()}`;
      animations[key] = copy;
      const short = `lexf_${id}_${full.split('.').pop()}`;
      entity.animations[short] = key;
      animNames[full] = short;
      animNames[full.split('.').pop()] = short;
    }

    // Controlador de estados: reposo, andar, volar, dormir y (si existe) aliento.
    const states = meta.states ?? {};
    const pick = (s) => (states[s] ?? []).map((a) => animNames[a] ?? (() => { throw new Error(`${id}: animacion ${a} no existe`); })());
    const has = (s) => (states[s] ?? []).length > 0;
    const sleepAfter = meta.sleepAfter ?? 20;
    const every = meta.breathEvery ?? 14;
    // Prioridad: te hacen dano > en el aire > agachado > corriendo > andando >
    // dormido > aliento > sentado (quieto un rato) > de pie.
    const sitAfter = meta.sitAfter ?? 4;
    const target = [
      [has('hurt'), 'variable.lex_calm < 1.2', 'hurt'],
      [has('fly'), 'variable.lex_air > 0.5', 'fly'],
      [has('sneak'), 'query.is_sneaking', 'sneak'],
      [has('run'), 'query.is_sprinting && query.ground_speed > 0.6', 'run'],
      [has('walk'), 'query.ground_speed > 0.6', 'walk'],
      [has('sleep'), `variable.lex_still > ${sleepAfter}`, 'sleep'],
      [has('breath'), `math.mod(query.life_time, ${every}) > ${every - (meta.breathTime ?? 2.4)}`, 'breath'],
      [has('sit'), `variable.lex_still > ${sitAfter}`, 'sit'],
    ].filter(([ok]) => ok);
    const when = (state) => {
      // Primera condicion que se cumple, por prioridad; si ninguna, reposo.
      const i = target.findIndex(([, , s]) => s === state);
      if (state === 'idle') return target.map(([, c]) => `!(${c})`).join(' && ') || '1';
      return [...target.slice(0, i).map(([, c]) => `!(${c})`), target[i][1]].join(' && ');
    };
    const used = ['idle', ...target.map(([, , s]) => s)];
    const ctl = `controller.animation.lexf.${id}`;
    controllers[ctl] = {
      initial_state: 'idle',
      states: Object.fromEntries(used.map((s) => [s, {
        // 'always' (p. ej. mirar a donde miras) suena en todos los estados.
        animations: [...pick(s), ...pick('always')],
        blend_transition: meta.blend ?? 0.3,
        transitions: used.filter((o) => o !== s).map((o) => ({ [o]: when(o) })),
      }])),
    };
    entity.animations[`lexf_${id}_ctl`] = ctl;
    entity.animate.push(`lexf_${id}_ctl`);

    // Variantes de color: comparten modelo y region; cambia solo la imagen.
    const variants = meta.variants?.length ? meta.variants : [{ key: '', name: meta.name }];
    for (const v of variants) {
      items.push({
        id: v.key ? `${meta.slot}-${id}-${v.key}` : `${meta.slot}-${id}`,
        slot: meta.slot, name: v.key ? `${meta.name} ${v.name}` : meta.name, rect, frames: 1, fps: 12,
        geo: `file${id}`, file: { id, variant: v.key, hue: v.hue ?? 0, saturation: v.saturation ?? 1, light: v.light ?? 1, texture: v.texture ?? null, keep: meta.keepColors ?? [] },
        particles: particleFiles.filter((p) => p.id === id).map((p) => ({ channel: p.channel, file: `${id}.fp${p.channel}` })),
      });
    }
  }
  return { items, bones, animations, controllers, entity, particles: particleFiles };
}
