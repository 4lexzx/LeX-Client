import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { HEADER_ID, PACK_NAME, buildPack, encodePng, findEntry, parseLoose, readParticleIds, readPlayerEntity, setGlobalPack, withLex, writePack } from '../electron/pack.mjs';
import { PRESETS, applyOptions, applyPreset, restore } from '../electron/optimizer.mjs';
import { vanillaPets } from '../electron/vanilla.mjs';
import { displayVersion } from '../electron/game.mjs';
import { ATLAS, BONES, CAPE_PLAIN, CHANNELS_FX, COSMETICS, DENSITY, FILE_DATA, VANILLA_PETS, bonesOf, cleanLoadout, registerFiles } from '../shared/cosmetics.mjs';
import { FILE_AREA, boxUv } from '../shared/filecosmetics.mjs';
import { loadFileCosmetics } from '../electron/filecosmetics.mjs';
import { BASE_H, BASE_W, CHANNELS, COUNTS, CROSSHAIR, REACHES, VALUES } from '../shared/telemetry.mjs';

// Los cosmeticos por archivos entran en todas las pruebas, como en el launcher.
const fileAssets = loadFileCosmetics(new URL('..', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1'));
registerFiles(fileAssets.compiled);

const entity = () => ({
  'minecraft:client_entity': { description: {
    identifier: 'minecraft:player', materials: {}, textures: {}, geometry: {}, animations: {},
    scripts: { animate: ['root'] }, render_controllers: [{ 'controller.render.player.third_person': '1' }],
  } },
});
const pngSize = (b) => [b.readUInt32BE(16), b.readUInt32BE(20)];
const temp = () => mkdtempSync(join(tmpdir(), 'lex-'));

function archive(name, body) {
  const head = Buffer.alloc(16 + 256);
  head.writeBigUInt64LE(0x267052a0b125277dn, 0);
  head.writeUInt32LE(1, 8);
  head[16] = name.length;
  head.write(name, 17);
  head.writeUInt32LE(body.length, 16 + 252);
  return Buffer.concat([head, Buffer.from(body)]);
}

test('brarchive: lee y rechaza datos dañados', () => {
  const a = archive('x.json', '{"a":1}');
  assert.equal(findEntry(a, 'x.json').toString(), '{"a":1}');
  assert.equal(findEntry(a, 'otro'), null);
  assert.throws(() => findEntry(Buffer.alloc(64), 'x'));
  assert.throws(() => findEntry(a.subarray(0, a.length - 2), 'x.json'));
});

test('cada PNG mide lo que declara el catálogo', () => {
  for (const c of COSMETICS.filter((i) => !VANILLA_PETS.includes(i) && !i.file)) {
    const size = pngSize(readFileSync(new URL(`../assets/cosmetics/${c.geo === 'trail' ? `trail-${c.art}` : c.id}.png`, import.meta.url)));
    assert.deepEqual(size, [c.rect[2] * DENSITY, c.rect[3] * DENSITY * c.frames], c.id);
    assert.ok(c.geo === 'trail' || bonesOf(c.id).length > 0, `${c.id} sin geometría`);
  }
});

test('las regiones del atlas no se pisan y caben', () => {
  const rects = [...new Map([...COSMETICS.filter((c) => c.geo !== 'trail'), { rect: CAPE_PLAIN }].map((c) => [String(c.rect), c.rect])).values()];
  for (const [x, y, w, h] of rects) assert.ok(x + w <= ATLAS.w && y + h <= ATLAS.h);
  rects.forEach((a, i) => rects.slice(i + 1).forEach((b) => {
    const overlap = a[0] < b[0] + b[2] && b[0] < a[0] + a[2] && a[1] < b[1] + b[3] && b[1] < a[1] + a[3];
    assert.ok(!overlap, `${a} / ${b}`);
  }));
});

test('la geometría solo usa UV dentro del atlas y padres existentes', () => {
  const names = new Set(['root', 'body', 'head', ...BONES.map((b) => b.name)]);
  for (const b of BONES) {
    assert.ok(b.parent === undefined || names.has(b.parent), b.name);
    for (const c of b.cubes) if (c.size.includes(0)) assert.equal(Object.keys(c.uv).length, 1, `${b.name}: plano con caras superpuestas`);
    for (const c of b.cubes) for (const f of Object.values(c.uv)) {
      const xs = [f.uv[0], f.uv[0] + f.uv_size[0]];
      const ys = [f.uv[1], f.uv[1] + f.uv_size[1]];
      assert.ok(Math.min(...xs) >= 0 && Math.max(...xs) <= ATLAS.w && Math.min(...ys) >= 0 && Math.max(...ys) <= ATLAS.h, b.name);
    }
  }
});

test('el pack conserva al jugador y solo afecta al local', () => {
  const source = entity();
  const files = buildPack(source, { version: [1, 0, 1] });
  const d = JSON.parse(files.get('entity/player.entity.json'))['minecraft:client_entity'].description;
  assert.ok(d.render_controllers.length <= 17, `${d.render_controllers.length} controles por jugador`);
  for (const a of d.scripts.animate.slice(1)) assert.match(Object.values(a)[0], /query\.is_local_player/, 'ninguna animación corre para todos');
  for (const k of CHANNELS_FX) assert.ok(files.has(`particles/lex_fx_${k}.json`));
  for (const rc of d.render_controllers.slice(1)) assert.match(Object.values(rc)[0], /query\.is_local_player/);
  const health = d.render_controllers.find((rc) => rc['controller.render.lex_health']);
  assert.match(Object.values(health)[0], /^!query\.is_local_player/, 'la vida es solo de otros jugadores');
  assert.deepEqual(pngSize(files.get('textures/lex/atlas.png')), [ATLAS.w * DENSITY, ATLAS.h * DENSITY]);
  for (const path of Object.values(d.textures)) assert.ok(files.has(`${path}.png`));
  assert.deepEqual(source, entity(), 'no muta la entidad original');
});

test('telemetría: cada (canal, valor) tiene un tamaño único', () => {
  const files = buildPack(entity(), { version: [1, 0, 1] });
  const seen = new Set();
  for (let ch = 0; ch < CHANNELS.length; ch++) for (let v = 0; v < VALUES; v++) {
    const size = pngSize(files.get(`textures/lex/t/${ch}_${v}.png`));
    assert.deepEqual(size, [BASE_W + ch, BASE_H + v]);
    seen.add(String(size));
  }
  assert.equal(seen.size, CHANNELS.length * VALUES);
});

test('HUD, crosshair y alcance', () => {
  const files = buildPack(entity(), { version: [1, 0, 1] });
  const ui = JSON.parse(files.get('ui/hud_screen.json'));
  assert.equal(ui.namespace, 'hud');
  for (let i = 0; i < COUNTS; i++) assert.ok(files.has(`textures/lex/n/${i}.png`));
  for (let i = 0; i < REACHES; i++) assert.ok(files.has(`textures/lex/t/r${i}.png`));
  assert.deepEqual(pngSize(files.get('textures/ui/cross_hair.png')), [CROSSHAIR, CROSSHAIR]);
});

test('activación automática: primero, sin duplicar y reversible', () => {
  const other = { pack_id: 'otro', version: [1, 0, 0] };
  const once = withLex([other], [1, 0, 1]);
  assert.deepEqual(once.map((p) => p.pack_id), [HEADER_ID, 'otro']);
  assert.equal(withLex(once, [1, 0, 2]).length, 2);
  assert.deepEqual(withLex(once, null), [other]);

  const users = temp();
  const dir = join(users, '1', 'games', 'com.mojang', 'minecraftpe');
  mkdirSync(dir, { recursive: true });
  const path = join(dir, 'global_resource_packs.json');
  writeFileSync(path, '\uFEFF[\n\t{ "pack_id" : "otro", "version" : [ 1, 0, 0 ] }\n]\n');
  assert.equal(setGlobalPack(users, [1, 0, 5]), 1);
  assert.equal(parseLoose(readFileSync(path, 'utf8'))[0].pack_id, HEADER_ID);
  setGlobalPack(users, null);
  assert.deepEqual(parseLoose(readFileSync(path, 'utf8')), [other]);
  writeFileSync(path, '{roto');
  assert.equal(setGlobalPack(users, [1, 0, 6]), 0);
  assert.equal(readFileSync(path, 'utf8'), '{roto', 'no toca un archivo ilegible');
  rmSync(users, { recursive: true });
});

test('optimizador: solo claves conocidas, distancia a la baja, reversible', () => {
  const r = applyOptions('a:1\r\ngfx_vsync:1\r\ngfx_viewdistance:64\r\n', PRESETS.max.options, 96);
  assert.equal(r.text, 'a:1\r\ngfx_vsync:0\r\ngfx_viewdistance:64\r\n');
  const users = temp();
  const dir = join(users, '1', 'games', 'com.mojang', 'minecraftpe');
  mkdirSync(dir, { recursive: true });
  const path = join(dir, 'options.txt');
  const original = 'gfx_vsync:1\ngfx_smoothlighting:1\ngfx_viewdistance:512\naudio_main:0.5\n';
  writeFileSync(path, original);
  applyPreset(users, 'max');
  assert.match(readFileSync(path, 'utf8'), /gfx_vsync:0\ngfx_smoothlighting:0\ngfx_viewdistance:96/);
  applyPreset(users, 'balanced');
  assert.match(readFileSync(path, 'utf8'), /gfx_smoothlighting:1\ngfx_viewdistance:160/);
  writeFileSync(path, readFileSync(path, 'utf8').replace('audio_main:0.5', 'audio_main:0.9'));
  restore(users);
  assert.equal(readFileSync(path, 'utf8'), original.replace('0.5', '0.9'), 'restaura gráficos y respeta lo demás');
  rmSync(users, { recursive: true });
});

test('cleanLoadout descarta ids desconocidos o de otra ranura', () => {
  assert.deepEqual(cleanLoadout({ cape: 'wings-frost', head: 'head-halo', aura: '../x' }), { cape: null, wings: null, head: 'head-halo', pet: null, aura: null, trail: null, hit: null, kill: null, death: null });
});

test('writePack solo borra su propia carpeta', () => {
  const dir = temp();
  writeFileSync(join(dir, 'keep.txt'), 'x');
  assert.throws(() => writePack(dir, new Map()));
  writePack(join(dir, PACK_NAME), new Map([['a/b.json', Buffer.from('1')]]));
  assert.ok(existsSync(join(dir, 'keep.txt')));
  rmSync(dir, { recursive: true });
});

test('versión visible y PNG', () => {
  assert.equal(displayVersion('1.26.6501.0'), '1.26.65.1');
  assert.deepEqual(pngSize(encodePng({ width: 3, height: 2, data: new Uint8ClampedArray(24) })), [3, 2]);
});

const real = 'C:\\XboxGames\\Minecraft for Windows\\Content';
test('instalación real: dragones del juego', { skip: !existsSync(real) }, () => {
  const { bones, images } = vanillaPets(real);
  assert.equal(images.size, VANILLA_PETS.length, [...images.keys()].join());
  const names = new Set(bones.map((b) => b.name));
  for (const b of bones) assert.ok(!b.parent || names.has(b.parent), `${b.name} -> ${b.parent}`);
});

test('instalación real: entidad y partículas', { skip: !existsSync(real) }, () => {
  const ids = readParticleIds(real);
  assert.ok(ids.length > 80, `solo ${ids.length}`);
  const files = buildPack(readPlayerEntity(real), { version: [1, 0, 0], disabledParticles: true, particleIds: ids });
  assert.ok(files.has('particles/lex_0.json'));
});

test('la DLL busca el atlas con el mismo tamaño que genera el launcher', () => {
  const cpp = readFileSync(new URL('../../client/platform/windows/game/Telemetry.cpp', import.meta.url), 'utf8');
  const m = cpp.match(/kLive\[\] = \{\{(\d+) \* 12, (\d+) \* 12\}/);
  assert.ok(m, 'no se encontró kLive en Telemetry.cpp');
  assert.deepEqual([Number(m[1]), Number(m[2])], [ATLAS.w, ATLAS.h]);
  assert.equal(DENSITY, 12);
});

test('cosméticos por archivos: modelo, atlas, animaciones, estados y partículas', () => {
  const items = COSMETICS.filter((c) => c.file);
  assert.ok(items.length >= 3, 'faltan las mascotas por archivos');
  const names = new Set(BONES.map((b) => b.name));
  for (const c of items) {
    const [x, y, w, h] = c.rect;
    assert.ok(x >= FILE_AREA.x && y >= FILE_AREA.y && x + w <= FILE_AREA.x + FILE_AREA.w && y + h <= FILE_AREA.y + FILE_AREA.h, `${c.id}: fuera de su zona`);
    const bones = bonesOf(c.id);
    assert.ok(bones.length > 3, `${c.id}: sin huesos`);
    for (const b of bones) for (const cube of b.cubes) for (const f of Object.values(cube.uv)) {
      const [u, v] = f.uv;
      const [du, dv] = f.uv_size;
      for (const [a, lo, hi] of [[u, x, x + w], [u + du, x, x + w], [v, y, y + h], [v + dv, y, y + h]]) assert.ok(a >= lo - 1e-6 && a <= hi + 1e-6, `${b.name}: UV fuera de su region`);
    }
    // La imagen que recibe la DLL mide exactamente su region del atlas.
    const png = fileAssets.texture(c);
    assert.equal(png.readUInt32BE(16), w * DENSITY);
    assert.equal(png.readUInt32BE(20), h * DENSITY);
  }
  // Cada animacion solo mueve huesos que existen; cada estado, animaciones registradas.
  for (const [key, anim] of Object.entries(FILE_DATA.animations)) for (const bone of Object.keys(anim.bones ?? {})) assert.ok(names.has(bone), `${key}: hueso ${bone}`);
  const registered = new Set(Object.keys(FILE_DATA.entity.animations));
  for (const [ctl, c] of Object.entries(FILE_DATA.controllers)) {
    assert.ok(c.states.idle, `${ctl}: sin reposo`);
    for (const st of Object.values(c.states)) for (const a of st.animations) assert.ok(registered.has(a), `${ctl}: ${a}`);
  }
  // Particulas: textura en un canal que la DLL pinta y flipbook del tamano fijo.
  for (const p of FILE_DATA.particles) {
    const e = p.json.particle_effect;
    assert.equal(e.description.basic_render_parameters.texture, `textures/lex/fp/${p.channel}`);
    const png = fileAssets.particle(p);
    assert.deepEqual([png.readUInt32BE(16), png.readUInt32BE(20)], [512, 64 + p.channel]);
  }
  const files = buildPack(entity(), { version: [1, 0, 1] });
  const d = JSON.parse(files.get('entity/player.entity.json'))['minecraft:client_entity'].description;
  for (const ctl of FILE_DATA.entity.animate) assert.ok(d.scripts.animate.some((a) => a[ctl]), `${ctl} no se anima`);
  for (const p of FILE_DATA.particles) assert.ok(files.has(`particles/lexf_${p.id}_${p.name}.json`));
});

test('UV de caja de Blockbench: tamaños de cada cara', () => {
  const f = boxUv([0, 0], [4, 6, 2]);
  assert.deepEqual(f.north, { uv: [2, 2], uv_size: [4, 6] });
  assert.deepEqual(f.east.uv_size, [2, 6]);
  assert.deepEqual(f.up, { uv: [2, 0], uv_size: [4, 2] });
});

test('kills y muertes: doble con la skin, coreografía elegida, partículas y cuerpo oculto', () => {
  const fx = Object.fromEntries(readdirSync(new URL('../assets/fx', import.meta.url)).map((n) => [n.slice(0, -4), readFileSync(new URL(`../assets/fx/${n}`, import.meta.url))]));
  const files = buildPack(entity(), { version: [1, 0, 1], loadout: { kill: 'kill-execution', death: 'death-implode' }, fxTextures: fx });
  const d = JSON.parse(files.get('entity/player.entity.json'))['minecraft:client_entity'].description;
  // El cuerpo normal se oculta mientras dura la animacion.
  assert.match(Object.values(d.render_controllers[0])[0], /!variable\.lex_fxon/);
  const rc = JSON.parse(files.get('render_controllers/lex.render_controllers.json')).render_controllers['controller.render.lex_clone'];
  assert.deepEqual(rc.textures, ['Texture.default'], 'el doble usa la skin del jugador');
  assert.ok(rc.overlay_color.a.includes('variable.lex_dt'));
  const anims = JSON.parse(files.get('animations/lex.animation.json')).animations;
  assert.ok(anims['animation.lex.kfx'].bones['lexDfxexecution_sword'], 'falta la espada');
  assert.ok(anims['animation.lex.dfx'].bones['lexC_head'], 'falta la coreografia de muerte');
  assert.ok(anims['animation.lex.kfx'].animation_length >= 5, 'menos de 5 s');
  const geo = JSON.parse(files.get('models/entity/lex.geo.json'))['minecraft:geometry'];
  const ids = geo.map((g) => g.description.identifier);
  for (const id of ['geometry.lex.clone', 'geometry.lex.cloneself', 'geometry.lex.killfx', 'geometry.lex.deathfx']) assert.ok(ids.includes(id), id);
  // Cada particula usada existe y tiene su textura.
  const used = new Set(Object.values(anims['animation.lex.kfx'].particle_effects).flat().map((e) => e.effect));
  for (const e of used) {
    assert.ok(d.particle_effects[e], e);
    assert.ok(files.has(`textures/lex/kfx/${e.replace('lex_k_', '')}.png`), e);
  }
});
