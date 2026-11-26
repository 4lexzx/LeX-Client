"""Genera las texturas HD originales de LeX (python tools/make_cosmetics.py).

Cada PNG cubre la region de su ranura en el atlas (launcher/shared/cosmetics.mjs)
a 12 px por unidad. Las animadas apilan fotogramas en vertical.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

D = 12
SS = 4  # supermuestreo
ROOT = os.path.join(os.path.dirname(__file__), '..', 'launcher')
OUT = os.path.join(ROOT, 'assets', 'cosmetics')
os.makedirs(OUT, exist_ok=True)
TAU = math.tau


def rgb(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], float)


def lerp(a, b, t):
    t = np.clip(t, 0, 1)[..., None]
    return a * (1 - t) + b * t


def ramp(t, stops):
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,))
    for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
        m = (t >= p0) & (t <= p1)
        out[m] = lerp(rgb(c0), rgb(c1), (t[m] - p0) / (p1 - p0))
    return out


_rng = np.random.default_rng(7)
_grid = _rng.random((64, 64))


def vnoise(x, y):
    """Ruido de valor suave y periodico (64)."""
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    g = lambda i, j: _grid[j % 64, i % 64]
    return (g(x0, y0) * (1 - fx) + g(x0 + 1, y0) * fx) * (1 - fy) + (g(x0, y0 + 1) * (1 - fx) + g(x0 + 1, y0 + 1) * fx) * fy


def fbm(x, y, o=4):
    s, a, n = 0, 0.5, 0
    for _ in range(o):
        s, n = s + a * vnoise(x, y), n + a
        x, y, a = x * 2 + 7.3, y * 2 + 3.1, a / 2
    return s / n


# --- capas: frente 10x16 en (0,0), forro en (10,0), canto 1x16 en (20,0) ---
CW, CH = 10 * D, 16 * D
Y, X = np.mgrid[0:CH, 0:CW].astype(float)
U, V = X / CW, Y / CH
logo = Image.open(os.path.join(ROOT, 'renderer', 'logo-mark.png')).convert('RGBA')


def cloth(img):
    """Pliegues verticales y borde rematado."""
    fold = 1 + 0.09 * np.sin(U * 9.5 + 0.6) + 0.04 * np.sin(U * 21 + V * 2)
    img = img * (fold * (1.06 - 0.22 * V))[..., None]
    return np.clip(img, 0, 255)


def trim(img, color, w=5):
    e = np.minimum.reduce([X, CW - 1 - X, CH - 1 - Y])
    img = np.where((e < w)[..., None], rgb(color) * (0.75 + 0.35 * (e / w))[..., None], img)
    return np.where(((e >= w) & (e < w + 1.5))[..., None], img * 0.55, img)


def paste_logo(img, size, cy, tint=None, glow=None):
    base = Image.fromarray(img.astype(np.uint8)).convert('RGBA')
    mark = logo.resize((size, size), Image.LANCZOS)
    if tint is not None:
        a = np.array(mark).astype(float)
        lum = a[..., :3].max(-1) / 255
        a[..., :3] = lerp(rgb(tint) * 0.35, rgb(tint), lum ** 0.8)
        mark = Image.fromarray(a.astype(np.uint8))
    pos = ((CW - size) // 2, cy - size // 2)
    if glow is not None:
        g = Image.new('RGBA', base.size)
        g.paste(Image.new('RGBA', mark.size, tuple(int(v) for v in rgb(glow)) + (150,)), pos, mark)
        base = Image.alpha_composite(base, g.filter(ImageFilter.GaussianBlur(7)))
    base.alpha_composite(mark, pos)
    return np.array(base)[..., :3].astype(float)


def cape_sheet(front, lining):
    sheet = np.zeros((CH, 21 * D, 4), np.uint8)
    sheet[..., 3] = 255
    sheet[:, :CW, :3] = np.clip(front, 0, 255)
    inner = lerp(rgb(lining), rgb(lining) * 0.5, V) * (1 + 0.08 * np.sin(U * 11))[..., None]
    sheet[:, CW:2 * CW, :3] = np.clip(inner, 0, 255)
    sheet[:, 2 * CW:, :3] = np.clip(front[:, :D], 0, 255)
    return sheet


def save(name, frames):
    Image.fromarray(np.concatenate(frames, 0)).save(os.path.join(OUT, name + '.png'), optimize=True)
    print(name, len(frames))


N = 24
PH = [i / N * TAU for i in range(N)]


def stars(p, density=0.986):
    s = np.zeros_like(U)
    r = np.random.default_rng(3)
    for _ in range(46):
        x, y, k = r.random() * CW, r.random() * CH, r.random()
        d = (X - x) ** 2 + (Y - y) ** 2
        s += np.exp(-d / (0.5 + 1.6 * k * k)) * (0.55 + 0.45 * math.sin(p + k * TAU))
    return np.clip(s, 0, 1)


def capes():
    base = ramp(V, [(0, 0x0d1830), (0.6, 0x16305c), (1, 0x0a1224)])
    diag = (np.sin((U * 10 + V * 16) * 2.2) > 0.93) * 0.10
    save('cape-lynx', [cape_sheet(trim(paste_logo(cloth(base * (1 + diag)[..., None]), 92, 84, glow=0x2f7fd0), 0x60c0ff), 0x0c1830)])

    noir = ramp(V, [(0, 0x1a1a1d), (1, 0x0b0b0d)])
    save('cape-gold', [cape_sheet(trim(paste_logo(cloth(noir), 92, 84, tint=0xffcf5a), 0xd9a53a), 0x3a0f14)])

    fr = []
    for p in PH:
        g = np.zeros((CH, CW, 3)) + ramp(V, [(0, 0x050816), (0.7, 0x0b1a3a), (1, 0x123a3f)])
        for k, (c, y0) in enumerate([(0x2ef2a6, 0.34), (0x36b6ff, 0.46), (0xa05bff, 0.25)]):
            cen = y0 + 0.07 * np.sin(U * 5 + p + k * 2) + 0.03 * np.sin(U * 11 - p * 2 + k)
            band = np.exp(-np.clip(cen - V, 0, 1) * 5.5) * (V < cen) + np.exp(-((V - cen) / 0.025) ** 2)
            band *= 0.55 + 0.45 * np.sin(U * 34 + k * 5 + 3 * np.sin(p + U * 4))
            g += rgb(c) * np.clip(band, 0, 1)[..., None] * 0.75
        g += 235 * (stars(p) * (V < 0.75))[..., None]
        hill = 0.80 + 0.06 * np.sin(U * 6 + 1) + 0.02 * np.sin(U * 23)
        g = np.where((V > hill)[..., None], rgb(0x04070d), g)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x0b1a30))
    save('cape-aurora', fr)

    fr = []
    for p in PH:
        n = fbm(U * 4 + 0.5 * math.cos(p), V * 7 + 0.5 * math.sin(p), 5)
        arm = np.sin(np.arctan2(V - 0.45, (U - 0.5) * 0.62) * 2 + np.hypot((U - 0.5) * 0.62, V - 0.45) * 17 - p)
        r = np.hypot((U - 0.5) * 0.62, V - 0.45)
        t = np.clip(n * 0.75 + arm * 0.16 * np.exp(-r * 3) - r * 0.35 + 0.1, 0, 1)
        g = ramp(t, [(0, 0x05030f), (0.28, 0x1b0f45), (0.5, 0x6a2bb0), (0.72, 0xff5fb0), (1, 0xfff0d0)])
        g += rgb(0xffe9c8) * np.exp(-(r / 0.07) ** 2)[..., None] + 255 * stars(p)[..., None]
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x160a35))
    save('cape-galaxy', fr)

    fr = []
    for i in range(N):
        y = V * 6 + i / N * 8  # desplaza un multiplo del periodo del ruido sobre 8 celdas
        y2 = V * 6 + (i / N + 1) * 8 - 16
        w = i / N
        n = fbm(U * 5, y, 4) * (1 - w) + fbm(U * 5, y - 8, 4) * w
        heat = V * 1.25 - 0.28 + (n - 0.5) * 0.95
        g = ramp(heat, [(0, 0x120606), (0.25, 0x3a0c08), (0.45, 0xb3260a), (0.65, 0xff7a14), (0.85, 0xffd04a), (1, 0xfff6cf)])
        fr.append(cape_sheet(trim(np.clip(g, 0, 255), 0x2a0f0a, 3), 0x2a0f0a))
    save('cape-inferno', fr)

    fr = []
    for p in PH:
        hue = (U * 0.35 + V * 0.55 + p / TAU) % 1
        k = np.stack([np.abs(((hue + s) % 1) * 6 - 3) - 1 for s in (0, 2 / 3, 1 / 3)], -1)
        g = np.clip(k, 0, 1) * 205 + 35
        fr.append(cape_sheet(trim(paste_logo(cloth(g), 84, 84, tint=0xffffff), 0x101218, 4), 0x14161c))
    save('cape-rgb', fr)

    # Vidriera: celdas de Voronoi con plomo.
    r = np.random.default_rng(11)
    pts = np.stack([r.random(46) * CW, r.random(46) * CH], -1)
    dist = np.sqrt((X[..., None] - pts[:, 0]) ** 2 + (Y[..., None] - pts[:, 1]) ** 2)
    o = np.sort(dist, -1)
    cell = dist.argmin(-1)
    pal = np.array([rgb(c) for c in (0xd7263d, 0xf4a259, 0x2a9d8f, 0x1d6fb8, 0x7b2cbf, 0xf1c453)])
    g = pal[cell % 6] * (0.8 + 0.4 * (cell % 5) / 5)[..., None] * (1.12 - 0.5 * np.clip(o[..., 0] / 34, 0, 1))[..., None]
    g = np.where((o[..., 1] - o[..., 0] < 3.2)[..., None], rgb(0x17181c), g)
    save('cape-glass', [cape_sheet(trim(np.clip(g, 0, 255), 0x23252b), 0x1b1c22)])

    # Cerezo con petalos que caen.
    fr = []
    tree = Image.new('RGBA', (CW * SS, CH * SS))
    d = ImageDraw.Draw(tree)

    def branch(x, y, a, l, w, depth):
        x2, y2 = x + math.cos(a) * l, y - math.sin(a) * l
        d.line([(x, y), (x2, y2)], fill=(74, 46, 48, 255), width=max(2, int(w)))
        if depth:
            for s in (-0.5, 0.42):
                branch(x2, y2, a + s, l * 0.74, w * 0.66, depth - 1)

    branch(CW * SS * 0.5, CH * SS, math.pi / 2, CH * SS * 0.30, 34, 5)
    rr = np.random.default_rng(5)
    for _ in range(620):
        a, q = rr.random() * TAU, rr.random() ** 0.6
        x, y = CW * SS * (0.5 + 0.46 * q * math.cos(a)), CH * SS * (0.33 + 0.25 * q * math.sin(a))
        s = 7 + rr.random() * 13
        c = [(255, 190, 214), (255, 158, 196), (255, 221, 234), (243, 128, 176)][rr.integers(4)]
        d.ellipse([x - s, y - s, x + s, y + s], fill=c + (255,))
    tree = np.array(tree.resize((CW, CH), Image.LANCZOS)).astype(float)
    for i in range(N):
        g = ramp(V, [(0, 0xbfe3f5), (0.6, 0xfbe4ea), (1, 0xf6cfd8)])
        g = g * (1 - tree[..., 3:] / 255) + tree[..., :3] * (tree[..., 3:] / 255)
        q = np.random.default_rng(9)
        for _ in range(16):
            x0, y0, k = q.random(), q.random(), q.integers(1, 3)
            yy = (y0 + k * i / N) % 1
            xx = x0 + 0.05 * math.sin(yy * 9 + x0 * 6)
            m = np.exp(-(((U - xx) * CW / 2.6) ** 2 + ((V - yy) * CH / 1.8) ** 2) ** 2)
            g = lerp(g, rgb(0xff86b4), m)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0xd88aa5))
    save('cape-cherry', fr)


# --- utilidades de dibujo ---
WN = 30 * D  # alas: 30x30 con la raiz en el borde izquierdo


def canvas(n=WN, m=None):
    im = Image.new('RGBA', (n * SS, (m or n) * SS))
    return im, ImageDraw.Draw(im)


def cut(a):
    a = np.array(a)
    a[..., 3] = np.where(a[..., 3] > 127, 255, 0)  # material con recorte, sin semitransparencias
    return a


def finish(im, n=WN, m=None):
    return cut(im.resize((n, m or n), Image.LANCZOS))


def P(*pts, s=WN * SS):
    return [(x * s, y * s) for x, y in pts]


def bez(p0, p1, p2, n=24):
    return [((1 - t) ** 2 * p0[0] + 2 * t * (1 - t) * p1[0] + t * t * p2[0], (1 - t) ** 2 * p0[1] + 2 * t * (1 - t) * p1[1] + t * t * p2[1]) for t in np.linspace(0, 1, n)]


def col(c, k=1.0):
    return tuple(int(min(255, max(0, v * k))) for v in rgb(c)) + (255,)


# --- alas de dragon: membrana con puntas, venas y bordes; hueso aparte ---
def dragon(name, skin, deep, bone, vein, glow, tear=False):
    im, d = canvas()
    # La muneca coincide con los huesos 3D: x=13/30, y=(32-28.6)/30.
    root, wrist = (0.0, 0.42), (0.40, 0.115)
    tips = [(0.985, 0.155), (0.935, 0.50), (0.665, 0.845), (0.33, 0.965)]
    edge = []
    for a, b in zip(tips, tips[1:]):
        mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        pull = (mid[0] + (wrist[0] - mid[0]) * 0.46, mid[1] + (wrist[1] - mid[1]) * 0.46)
        edge += bez(a, pull, b)
    edge += bez(tips[-1], (0.16, 0.70), (0.0, 0.66))
    poly = bez(root, (0.16, 0.20), wrist) + bez(wrist, (0.72, 0.06), tips[0]) + edge
    d.polygon(P(*poly), fill=(255, 255, 255, 255))
    if tear:
        for x, y, r in ((0.62, 0.40, 0.022), (0.50, 0.66, 0.026), (0.78, 0.36, 0.016), (0.30, 0.72, 0.018)):
            d.polygon(P((x - r, y - r * 1.8), (x + r * 1.3, y), (x + r * 0.2, y + r * 2.4), (x - r * 1.2, y + r * 0.6)), fill=(0, 0, 0, 0))
    small = im.resize((WN, WN), Image.LANCZOS)
    mask = np.array(small)[..., 3] / 255
    inner = np.array(small.filter(ImageFilter.GaussianBlur(6)))[..., 3] / 255
    yy, xx = np.mgrid[0:WN, 0:WN] / WN
    dist = np.hypot(xx - wrist[0], yy - wrist[1])
    ang = np.arctan2(yy - wrist[1], xx - wrist[0])
    t = np.clip(dist * 1.25 + (fbm(xx * 14, yy * 14) - 0.5) * 0.5, 0, 1)
    img = lerp(rgb(skin), rgb(deep), t)
    img *= (0.86 + 0.2 * np.cos(ang * 13) ** 2 * np.clip(dist * 2.2, 0, 1))[..., None]  # pliegues radiales
    img *= (0.94 + 0.12 * fbm(xx * 60, yy * 60, 2))[..., None]
    veins = np.abs(fbm(xx * 10, yy * 10, 3) - 0.5) < 0.011
    img = np.where(veins[..., None], lerp(img, rgb(vein), np.full(t.shape, 0.55)), img)
    rim = np.clip((0.86 - inner) * 3.2, 0, 1) * mask
    img = lerp(img, rgb(glow), rim * 0.55)
    a = np.dstack([np.clip(img, 0, 255), mask * 255]).astype(np.uint8)
    over, o = canvas()
    for tp in tips:
        c = bez(wrist, ((wrist[0] + tp[0]) / 2 + 0.03, (wrist[1] + tp[1]) / 2 - 0.03), tp, 40)
        for i, (p, q) in enumerate(zip(c, c[1:])):
            w = max(2, int((13 - 12 * i / 40) * SS / 2))
            o.line(P(p, q), fill=col(bone), width=w)
            o.line(P((p[0], p[1] - 0.004), (q[0], q[1] - 0.004)), fill=col(bone, 1.7), width=max(1, w // 3))
    out = Image.alpha_composite(Image.fromarray(a), over.resize((WN, WN), Image.LANCZOS))
    sheet = np.zeros((WN, 34 * D, 4), np.uint8)
    sheet[:, :WN] = cut(out)
    # Hueso: estrias, brillo superior y punta clara al final (garra).
    bw = 4 * D
    by, bx = np.mgrid[0:WN, 0:bw]
    shade = 0.75 + 0.5 * np.exp(-((bx / bw - 0.32) / 0.22) ** 2) + 0.10 * np.sin(by * 0.55 + np.sin(bx * 0.2) * 2)
    shade += 0.12 * (fbm(bx / 9.0, by / 9.0) - 0.5)
    b = rgb(bone)[None, None] * shade[..., None]
    claw = by >= 22 * D
    b = np.where(claw[..., None], rgb(0xe9e2d0)[None, None] * shade[..., None], b)
    sheet[:, WN:, :3] = np.clip(b, 0, 255)
    sheet[:, WN:, 3] = 255
    save(name, [sheet])


# --- alas de plumas: todas las puntas quedan dentro del lienzo ---
def feathers(name, light, dark, shine, edge=None, rows=3, flare=1.0):
    im, d = canvas()
    L, K = rgb(light), rgb(dark)

    def feather(x, y, tx, ty, w, t):
        dx, dy = tx - x, ty - y
        ln = math.hypot(dx, dy)
        c, s = dx / ln, dy / ln
        # Ensancha pronto y acaba en punta fina.
        prof = [(q, w * (math.sin(math.pi * q ** 0.62) ** 0.8)) for q in np.linspace(0, 1, 26)]
        bend = 0.05 * ln
        def at(q, off):
            b = bend * math.sin(math.pi * q)
            return (x + q * ln * c - (off + b) * s, y + q * ln * s + (off + b) * c)
        poly = [at(q, p) for q, p in prof] + [at(q, -p) for q, p in reversed(prof)]
        base = K * (1 - t) + L * t
        d.polygon(P(*[(px + 0.003, py + 0.006) for px, py in poly]), fill=tuple(int(v * 0.5) for v in base) + (255,))
        d.polygon(P(*poly), fill=tuple(int(v) for v in base) + (255,))
        half = [at(q, p * 0.96) for q, p in prof] + [at(q, 0) for q, _ in reversed(prof)]
        d.polygon(P(*half), fill=tuple(int(v * 0.90) for v in base) + (255,))
        spine = [at(q, 0) for q in np.linspace(0, 0.97, 20)]
        d.line(P(*spine), fill=tuple(int(v * 0.72) for v in base) + (255,), width=5)
        for q in np.linspace(0.2, 0.85, 9):
            p = w * (math.sin(math.pi * q ** 0.62) ** 0.8)
            for sgn in (1, -1):
                d.line(P(at(q, 0), at(min(1, q + 0.07), sgn * p * 0.86)), fill=tuple(int(v * 0.82) for v in base) + (255,), width=2)
        if edge is not None:
            tip = [at(q, p) for q, p in prof if q >= 0.80] + [at(q, -p) for q, p in reversed(prof) if q >= 0.80]
            if len(tip) > 2:
                d.polygon(P(*tip), fill=col(edge))

    def top(q):
        return (0.015 + 0.70 * q, 0.44 - 0.30 * math.sin(q * math.pi * 0.58) ** 0.9)

    # Puntas objetivo: de la raiz (abajo) a la mano (afuera), siempre dentro.
    def tip(q, k):
        a = math.radians(100 - 92 * q ** 1.05)
        r = (0.40 + 0.13 * math.sin(q * math.pi * 0.9)) * k * flare
        x, y = top(q)
        return (min(0.975, max(0.02, x + r * math.cos(a))), min(0.975, y + r * math.sin(a)))

    layers = [(17, 1.30, 0.030, 0.12), (15, 0.86, 0.030, 0.5), (13, 0.50, 0.027, 0.9)][:rows]
    for n, k, w, tt in layers:
        for i in range(n):
            q = i / (n - 1)
            x, y = top(q)
            tx, ty = tip(q, k)
            feather(x, y, tx, ty, w, min(1, tt + 0.10 * (i % 2)))
    for i, q in enumerate(np.linspace(0, 1, 30)):
        x, y = top(q)
        feather(x - 0.012, y - 0.004, *[a * 0.22 + b * 0.78 for a, b in zip(tip(q, 1), (x, y))], 0.020, 1)
    # Borde superior redondeado que cubre las raices.
    arm = [top(q) for q in np.linspace(0, 1, 40)]
    d.line(P(*arm), fill=tuple(int(v) for v in L) + (255,), width=int(0.030 * WN * SS), joint='curve')
    for x, y in arm[::3]:
        r = 0.015 * WN * SS
        d.ellipse([x * WN * SS - r, y * WN * SS - r, x * WN * SS + r, y * WN * SS + r], fill=tuple(int(v) for v in L) + (255,))
    a = np.array(im.resize((WN, WN), Image.LANCZOS))
    yy, xx = np.mgrid[0:WN, 0:WN] / WN
    a[..., :3] = np.clip(a[..., :3] + rgb(shine) * (np.clip(0.45 - yy, 0, 1) * 0.30)[..., None], 0, 255)
    save(name, [cut(a)])


def butterfly():
    fr = []
    im, d = canvas()
    up = bez((0.0, 0.46), (0.10, 0.0), (0.70, 0.04)) + bez((0.70, 0.04), (1.06, 0.10), (0.80, 0.44)) + bez((0.80, 0.44), (0.5, 0.56), (0.0, 0.50))
    lo = bez((0.0, 0.46), (0.5, 0.46), (0.66, 0.60)) + bez((0.66, 0.60), (0.76, 0.90), (0.46, 0.90)) + [(0.40, 0.975), (0.36, 0.90)] + bez((0.36, 0.90), (0.14, 0.86), (0.0, 0.52))
    d.polygon(P(*up), fill=(255, 255, 255, 255))
    d.polygon(P(*lo), fill=(255, 255, 255, 255))
    small = im.resize((WN, WN), Image.LANCZOS)
    full = np.array(small)[..., 3]
    inner = np.array(small.filter(ImageFilter.GaussianBlur(10)))[..., 3] / 255
    yy, xx = np.mgrid[0:WN, 0:WN] / WN
    ang, rad = np.arctan2(yy - 0.48, xx), np.hypot(xx, yy - 0.48)
    for p in PH[::2]:
        t = rad * 1.1 + 0.08 * np.sin(ang * 14) + 0.06 * math.sin(p)
        img = ramp(t, [(0, 0x063a6b), (0.35, 0x1177d1), (0.7, 0x35d0ff), (1, 0xb7f4ff)])
        img = np.where((np.abs(np.sin(ang * 9)) < 0.07)[..., None], rgb(0x06121f), img)
        img = np.where((inner < 0.83)[..., None], rgb(0x060d18), img)
        for (cx, cy, r) in ((0.80, 0.17, 0.030), (0.86, 0.27, 0.024), (0.72, 0.10, 0.022), (0.55, 0.82, 0.030), (0.44, 0.85, 0.022)):
            img = np.where((np.hypot(xx - cx, yy - cy) < r)[..., None], rgb(0xf7fbff), img)
        fr.append(np.dstack([np.clip(img, 0, 255), np.where(full > 127, 255, 0)]).astype(np.uint8))
    save('wings-morpho', fr)


def simple(name, w, h, draw, frames=1):
    out = []
    for i in range(frames):
        im = Image.new('RGBA', (w * D * SS, h * D * SS))
        draw(ImageDraw.Draw(im), w * D * SS, h * D * SS, i / frames, im)
        out.append(finish(im, w * D, h * D))
    save(name, out)


def grad_fill(im, stops):
    """Degradado vertical respetando el alfa dibujado."""
    a = np.array(im).astype(float)
    v = np.linspace(0, 1, a.shape[0])[:, None] * np.ones((1, a.shape[1]))
    a[..., :3] = a[..., :3] / 255 * ramp(v, stops)
    im.paste(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)))


def hue_rgb(h):
    return tuple(int(255 * max(0, min(1, abs(((h + s) % 1) * 6 - 3) - 1))) for s in (0, 2 / 3, 1 / 3))


# --- bandanas HD: frente 20x5.2 y tela 20x5.2, cintas y canto debajo ---
def bandana(name, c, mark=None, trimc=None, frames=1, rainbow=False):
    w, h = 40 * D, 8 * D
    fh = int(6.2 * D)
    out = []
    for i in range(frames):
        yy, xx = np.mgrid[0:h, 0:w].astype(float)
        base = np.zeros((h, w, 3)) + rgb(c)
        if rainbow:
            hue = (xx / (20 * D) + i / frames) % 1
            base = np.stack([np.clip(np.abs(((hue + s) % 1) * 6 - 3) - 1, 0, 1) for s in (0, 2 / 3, 1 / 3)], -1) * 200 + 35
        weave = 1 + 0.05 * np.sin(xx * 1.57) * np.sin(yy * 1.57)
        fold = 1 + 0.10 * np.sin(yy / fh * 9 + np.sin(xx / 46.0) * 0.9) + 0.05 * (fbm(xx / 26, yy / 10) - 0.5)
        light = 1.10 - 0.28 * np.abs(yy / fh - 0.42)
        img = base * (weave * fold * light)[..., None]
        if trimc is not None:
            stripe = ((np.abs(yy - 5) < 2) | (np.abs(yy - (fh - 6)) < 2)) & (yy < fh)
            img = np.where(stripe[..., None], rgb(trimc) * weave[..., None], img)
        hem = ((yy < 2) | ((yy > fh - 3) & (yy <= fh)))
        img = np.where(hem[..., None], img * 0.62, img)
        if trimc is not None:  # cintas con punta de color
            tail = (yy >= 6.4 * D) & (xx < 12 * D) & (xx > 10.6 * D)
            img = np.where(tail[..., None], rgb(trimc), img)
        im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).convert('RGBA')
        if mark:
            big = logo.crop(logo.getbbox())
            mh = fh - 8
            mw = int(big.width * mh / big.height)
            m = big.resize((mw, mh), Image.LANCZOS)
            pos = (int(10 * D - mw / 2), 4)
            if mark == 'glow':
                g = Image.new('RGBA', im.size)
                g.paste(Image.new('RGBA', m.size, (70, 170, 255, 150)), pos, m)
                im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(4)))
            if mark == 'plate':
                dd = ImageDraw.Draw(im)
                dd.ellipse([10 * D - mh * 0.62, 2, 10 * D + mh * 0.62, fh - 3], fill=(18, 26, 44, 255))
            im.alpha_composite(m, pos)
        a = np.array(im)
        a[..., 3] = 255
        out.append(a)
    save(name, out)


def heads():
    bandana('head-band', 0xc2182f, trimc=0xffe1c4)
    bandana('head-bandlex', 0x121317, mark='glow', trimc=0x2f9bff)
    bandana('head-bandblue', 0x15479c, mark='plate', trimc=0x9fdcff)
    bandana('head-bandwhite', 0xe9ecf2, mark='plate', trimc=0x2c6fd6)
    bandana('head-bandrgb', 0xffffff, mark='plate', frames=24, rainbow=True)

    def halo(dark, mid, hi):
        def f(d, w, h, t, im):
            for k, (r0, r1, c) in enumerate([(0.49, 0.36, dark), (0.475, 0.375, mid), (0.45, 0.40, hi)]):
                d.ellipse([w * (0.5 - r0), h * (0.5 - r0), w * (0.5 + r0), h * (0.5 + r0)], fill=c + (255,))
                d.ellipse([w * (0.5 - r1), h * (0.5 - r1), w * (0.5 + r1), h * (0.5 + r1)], fill=(dark if k < 2 else mid) + (255,))
            d.ellipse([w * .14, h * .14, w * .86, h * .86], fill=(0, 0, 0, 0))
            for i in range(8):
                a = TAU * (i / 8 + t / 8)
                x, y, s = w / 2 + math.cos(a) * w * .425, h / 2 + math.sin(a) * h * .425, w * .022
                d.ellipse([x - s, y - s, x + s, y + s], fill=(255, 255, 255, 255))
        return f
    simple('head-halo', 12, 12, halo((196, 132, 30), (255, 208, 92), (255, 244, 196)), 8)
    simple('head-halodark', 12, 12, halo((52, 8, 70), (150, 40, 220), (236, 150, 255)), 8)

    def crown(stops, band, gems, flower=False):
        def f(d, w, h, t, im):
            n = 5
            if flower:
                d.rectangle([0, h * .70, w, h * .90], fill=(64, 118, 48, 255))
                for i in range(14):
                    x = w * i / 14
                    d.ellipse([x, h * .58, x + w * .08, h * .98], fill=(86, 150, 60, 255) if i % 2 else (58, 108, 44, 255))
                for i in range(5):
                    cx, cy, r = w * (i + .5) / 5, h * (.60 + .06 * (i % 2)), h * .20
                    c = [(255, 120, 170), (255, 214, 90), (245, 245, 255), (190, 130, 255), (255, 110, 96)][i]
                    for k in range(6):
                        a = TAU * k / 6
                        x, y = cx + math.cos(a) * r * .72, cy + math.sin(a) * r * .72
                        d.ellipse([x - r * .58, y - r * .58, x + r * .58, y + r * .58], fill=c + (255,), outline=tuple(int(v * .72) for v in c) + (255,), width=4)
                    d.ellipse([cx - r * .42, cy - r * .42, cx + r * .42, cy + r * .42], fill=(255, 190, 40, 255))
                return
            pts = [(0, h), (0, h * .30)]
            for i in range(n):
                x0 = w * i / n
                pts += [(x0 + w / n * .25, h * .52), (x0 + w / n * .5, h * .06), (x0 + w / n * .75, h * .52), (x0 + w / n, h * .30)]
            d.polygon(pts + [(w, h)], fill=(255, 255, 255, 255))
            grad_fill(im, stops)
            d.rectangle([0, h * .66, w, h * .70], fill=band)
            d.rectangle([0, h * .94, w, h], fill=band)
            for i in range(n):
                cx, cy, s = w * (i + .5) / n, h * .82, h * .085
                c = gems[i % 2]
                d.polygon([(cx, cy - s), (cx + s, cy), (cx, cy + s), (cx - s, cy)], fill=c + (255,))
                d.polygon([(cx, cy - s), (cx + s * .5, cy - s * .2), (cx - s * .5, cy - s * .2)], fill=tuple(min(255, v + 90) for v in c) + (255,))
                x, y, r = w * (i + .5) / n, h * .08, h * .07
                d.ellipse([x - r, y - r + r * .3, x + r, y + r + r * .3], fill=(255, 250, 235, 255))
        return f
    simple('head-crown', 10, 6, crown([(0, 0xfff3b0), (0.5, 0xffc93c), (0.8, 0xe0951a), (1, 0xa86210)], (150, 84, 10, 255), [(216, 40, 62), (46, 128, 232)]))
    simple('head-crownice', 10, 6, crown([(0, 0xffffff), (0.5, 0xa8e8ff), (0.8, 0x56b8f0), (1, 0x2a6cb8)], (38, 92, 160, 255), [(120, 220, 255), (255, 255, 255)]))
    simple('head-flowers', 10, 6, crown([], None, [], flower=True))

    def horns(stops):
        def f(d, w, h, t, im):
            d.rectangle([0, 0, w, h], fill=(255, 255, 255, 255))
            grad_fill(im, stops)
            for i in range(14):
                y = h * i / 14
                d.line([(0, y), (w * .5, y + h * .02), (w, y)], fill=(14, 8, 14, 255), width=int(h * .010))
        return f
    simple('head-horns', 8, 8, horns([(0, 0xff6a3a), (0.25, 0x8e1c24), (0.6, 0x3d1220), (1, 0x1d0c16)]))
    simple('head-hornsend', 8, 8, horns([(0, 0xe6e0ee), (0.3, 0x9a8fb0), (0.6, 0x3a3048), (1, 0x15111c)]))

    def hat(d, w, h, t, im):
        u = w / 14
        d.rectangle([0, 0, w, 14 * u], fill=(24, 24, 30, 255))
        d.rectangle([u * .5, u * .5, w - u * .5, 13.5 * u], fill=(34, 34, 42, 255))
        d.rectangle([0, 14 * u, 8 * u, 22 * u], fill=(30, 30, 38, 255))
        for i in range(40):
            x = 8 * u * i / 40
            k = 14 * (i % 3 == 0)
            d.line([(x, 14 * u), (x, 22 * u)], fill=(30 + k, 30 + k, 40 + k, 255), width=4)
        d.rectangle([0, 19 * u, 8 * u, 20.8 * u], fill=(96, 192, 255, 255))
        d.rectangle([0, 19 * u, 8 * u, 19.2 * u], fill=(190, 230, 255, 255))
        d.rectangle([8 * u, 14 * u, w, 20 * u], fill=(38, 38, 46, 255))
    simple('head-hat', 14, 22, hat)

    def wizard(d, w, h, t, im):
        u = w / 20
        d.rectangle([0, 0, w, h], fill=(255, 255, 255, 255))
        grad_fill(im, [(0, 0x3a2a86), (0.65, 0x2b1d6b), (1, 0x1b1148)])
        d.rectangle([0, 0, 16 * u, 16 * u], outline=(255, 206, 84, 255), width=int(u * .5))
        d.rectangle([0, 18.4 * u, 8 * u, 20 * u], fill=(255, 206, 84, 255))
        r = np.random.default_rng(4)
        for _ in range(26):
            x, y, s = r.random() * 16 * u, r.random() * h, u * (0.16 + 0.3 * r.random())
            p = [(x + s * (1 if k % 2 == 0 else .4) * math.cos(TAU * k / 10 - 1.57), y + s * (1 if k % 2 == 0 else .4) * math.sin(TAU * k / 10 - 1.57)) for k in range(10)]
            d.polygon(p, fill=(255, 232, 150, 255))
    simple('head-wizard', 20, 24, wizard)

    def ears(outer, inner):
        def f(d, w, h, t, im):
            d.rectangle([0, 0, w, h], fill=outer + (255,))
            for i in range(60):
                x = w * ((i * 37) % 60) / 60
                d.line([(x, h), (x + w * .02, h * ((i * 13) % 50) / 60)], fill=tuple(int(v * (.82 + .3 * (i % 2))) for v in outer) + (255,), width=3)
            d.polygon([(w * .26, h), (w * .5, h * .30), (w * .74, h)], fill=inner + (255,))
        return f
    simple('head-cat', 4, 5, ears((34, 36, 46), (255, 140, 178)))
    simple('head-fox', 4, 5, ears((216, 112, 40), (250, 236, 214)))

    def phones(d, w, h, t, im):
        d.rectangle([0, 0, w, h], fill=(26, 28, 36, 255))
        d.rectangle([0, 0, w, h * .3], fill=(46, 50, 62, 255))
        c = hue_rgb(t)
        d.ellipse([w * .12, h * .40, w * .68, h * .96], fill=c + (255,))
        d.ellipse([w * .20, h * .48, w * .60, h * .88], fill=(26, 28, 36, 255))
        d.ellipse([w * .32, h * .60, w * .48, h * .76], fill=c + (255,))
    simple('head-phones', 8, 8, phones, 24)

    def goggles(d, w, h, t, im):
        u = w / 20
        d.rectangle([0, 0, w, h], fill=(70, 44, 28, 255))
        d.rectangle([0, 0, 8 * u, 6 * u], fill=(196, 150, 70, 255))
        d.rounded_rectangle([.8 * u, .8 * u, 7.2 * u, 5.2 * u], radius=u, fill=(26, 60, 92, 255))
        d.polygon([(1.4 * u, 5 * u), (4.4 * u, 1 * u), (5.6 * u, 1 * u), (2.6 * u, 5 * u)], fill=(120, 210, 255, 255))
        d.polygon([(5.2 * u, 5 * u), (6.8 * u, 2.6 * u), (6.8 * u, 4 * u), (6.2 * u, 5 * u)], fill=(120, 210, 255, 255))
        d.rectangle([9 * u, 0, 13 * u, 4 * u], fill=(176, 130, 58, 255))
        d.rectangle([0, 6 * u, w, h], fill=(64, 40, 26, 255))
        for i in range(50):
            x = w * i / 50
            d.line([(x, 6.5 * u), (x + u * .15, 6.5 * u)], fill=(212, 176, 120, 255), width=5)
            d.line([(x, 7.5 * u), (x + u * .15, 7.5 * u)], fill=(212, 176, 120, 255), width=5)
    simple('head-goggles', 20, 8, goggles)


# --- mascotas ---
def noise_fill(w, h, c, amount=0.25, scale=6.0, scales=False):
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    k = 1 + amount * (fbm(xx / scale, yy / scale) - 0.5) * 2
    if scales:
        row = np.floor(yy / 8)
        cx = (xx + (row % 2) * 6) % 12 - 6
        cy = yy % 8 - 2
        k *= np.where((cx / 6) ** 2 + (cy / 6.5) ** 2 > 0.9, 0.72, 1.0)
    return np.clip(rgb(c)[None, None] * k[..., None], 0, 255)


def pet_lynx(name, fur, light, ink, eye):
    w, h = 32 * D, 24 * D
    a = np.zeros((h, w, 4), np.uint8)
    a[..., 3] = 255

    def put(x, y, ww, hh, px):
        a[y * D:(y + hh) * D, x * D:(x + ww) * D, :3] = px.astype(np.uint8)
    put(0, 0, 32, 24, noise_fill(w, h, fur, 0.2, 4))
    put(8, 0, 8, 16, noise_fill(8 * D, 16 * D, light, 0.1, 4))
    put(0, 16, 4, 4, noise_fill(4 * D, 4 * D, ink, 0.1))
    im = Image.fromarray(a)
    d = ImageDraw.Draw(im)
    r = np.random.default_rng(2)
    for _ in range(26):  # motas
        x, y = r.random() * 7.4 * D, r.random() * 7.4 * D
        d.ellipse([x, y, x + 8, y + 6], fill=col(ink, 1.2))
    # cara 12x10 en (16,0)
    X0, W2, H2 = 16 * D, 12 * D, 10 * D
    d.polygon([(X0 + W2 * .30, H2 * .98), (X0 + W2 * .24, H2 * .62), (X0 + W2 * .5, H2 * .46), (X0 + W2 * .76, H2 * .62), (X0 + W2 * .70, H2 * .98)], fill=col(light))
    for sx in (0.28, 0.72):
        cx, cy = X0 + W2 * sx, H2 * .44
        s = 1 if sx < .5 else -1
        d.polygon([(cx - s * 20, cy - 8), (cx + s * 16, cy + 6), (cx + s * 12, cy + 16), (cx - s * 10, cy + 12)], fill=col(ink))
        d.polygon([(cx - s * 13, cy - 2), (cx + s * 11, cy + 7), (cx + s * 8, cy + 12), (cx - s * 7, cy + 8)], fill=col(eye))
    for k in (-1, 0, 1):
        d.line([(X0 + W2 * .5 + k * 14, 4), (X0 + W2 * .5 + k * 8, H2 * .24)], fill=col(ink), width=5)
    # hocico
    d.rectangle([16 * D, 11 * D, 22 * D, 15 * D], fill=col(light))
    d.polygon([(17.6 * D, 11.4 * D), (20.4 * D, 11.4 * D), (19 * D, 13 * D)], fill=col(ink))
    d.line([(19 * D, 13 * D), (19 * D, 14 * D)], fill=col(ink), width=4)
    d.arc([17.2 * D, 13 * D, 19 * D, 14.8 * D], 0, 180, fill=col(ink), width=4)
    d.arc([19 * D, 13 * D, 20.8 * D, 14.8 * D], 0, 180, fill=col(ink), width=4)
    d.rectangle([24 * D, 11 * D, 28 * D, 15 * D], fill=col(fur))
    d.polygon([(24.6 * D, 15 * D), (26 * D, 11.8 * D), (27.4 * D, 15 * D)], fill=(255, 168, 180, 255))
    save(name, [np.array(im)])


def pet_spirit():
    fr = []
    w = 16 * D
    for i in range(12):
        p = i / 12 * TAU
        yy, xx = np.mgrid[0:w, 0:w] / float(w)
        t = np.clip(0.45 + 0.35 * np.sin(yy * 9 - p) * np.cos(xx * 7 + p) + 0.25 * (fbm(xx * 8, yy * 8) - 0.5), 0, 1)
        img = ramp(t, [(0, 0x1a6fd0), (0.5, 0x4cc4ff), (1, 0xd6f6ff)])
        im = Image.fromarray(np.dstack([img, np.full((w, w), 255)]).astype(np.uint8))
        d = ImageDraw.Draw(im)
        blink = 2 if i == 7 else 14
        for x in (26, 64):
            d.ellipse([x - 7, 44 - blink, x + 7, 44 + blink], fill=(8, 24, 52, 255))
            if blink > 4:
                d.ellipse([x - 1, 34, x + 5, 41], fill=(255, 255, 255, 255))
        d.arc([38, 52, 54, 66], 10, 170, fill=(8, 24, 52, 255), width=4)
        d.rectangle([8 * D, 8 * D, 16 * D, 16 * D], fill=(226, 250, 255, 255))
        fr.append(np.array(im))
    save('pet-spirit', fr)


# --- auras: suelo 32x32, muro 32x12 y cristal 4x4 ---
def auras():
    def floor(c1, c2, pts, glyph):
        def f(d, w, t):
            def ring(r, wd, c):
                d.ellipse([w * (.5 - r), w * (.5 - r), w * (.5 + r), w * (.5 + r)], outline=c + (255,), width=int(w * wd))
            ring(.49, .012, c1)
            ring(.455, .006, c2)
            ring(.37, .009, c1)
            ring(.20, .008, c2)
            for k in range(2):
                p = [(w * (.5 + .37 * math.cos(TAU * (i / pts + k / (2 * pts)))), w * (.5 + .37 * math.sin(TAU * (i / pts + k / (2 * pts))))) for i in range(pts)]
                d.line(p + [p[0], p[1]], fill=(c1 if k else c2) + (255,), width=int(w * .007))
            for i in range(glyph):
                a = TAU * i / glyph
                on = (math.sin(a * 2 - t * TAU) + 1) / 2
                x, y, s = w * (.5 + .413 * math.cos(a)), w * (.5 + .413 * math.sin(a)), w * .022
                c = tuple(int(p * on + q * (1 - on)) for p, q in zip((255, 255, 255), c1))
                d.line([(x + s * math.cos(a + o), y + s * math.sin(a + o)) for o in (0, 2.0, 4.1, 5.2)], fill=c + (255,), width=int(w * .008))
        return f

    def wall_runes(c1, c2):
        def f(d, w, h, t):
            d.rectangle([0, h * .93, w, h], fill=c1 + (255,))
            d.rectangle([0, h * .90, w, h * .915], fill=c2 + (255,))
            r = np.random.default_rng(8)
            for i in range(16):
                cx = w * (i + .5) / 16
                y0 = (r.random() - t * (1 + i % 2)) % 1
                for k in range(2):
                    y = ((y0 + k * .5) % 1) * h * .86
                    s = h * .085 * (0.5 + y / h)
                    pts = [(cx + s * math.cos(a), y + s * math.sin(a)) for a in r.random(4) * TAU]
                    d.line(pts, fill=c2 + (255,), width=int(h * .02))
                d.line([(cx + w / 32, h * .93), (cx + w / 32, h * (.45 + .25 * math.sin(i * 2.4 + t * TAU)))], fill=c1 + (255,), width=int(h * .012))
        return f

    def wall_flames(cs):
        def f(d, w, h, t):
            for layer, (c, k) in enumerate(zip(cs, (1.0, 0.72, 0.42))):
                pts = [(0, h)]
                n = 160
                for j in range(n + 1):
                    u = j / n
                    y = 0
                    for q, (fq, sp) in enumerate(((8, 1), (16, 2), (28, 3))):
                        y += abs(math.sin(u * fq * math.pi + (layer + q) * 1.3 + (math.sin(t * TAU * sp + q * 2) * .5))) ** 1.6 / (q + 1.4)
                    pts.append((u * w, h * (1 - min(.97, k * (.22 + .46 * y)))))
                d.polygon(pts + [(w, h)], fill=c + (255,))
        return f

    def make(name, floor_fn, wall_fn, gem):
        out = []
        for i in range(12):
            t = i / 12
            a = np.zeros((48 * D, 32 * D, 4), np.uint8)
            im = Image.new('RGBA', (32 * D * SS, 32 * D * SS))
            floor_fn(ImageDraw.Draw(im), 32 * D * SS, t)
            a[:32 * D] = finish(im, 32 * D, 32 * D)
            im = Image.new('RGBA', (32 * D * SS, 12 * D * SS))
            wall_fn(ImageDraw.Draw(im), 32 * D * SS, 12 * D * SS, t)
            a[32 * D:44 * D] = finish(im, 32 * D, 12 * D)
            yy, xx = np.mgrid[0:4 * D, 0:4 * D] / (4.0 * D)
            k = 0.75 + 0.25 * np.sin((xx + yy) * 5 + t * TAU)
            facet = np.where(xx + yy < 1, 1.15, 0.8)
            a[44 * D:48 * D, :4 * D, :3] = np.clip(rgb(gem)[None, None] * (k * facet)[..., None] + 40 * (np.abs(xx - yy) < .08)[..., None], 0, 255)
            a[44 * D:48 * D, :4 * D, 3] = 255
            out.append(a)
        save(name, out)

    make('aura-rune', floor((80, 176, 255), (170, 226, 255), 3, 24), wall_runes((58, 140, 240), (170, 226, 255)), 0x62c0ff)
    make('aura-ember', floor((255, 120, 40), (255, 208, 120), 5, 20), wall_flames([(190, 44, 12), (255, 122, 24), (255, 220, 110)]), 0xff8a2c)
    make('aura-void', floor((176, 88, 255), (232, 186, 255), 4, 28), wall_flames([(52, 14, 92), (128, 48, 214), (222, 160, 255)]), 0xb060ff)
    make('aura-frost', floor((140, 220, 255), (255, 255, 255), 6, 18), wall_runes((120, 206, 255), (255, 255, 255)), 0xd2f4ff)


# --- estelas: 8 fotogramas de 33x36 ---
def trails():
    FW, FH = 33, 36
    K = 8

    def make(name, draw):
        sheet = Image.new('RGBA', (FW * 8, FH))
        for i in range(8):
            im = Image.new('RGBA', (FW * K, FH * K))
            draw(ImageDraw.Draw(im), FW * K, FH * K, i / 7, im)
            sheet.paste(im.resize((FW, FH), Image.LANCZOS), (i * FW, 0))
        save(name, [np.array(sheet)])

    def blob(d, cx, cy, r, c, a=255):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c + (a,))

    def fire(d, w, h, t, im):
        s = 1 - t * .75
        for k, c in ((1, (200, 40, 8)), (.72, (255, 120, 20)), (.42, (255, 226, 120))):
            r = w * .40 * s * k
            cy = h * .62
            d.polygon([(w / 2 - r, cy), (w / 2 - r * .55, cy - r * 1.0), (w / 2 + r * .1, cy - r * 1.9), (w / 2 + r * .5, cy - r * .9), (w / 2 + r, cy)], fill=c + (255,))
            blob(d, w / 2, cy, r, c)
    make('trail-fire', fire)

    def bolt(d, w, h, t, im):
        r = np.random.default_rng(int(t * 7) + 20)
        if int(t * 7) % 3 == 2:
            return
        pts = [(w * (.5 + (r.random() - .5) * .6), h * j / 5) for j in range(6)]
        d.line(pts, fill=(90, 160, 255, 255), width=int(w * .13), joint='curve')
        d.line(pts, fill=(235, 246, 255, 255), width=int(w * .055), joint='curve')
        d.line([pts[2], (w * r.random(), h * .72)], fill=(160, 210, 255, 255), width=int(w * .04))
    make('trail-bolt', bolt)

    def water(d, w, h, t, im):
        r = w * .30 * (1 - t * .6)
        d.polygon([(w / 2, h * .5 - r * 1.9), (w / 2 + r, h * .5), (w / 2 - r, h * .5)], fill=(46, 136, 232, 255))
        blob(d, w / 2, h * .5 + r * .1, r, (46, 136, 232))
        blob(d, w / 2 - r * .25, h * .5 - r * .05, r * .62, (110, 194, 255))
        blob(d, w / 2 - r * .35, h * .5 - r * .25, r * .22, (255, 255, 255))
    make('trail-water', water)

    def frost(d, w, h, t, im):
        R = w * .44 * (1 - t * .55)
        for k in range(6):
            a = TAU * k / 6 + t
            x, y = w / 2 + R * math.cos(a), h / 2 + R * math.sin(a)
            d.line([(w / 2, h / 2), (x, y)], fill=(214, 244, 255, 255), width=int(w * .07))
            for q in (.55, .8):
                bx, by = w / 2 + R * q * math.cos(a), h / 2 + R * q * math.sin(a)
                for s in (.8, -.8):
                    d.line([(bx, by), (bx + R * .26 * math.cos(a + s), by + R * .26 * math.sin(a + s))], fill=(160, 220, 255, 255), width=int(w * .045))
    make('trail-frost', frost)

    def hearts(d, w, h, t, im):
        s = w * .42 * (1 - t * .6)
        for k, c in ((1, (140, 12, 40)), (.84, (240, 48, 84))):
            p = []
            for j in range(60):
                a = TAU * j / 60
                p.append((w / 2 + s * k * (16 * math.sin(a) ** 3) / 17, h / 2 - s * k * (13 * math.cos(a) - 5 * math.cos(2 * a) - 2 * math.cos(3 * a) - math.cos(4 * a)) / 17))
            d.polygon(p, fill=c + (255,))
        blob(d, w / 2 - s * .42, h / 2 - s * .36, s * .15, (255, 190, 204))
    make('trail-hearts', hearts)

    def stars_(d, w, h, t, im):
        s = w * .46 * (1 - t * .65)
        for k, c in ((1, (214, 138, 16)), (.8, (255, 216, 84)), (.4, (255, 250, 214))):
            p = [(w / 2 + s * k * (1 if j % 2 == 0 else .44) * math.cos(TAU * j / 10 - 1.57 + t), h / 2 + s * k * (1 if j % 2 == 0 else .44) * math.sin(TAU * j / 10 - 1.57 + t)) for j in range(10)]
            d.polygon(p, fill=c + (255,))
    make('trail-stars', stars_)

    def petals(d, w, h, t, im):
        s = w * .40 * (1 - t * .35)
        a = t * 2.4
        for k, c in ((1, (226, 110, 160)), (.8, (255, 176, 208))):
            p = [(s * k * math.cos(u) * 1.0, s * k * math.sin(u) * .52 * (1 - .35 * math.cos(u))) for u in np.linspace(0, TAU, 40)]
            d.polygon([(w / 2 + x * math.cos(a) - y * math.sin(a), h / 2 + x * math.sin(a) + y * math.cos(a)) for x, y in p], fill=c + (255,))
    make('trail-petals', petals)

    def smoke(d, w, h, t, im):
        r = np.random.default_rng(6)
        al = int(210 * (1 - t) ** 1.2) + 8
        for _ in range(9):
            g = int(60 + 90 * r.random())
            blob(d, w * (.5 + (r.random() - .5) * .42), h * (.5 + (r.random() - .5) * .42), w * (.12 + .16 * t), (g, g, g + 6), al)
    make('trail-smoke', smoke)

    def ender(d, w, h, t, im):
        s = w * .34 * (1 - t * .7)
        for k, c in ((1, (58, 10, 92)), (.7, (176, 60, 240)), (.36, (246, 190, 255))):
            d.rectangle([w / 2 - s * k, h / 2 - s * k, w / 2 + s * k, h / 2 + s * k], fill=c + (255,))
    make('trail-ender', ender)

    def rainbow(d, w, h, t, im):
        s = w * .40 * (1 - t * .6)
        c = hue_rgb(t)
        blob(d, w / 2, h / 2, s, tuple(v // 2 for v in c), 200)
        blob(d, w / 2, h / 2, s * .74, c)
        blob(d, w / 2 - s * .2, h / 2 - s * .2, s * .24, (255, 255, 255))
    make('trail-rgb', rainbow)


def extra_capes():
    noir = ramp(V, [(0, 0x15161a), (1, 0x08090b)])
    diag = (np.sin((U * 10 - V * 16) * 2.2) > 0.94) * 0.35
    save('cape-noir', [cape_sheet(trim(paste_logo(cloth(noir * (1 + diag)[..., None]), 100, 84, glow=0x2f9bff), 0x2f9bff, 3), 0x101218)])
    fr = []
    for i in range(N):
        p = PH[i]
        n = fbm(U * 4 + 0.6 * math.cos(p), V * 6 + 0.6 * math.sin(p), 5)
        g = ramp(n, [(0, 0x070b16), (0.45, 0x1a2640), (0.75, 0x3b5378), (1, 0x7d97bd)])
        r = np.random.default_rng(100 + i // 3)
        if i % 6 < 3:
            bolt = Image.new('L', (CW, CH))
            dd = ImageDraw.Draw(bolt)
            x, y, pts = CW * (0.25 + 0.5 * r.random()), 0, []
            while y < CH:
                pts.append((x, y))
                x += (r.random() - .5) * 34
                y += 10 + r.random() * 14
            dd.line(pts, fill=255, width=3)
            m = np.array(bolt).astype(float) / 255
            glow = np.array(bolt.filter(ImageFilter.GaussianBlur(9))).astype(float) / 255
            k = (1, .65, .3)[i % 3]
            g = g + (rgb(0x6fb4ff) * 2.6 * k) * glow[..., None] + 255 * k * m[..., None]
            g = g * (1 + .25 * k)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x0b1120))
    save('cape-storm', fr)


def crosshair():
    """Cruz por defecto del juego, a 67x67."""
    a = np.zeros((67, 67, 4), np.uint8)
    a[32:36, 16:52] = 255
    a[16:52, 32:36] = 255
    save('crosshair', [a])


crosshair()
capes()
extra_capes()
dragon('wings-end', 0x2a2730, 0x0c0b10, 0x1a181f, 0x55505f, 0x8c5cc8, tear=True)
dragon('wings-frost', 0x1b5aa8, 0x0c1f44, 0x16365f, 0x9fe6ff, 0x9fe6ff)
dragon('wings-ender', 0x40206e, 0x140824, 0x1c0d30, 0xd46bff, 0xc060ff)
dragon('wings-magma', 0x8e1f10, 0x1e0605, 0x2a0d0a, 0xffd25a, 0xff8a2a)
dragon('wings-gold', 0xc08a2c, 0x4a2c0c, 0x6a4414, 0xffe9a8, 0xffe08a)
feathers('wings-angel', 0xffffff, 0xb4c4dc, 0xfff2c8)
feathers('wings-raven', 0x343a4e, 0x0d0f16, 0x3b5fa0)
feathers('wings-seraph', 0xfff4d6, 0xe0a848, 0xffe6a0, edge=0xffc24a)
feathers('wings-demon', 0x5a1420, 0x18060a, 0x902030, edge=0xff5a2c)
butterfly()
heads()
auras()
trails()


# --- alitas: las alas grandes reducidas, con variantes RGB ---
MN = 15 * D


def load_wing(name):
    return Image.open(os.path.join(OUT, name + '.png')).convert('RGBA').crop((0, 0, WN, WN))


def crisp(im, n):
    """Reduce y realza el contorno para que se lea en pequeño."""
    a = np.array(im.resize((n, n), Image.LANCZOS)).astype(float)
    mask = a[..., 3] > 110
    inner = np.array(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(5))) > 0
    a[..., :3] = np.clip(a[..., :3] * 1.18, 0, 255)
    edge = mask & ~inner
    a[edge, :3] *= 0.55
    a[..., 3] = np.where(mask, 255, 0)
    return a.astype(np.uint8)


def rainbow_over(a, t, strength=0.82):
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]] / float(a.shape[0])
    hue = (xx * 0.55 + yy * 0.35 - t) % 1
    c = np.stack([np.clip(np.abs(((hue + s) % 1) * 6 - 3) - 1, 0, 1) for s in (0, 2 / 3, 1 / 3)], -1) * 255
    lum = a[..., :3].mean(-1, keepdims=True) / 255
    k = np.clip(0.35 + lum * 1.5, 0, 1.35)
    out = a.copy()
    out[..., :3] = np.clip(a[..., :3] * (1 - strength) + c * k * strength, 0, 255)
    return out


def minis():
    for key in ('end', 'frost', 'magma', 'gold', 'angel'):
        save('wings-mini' + key, [crisp(load_wing('wings-' + key), MN)])
    base = crisp(load_wing('wings-frost'), MN)
    save('wings-minirgb', [rainbow_over(base, i / N) for i in range(N)])
    base = crisp(load_wing('wings-angel'), MN)
    save('wings-miniangelrgb', [rainbow_over(base, i / N, 0.7) for i in range(N)])
    strip = Image.open(os.path.join(OUT, 'wings-morpho.png')).convert('RGBA')
    save('wings-minimorpho', [crisp(strip.crop((0, i * WN, WN, (i + 1) * WN)), MN) for i in range(12)])


# --- mas capas ---
def more_capes():
    fr = []
    for i in range(N):
        p = PH[i]
        g = ramp(V, [(0, 0x9fe8ff), (0.35, 0x2aa3d8), (1, 0x062a55)])
        ray = np.clip(np.sin((U - V * 0.35) * 14 + p) * np.sin((U - V * 0.35) * 5 + 1), 0, 1) * np.clip(0.75 - V, 0, 1)
        g += 130 * ray[..., None]
        caustic = np.abs(np.sin(U * 12 + 2 * np.sin(V * 9 + p)) + np.sin(V * 15 - 2 * np.cos(U * 8 - p)))
        g += 60 * (caustic < 0.22)[..., None] * np.clip(1 - V, 0.1, 1)[..., None]
        r = np.random.default_rng(4)
        for _ in range(9):  # peces
            y = 0.25 + 0.65 * r.random()
            d = 1 if r.random() < .5 else -1
            x = (r.random() + d * (i / N)) % 1.3 - 0.15
            s = 0.05 + 0.03 * r.random()
            body = ((U - x) / s) ** 2 + ((V - y) / (s * 0.28)) ** 2 < 1
            tail = (np.abs(V - y) < (d * (x - U) - s * 0.8) * 0.55) & (d * (x - U) < s * 1.6) & (d * (x - U) > s * 0.8)
            g = np.where((body | tail)[..., None], rgb([0xffb347, 0xff6b6b, 0xfff2a8][r.integers(3)]), g)
        sand = V > 0.93 + 0.015 * np.sin(U * 14)
        g = np.where(sand[..., None], rgb(0xd9c08a), g)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x0a3a66))
    save('cape-ocean', fr)

    fr = []
    r = np.random.default_rng(12)
    cols, rows = 15, 24
    glyph = r.random((64, 4, 3)) > 0.45
    pick = r.integers(0, 64, (rows, cols))
    start = r.random(cols)
    speed = r.integers(1, 3, cols)
    for i in range(N):
        g = np.zeros((CH, CW, 3)) + rgb(0x020a06)
        for cx in range(cols):
            head = (start[cx] + speed[cx] * i / N) % 1 * rows
            for cy in range(rows):
                dist = (head - cy) % rows
                k = max(0.0, 1 - dist / 12)
                if k <= 0:
                    continue
                c = rgb(0xd8ffe4) if dist < 1 else rgb(0x22e06a) * k
                gl = glyph[(pick[cy, cx] + (i // 4) * ((cx + cy) % 5 == 0)) % 64]
                for yy in range(4):
                    for xx in range(3):
                        if gl[yy, xx]:
                            x0, y0 = cx * 8 + xx * 2 + 1, cy * 8 + yy * 2
                            g[y0:y0 + 2, x0:x0 + 2] = c
        fr.append(cape_sheet(g, 0x03140a))
    save('cape-matrix', fr)

    fr = []
    for i in range(N):
        g = ramp(V, [(0, 0x12042e), (0.45, 0x6a1277), (0.58, 0xff4d8d), (0.6, 0xff9a4d)])
        sun = (U - 0.5) ** 2 * 0.39 + (V - 0.46) ** 2 < 0.032
        bars = (np.sin(V * 130) > 0.2) & (V > 0.44)
        g = np.where((sun & ~bars)[..., None], ramp((V - 0.28) / 0.35, [(0, 0xfff08a), (1, 0xff3d8f)]), g)
        ground = V > 0.6
        z = 0.06 / np.clip(V - 0.585, 0.004, 1)
        gx = np.abs(((U - 0.5) * z * 9) % 1 - 0.5) > 0.5 - np.clip(z * 0.05, 0.012, 0.2)
        gy = ((z * 2.2 + i / N) % 1) < 0.10 + 0.03 * z
        floor = np.where((gx | gy)[..., None], rgb(0x35f0ff), rgb(0x0b0426))
        g = np.where(ground[..., None], floor, g)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x1a0638))
    save('cape-synth', fr)

    fr = []
    s = 15.0
    q = (X * math.sqrt(3) / 3 - Y / 3) / s
    rr = Y * 2 / 3 / s
    xq, zr = q, rr
    yq = -xq - zr
    rx, ry, rz = np.round(xq), np.round(yq), np.round(zr)
    dx, dy, dz = np.abs(rx - xq), np.abs(ry - yq), np.abs(rz - zr)
    rx = np.where((dx > dy) & (dx > dz), -ry - rz, rx)
    rz = np.where((dz >= dx) & (dz >= dy), -rx - ry, rz)
    edge = np.maximum.reduce([np.abs(xq - rx - (zr - rz)), np.abs(zr - rz - (yq + rx + rz)), np.abs(yq + rx + rz - (xq - rx))])
    cell = np.abs(np.sin(rx * 12.9898 + rz * 78.233) * 43758.5) % 1
    for i in range(N):
        pulse = 0.5 + 0.5 * np.sin(PH[i] * 1 + cell * TAU)
        g = np.zeros((CH, CW, 3)) + rgb(0x07101f)
        g += rgb(0x1566d8) * (pulse * (cell > 0.45) * 0.9)[..., None]
        g = np.where((edge > 0.88)[..., None], rgb(0x2fd0ff) * (0.5 + 0.5 * pulse)[..., None], g)
        fr.append(cape_sheet(trim(paste_logo(np.clip(g, 0, 255), 84, 84, tint=0xffffff, glow=0x2fd0ff), 0x2fd0ff, 3), 0x07101f))
    save('cape-hex', fr)

    weave = ((X // 4 + Y // 4) % 2 == 0)
    g = np.where(weave[..., None], rgb(0x2b2e35), rgb(0x16181d)) * (1 + 0.35 * np.sin((U + V) * 5))[..., None]
    save('cape-carbon', [cape_sheet(trim(paste_logo(np.clip(g, 0, 255), 92, 84, tint=0xd7dde8), 0x8d97a8, 3), 0x111317)])

    g = ramp(V, [(0, 0x1b1446), (0.3, 0x7a2a6f), (0.5, 0xe8566a), (0.64, 0xffb05a), (0.72, 0xffe29a)])
    sun = (U - 0.5) ** 2 * 0.39 + (V - 0.62) ** 2 < 0.012
    g = np.where(sun[..., None], rgb(0xfff6cf), g)
    for k, (c, h, f) in enumerate([(0x6a3a6a, 0.70, 5), (0x3f2450, 0.78, 8), (0x1d1330, 0.87, 6)]):
        ridge = h - 0.05 * np.abs(np.sin(U * f + k * 2)) - 0.02 * np.sin(U * 23 + k)
        g = np.where((V > ridge)[..., None], rgb(c), g)
    save('cape-sunset', [cape_sheet(g, 0x2a1648)])

    g = ramp(V, [(0, 0x08060c), (0.7, 0x24070c), (1, 0x0a0406)])
    d = np.hypot((U - 0.5) * 0.625, V - 0.36)
    moon = d < 0.17
    craters = fbm(U * 9, V * 14, 4)
    g += rgb(0xb01020) * np.exp(-np.clip(d - 0.17, 0, 1) * 9)[..., None] * 0.7
    g = np.where(moon[..., None], ramp(craters, [(0.2, 0x7a0c14), (0.5, 0xd83a34), (0.8, 0xff8a6a)]), g)
    g += 200 * (stars(1.0) * (d > 0.2) * (V < 0.8))[..., None]
    tree = Image.new('L', (CW * SS, CH * SS))
    dd = ImageDraw.Draw(tree)
    rt = np.random.default_rng(2)

    def twig(x, y, a, l, w, depth):
        x2, y2 = x + math.cos(a) * l, y - math.sin(a) * l
        dd.line([(x, y), (x2, y2)], fill=255, width=max(2, int(w)))
        if depth:
            for sgn in (-1, 1):
                twig(x2, y2, a + sgn * (0.3 + rt.random() * 0.4), l * 0.74, w * 0.64, depth - 1)
    twig(CW * SS * 0.18, CH * SS, math.pi / 2 - 0.1, CH * SS * 0.22, 26, 6)
    twig(CW * SS * 0.9, CH * SS, math.pi / 2 + 0.2, CH * SS * 0.17, 20, 5)
    m = np.array(tree.resize((CW, CH), Image.LANCZOS)) / 255.0
    g = g * (1 - m[..., None]) + rgb(0x050305) * m[..., None]
    save('cape-moon', [cape_sheet(np.clip(g, 0, 255), 0x1a0508)])

    r = np.random.default_rng(31)
    pts = np.stack([r.random(60) * CW, r.random(60) * CH], -1)
    dist = np.sqrt((X[..., None] - pts[:, 0]) ** 2 + (Y[..., None] - pts[:, 1]) ** 2)
    o = np.sort(dist, -1)
    cell = dist.argmin(-1)
    tone = (cell * 0.618) % 1
    fr = []
    for i in range(N):
        k = 0.5 + 0.5 * np.sin(PH[i] + tone * TAU)
        g = lerp(rgb(0x2c7fc4), rgb(0xd9f5ff), tone * 0.7 + 0.25 * k)
        g *= (1.05 - 0.25 * V)[..., None]
        g = np.where((o[..., 1] - o[..., 0] < 1.8)[..., None], rgb(0xf6fdff), g)
        fr.append(cape_sheet(np.clip(g, 0, 255), 0x2a6aa8))
    save('cape-ice', fr)

    g = np.zeros((CH, CW, 3)) + rgb(0x6d0f24)
    dm = (np.abs(((X + Y) / 20) % 2 - 1) < 0.09) | (np.abs(((X - Y) / 20) % 2 - 1) < 0.09)
    g = np.where(dm[..., None], rgb(0xb8862c), g)
    fur = (V > 0.9) | (U < 0.07) | (U > 0.93)
    spots = (np.hypot((X % 22) - 11, (Y % 22) - 11) < 2.4)
    g = cloth(g)
    g = np.where(fur[..., None], np.where(spots[..., None], rgb(0x16130f), rgb(0xf6f1e4)), g)
    save('cape-royal', [cape_sheet(paste_logo(np.clip(g, 0, 255), 72, 84, tint=0xffd36a), 0x3a0812)])

    fr = []
    for i in range(N):
        g = np.zeros((CH, CW, 3)) + rgb(0x04060c)
        rs = np.random.default_rng(77)
        for layer, (n, sp, c) in enumerate([(120, 1, 0x1d3f47), (80, 1, 0x2f6f66), (46, 2, 0x63c9a6), (26, 2, 0xc7a0ff)]):
            for _ in range(n):
                x, y = rs.random(), (rs.random() + sp * i / N) % 1
                w = 1 + layer // 2
                px, py = int(x * CW), int(y * CH)
                g[py:py + w * 2, px:px + w] = rgb(c)
        fr.append(cape_sheet(trim(g, 0x3a1d5c, 3), 0x0a0714))
    save('cape-endcape', fr)


def more_effects():
    FW, FH, K = 33, 36, 8

    def make(name, draw):
        sheet = Image.new('RGBA', (FW * 8, FH))
        for i in range(8):
            im = Image.new('RGBA', (FW * K, FH * K))
            draw(ImageDraw.Draw(im), FW * K, FH * K, i / 7, i)
            sheet.paste(im.resize((FW, FH), Image.LANCZOS), (i * FW, 0))
        save('trail-' + name, [np.array(sheet)])

    def souls(d, w, h, t, i):
        s = w * .36 * (1 - t * .45)
        cx, cy = w / 2 + w * .06 * math.sin(i * 1.3), h * .52
        for k, c in ((1, (20, 90, 110)), (.78, (70, 214, 226)), (.45, (214, 252, 255))):
            r = s * k
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c + (255,))
            d.polygon([(cx - r * .9, cy), (cx + r * .9, cy), (cx + r * .3 * math.sin(i), cy + r * 1.9)], fill=c + (255,))
        e = s * .2
        for x in (-.38, .38):
            d.ellipse([cx + s * x - e, cy - e * 1.4, cx + s * x + e, cy + e * 1.2], fill=(6, 34, 46, 255))
    make('souls', souls)

    def notes(d, w, h, t, i):
        c = hue_rgb(t * .9)
        s = 1 - t * .4
        ox, oy = w * .5, h * .5
        f = lambda x, y: (ox + x * w * s, oy + y * h * s)
        d.ellipse([*f(-.30, .14), *f(-.04, .36)], fill=c + (255,))
        d.ellipse([*f(.10, .06), *f(.36, .28)], fill=c + (255,))
        d.rectangle([*f(-.10, -.36), *f(-.04, .26)], fill=c + (255,))
        d.rectangle([*f(.30, -.42), *f(.36, .18)], fill=c + (255,))
        d.polygon([f(-.10, -.36), f(.36, -.44), f(.36, -.28), f(-.10, -.20)], fill=c + (255,))
    make('notes', notes)

    def crit(d, w, h, t, i):
        s = w * .46 * (1 - t * .7)
        for k, c in ((1, (150, 70, 10)), (.8, (255, 196, 70)), (.45, (255, 250, 220))):
            r = s * k
            p = [(w / 2 + r * (1 if j % 2 == 0 else .22) * math.cos(TAU * j / 8 + .78), h / 2 + r * (1 if j % 2 == 0 else .22) * math.sin(TAU * j / 8 + .78)) for j in range(8)]
            d.polygon(p, fill=c + (255,))
    make('crit', crit)

    def blood(d, w, h, t, i):
        r = w * .32 * (1 - t * .55)
        for k, c in ((1, (70, 4, 8)), (.82, (172, 12, 20))):
            q = r * k
            d.ellipse([w / 2 - q, h * .55 - q, w / 2 + q, h * .55 + q], fill=c + (255,))
            d.polygon([(w / 2 - q * .85, h * .5), (w / 2 + q * .85, h * .5), (w / 2, h * .55 - q * 2)], fill=c + (255,))
        d.ellipse([w / 2 - r * .5, h * .5 - r * .2, w / 2 - r * .1, h * .5 + r * .2], fill=(240, 96, 96, 255))
    make('blood', blood)

    def coins(d, w, h, t, i):
        turn = abs(math.cos(i * .8))
        rx, ry = w * .36 * max(.12, turn), h * .34
        for k, c in ((1, (140, 84, 8)), (.86, (255, 196, 46)), (.6, (255, 232, 130))):
            d.ellipse([w / 2 - rx * k, h / 2 - ry * k, w / 2 + rx * k, h / 2 + ry * k], fill=c + (255,))
        if turn > .4:
            d.rectangle([w / 2 - rx * .12, h / 2 - ry * .34, w / 2 + rx * .12, h / 2 + ry * .34], fill=(168, 104, 12, 255))
    make('coins', coins)


minis()
more_capes()
more_effects()
