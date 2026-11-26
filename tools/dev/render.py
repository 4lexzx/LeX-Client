"""Render de revision: cubos con sombreado plano, vistos desde varios angulos."""
import json, math, sys
import numpy as np
from PIL import Image, ImageDraw
data = json.load(open(sys.argv[1]))
out = sys.argv[2]
FACES = [((0, 1, 3, 2), (-1, 0, 0)), ((4, 6, 7, 5), (1, 0, 0)), ((0, 4, 5, 1), (0, -1, 0)), ((2, 3, 7, 6), (0, 1, 0)), ((0, 2, 6, 4), (0, 0, -1)), ((1, 5, 7, 3), (0, 0, 1))]
L = np.array([0.4, 0.8, -0.45]); L /= np.linalg.norm(L)
tiles = []
for id, cubes in data.items():
    tex = Image.open(f'launcher/assets/cosmetics/{id}.png').convert('RGB')
    W, H = tex.size
    for yaw in (30, 120, 210, 300):
        img = Image.new('RGB', (420, 420), (26, 32, 46))
        d = ImageDraw.Draw(img)
        a, p = math.radians(yaw), math.radians(18)
        R = np.array([[math.cos(a), 0, math.sin(a)], [0, 1, 0], [-math.sin(a), 0, math.cos(a)]])
        P = np.array([[1, 0, 0], [0, math.cos(p), -math.sin(p)], [0, math.sin(p), math.cos(p)]]) @ R
        allp = np.array([c for cu in cubes for c in cu['corners']])
        ctr = allp.mean(0)
        span = (allp.max(0) - allp.min(0)).max()
        k = 360 / span
        polys = []
        for cu in cubes:
            C = (np.array(cu['corners']) - ctr) @ P.T
            col = np.array(tex.getpixel((min(W - 1, int(cu['uv'][0] * W)), min(H - 1, int(cu['uv'][1] * H)))), float)
            for idx, n in FACES:
                nn = P @ np.array(n, float)
                if nn[2] > 0.01:
                    continue
                q = C[list(idx)]
                light = 0.55 + 0.45 * max(0, float(-(P.T @ nn) @ L) if False else float(np.dot(n, L)))
                polys.append((q[:, 2].mean(), [(210 + x * k, 210 - y * k) for x, y in q[:, :2]], tuple((col * light).clip(0, 255).astype(int))))
        for _, pts, c in sorted(polys, key=lambda t: -t[0]):
            d.polygon(pts, fill=c)
        tiles.append(img)
cols = 4
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new('RGB', (cols * 420, rows * 420))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * 420, (i // cols) * 420))
sheet.save(out)
