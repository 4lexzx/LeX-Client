// Vuelca los cubos de un cosmetico ya colocados (pose de reposo) para revisarlos.
import { writeFileSync } from 'node:fs';
import { bonesOf, find } from '../../launcher/shared/cosmetics.mjs';
const RAD = Math.PI / 180;
function rotate(p, pivot, [rx, ry, rz]) {
  let [x, y, z] = [p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]];
  if (rx) { const c = Math.cos(rx * RAD), s = Math.sin(rx * RAD); [y, z] = [y * c + z * s, z * c - y * s]; }
  if (rz) { const c = Math.cos(rz * RAD), s = Math.sin(rz * RAD); [x, y] = [x * c - y * s, x * s + y * c]; }
  if (ry) { const c = Math.cos(ry * RAD), s = Math.sin(ry * RAD); [x, z] = [x * c - z * s, z * c + x * s]; }
  return [x + pivot[0], y + pivot[1], z + pivot[2]];
}
const out = {};
for (const id of process.argv.slice(3)) {
  const item = find(id);
  const bones = bonesOf(id);
  const map = new Map(bones.map((b) => [b.name, b]));
  const cubes = [];
  for (const b of bones) {
    if (b.motion?.scale === 0) continue;
    const chain = [];
    for (let k = b; k; k = map.get(k.parent)) chain.push(k);
    const rot = (k) => (k.motion?.rot ?? [0, 0, 0]).map((w, i) => (Array.isArray(w) ? w[0] : w) * (i ? -1 : 1));
    for (const c of b.cubes) {
      const f = c.uv.north ?? Object.values(c.uv)[0];
      const corners = [];
      for (const dx of [0, 1]) for (const dy of [0, 1]) for (const dz of [0, 1]) {
        let p = [c.origin[0] + dx * c.size[0], c.origin[1] + dy * c.size[1], c.origin[2] + dz * c.size[2]];
        for (const k of chain) p = rotate(p, k.pivot, rot(k));
        corners.push(p);
      }
      cubes.push({ corners, uv: [(f.uv[0] - item.rect[0] + f.uv_size[0] / 2) / item.rect[2], (f.uv[1] - item.rect[1] + f.uv_size[1] / 2) / (item.rect[3])] });
    }
  }
  out[id] = cubes;
}
writeFileSync(process.argv[2], JSON.stringify(out));
