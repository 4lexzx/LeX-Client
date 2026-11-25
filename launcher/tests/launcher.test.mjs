import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { HEADER_ID, PACK_NAME, buildPack, encodePng, findEntry, parseLoose, readParticleIds, readPlayerEntity, setGlobalPack, withLex, writePack } from '../electron/pack.mjs';
import { PRESETS, applyOptions, applyPreset, restore } from '../electron/optimizer.mjs';
import { displayVersion } from '../electron/game.mjs';
import { ATLAS, BONES, CAPE_PLAIN, COSMETICS, DENSITY, TRAILS, bonesOf, cleanLoadout } from '../shared/cosmetics.mjs';
import { BASE_H, BASE_W, CHANNELS, COUNTS, CROSSHAIR, REACHES, VALUES } from '../shared/telemetry.mjs';

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
  for (const c of COSMETICS) {
    const size = pngSize(readFileSync(new URL(`../assets/cosmetics/${c.id}.png`, import.meta.url)));
    assert.deepEqual(size, [c.rect[2] * DENSITY, c.rect[3] * DENSITY * c.frames], c.id);
    assert.ok(c.slot === 'trail' || bonesOf(c.id).length > 0, `${c.id} sin geometría`);
  }
});

test('las regiones del atlas no se pisan y caben', () => {
  const rects = [...new Map([...COSMETICS.filter((c) => c.slot !== 'trail'), { rect: CAPE_PLAIN }].map((c) => [String(c.rect), c.rect])).values()];
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
  assert.ok(d.render_controllers.length <= 13, `${d.render_controllers.length} controles por jugador`);
  for (const a of d.scripts.animate.slice(1)) assert.match(Object.values(a)[0], /query\.is_local_player/, 'ninguna animación corre para todos');
  for (const k of TRAILS) assert.ok(files.has(`particles/lex_trail_${k}.json`));
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
  assert.deepEqual(cleanLoadout({ cape: 'wings-frost', head: 'head-halo', aura: '../x' }), { cape: null, wings: null, head: 'head-halo', pet: null, aura: null, trail: null });
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
test('instalación real: entidad y partículas', { skip: !existsSync(real) }, () => {
  const ids = readParticleIds(real);
  assert.ok(ids.length > 80, `solo ${ids.length}`);
  const files = buildPack(readPlayerEntity(real), { version: [1, 0, 0], disabledParticles: true, particleIds: ids });
  assert.ok(files.has('particles/lex_0.json'));
});
