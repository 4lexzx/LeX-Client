"""Pinta las hojas de las animaciones 3D de muerte y de baja.

El mapa de muestras sale de launcher/shared/deathfx.mjs (se lee con node), asi
que la geometria y el dibujo no se pueden desalinear.
"""
import json
import math
import os
import subprocess

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

D = 12
SS = 4
ROOT = os.path.join(os.path.dirname(__file__), '..', 'launcher')
OUT = os.path.join(ROOT, 'assets', 'cosmetics')
rng = np.random.default_rng(7)


def layout():
    code = "import('./shared/deathfx.mjs').then(m => console.log(JSON.stringify({ kinds: m.DFX_KINDS, sw: m.SWATCHES })))"
    out = subprocess.run(['node', '-e', code], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def noise(w, h, scale, seed):
    """Ruido suave en [0, 1]."""
    r = np.random.default_rng(seed)
    gw, gh = max(2, int(w / scale) + 2), max(2, int(h / scale) + 2)
    g = r.random((gh, gw)).astype(np.float32)
    img = Image.fromarray((g * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
    return np.asarray(img).astype(np.float32) / 255


def fbm(w, h, scale, seed, octaves=4):
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        out += noise(w, h, scale / (2 ** o), seed + o) * amp
        tot += amp
        amp *= 0.5
    return out / tot


def rgba(color, alpha, shape):
    a = np.zeros(shape + (4,), np.float32)
    a[..., :3] = color
    a[..., 3] = alpha
    return a


def grid(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return (x + 0.5) / w, (y + 0.5) / h


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')


def big(w, h):
    """Lienzo supermuestreado para dibujar formas con bordes suaves."""
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def small(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def over(dst, src):
    """Compone src (PIL RGBA) encima de dst (PIL RGBA)."""
    return Image.alpha_composite(dst, src)


# ------------------------------------------------------------------ rayo
def bolt_core(w, h, thin=False):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    core = np.clip(1 - d / (0.35 if thin else 0.45), 0, 1)
    flick = 0.85 + 0.15 * noise(w, h, 6, 3)
    c = np.zeros((h, w, 4), np.float32)
    edge = np.array([70, 170, 255], np.float32)
    mid = np.array([170, 230, 255], np.float32)
    white = np.array([255, 255, 255], np.float32)
    t = core[..., None]
    c[..., :3] = np.where(t > 0.5, mid + (white - mid) * ((t - 0.5) * 2), edge + (mid - edge) * (t * 2))
    c[..., 3] = (150 + 105 * core) * flick
    return to_img(c)


def bolt_glow(w, h):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    a = np.exp(-(d / 0.28) ** 2) * 170 + np.exp(-(d / 0.08) ** 2) * 80
    a *= np.clip(v * 6, 0, 1) * np.clip((1 - v) * 3, 0, 1) * 0.6 + 0.4
    c = rgba((150, 215, 255), a, (h, w))
    return to_img(c)


def scorch(w, h):
    u, v = grid(w, h)
    x, y = (u - 0.5) * 2, (v - 0.5) * 2
    r = np.sqrt(x * x + y * y)
    ang = np.arctan2(y, x)
    n = fbm(w, h, w / 5, 11)
    edge = 0.86 + (n - 0.5) * 0.3
    inside = r < edge
    burnt = 0.55 + 0.45 * fbm(w, h, w / 10, 12)
    c = np.zeros((h, w, 4), np.float32)
    c[..., 0] = 34 * burnt
    c[..., 1] = 26 * burnt
    c[..., 2] = 22 * burnt
    c[..., 3] = np.where(inside, 235 * np.clip((edge - r) / 0.12, 0, 1), 0)
    # Brasas en anillo y grietas que brillan.
    ring = np.exp(-((r - 0.55 * edge) / 0.07) ** 2) * (n > 0.45)
    cracks = np.zeros_like(r)
    for k in range(9):
        a0 = k / 9 * math.tau + 0.3 * math.sin(k * 7)
        da = np.angle(np.exp(1j * (ang - a0)))
        cracks = np.maximum(cracks, np.exp(-(da * r * 18) ** 2) * (r < edge * 0.9) * (r > 0.08))
    glow = np.clip(ring + cracks * 0.9, 0, 1) * inside
    hot = np.array([255, 120, 30], np.float32)
    c[..., :3] = c[..., :3] * (1 - glow[..., None]) + hot * glow[..., None]
    center = np.exp(-(r / 0.16) ** 2)
    c[..., :3] = c[..., :3] * (1 - center[..., None]) + np.array([255, 220, 150]) * center[..., None]
    return to_img(c)


def spark(w, h):
    u, v = grid(w, h)
    d = np.maximum(np.abs(u - 0.5), np.abs(v - 0.5)) * 2
    c = np.zeros((h, w, 4), np.float32)
    t = np.clip(1 - d, 0, 1)[..., None]
    c[..., :3] = np.array([255, 150, 40]) * (1 - t) + np.array([255, 255, 210]) * t
    c[..., 3] = 255
    return to_img(c)


# ------------------------------------------------------------------ lapida
def stone(w, h, seed, light=1.0, moss=True):
    n = fbm(w, h, w / 3, seed)
    fine = noise(w, h, 2.5, seed + 9)
    base = (112 + 46 * n + 18 * (fine - 0.5)) * light
    c = np.stack([base, base, base * 1.06], -1)
    speck = rng.random((h, w)) > 0.985
    c[speck] *= 0.6
    if moss:
        u, v = grid(w, h)
        m = (fbm(w, h, w / 4, seed + 3) + v * 0.6) > 1.05
        c[m] = c[m] * 0.4 + np.array([60, 110, 50]) * 0.6
    a = np.full((h, w), 255.0)
    out = np.concatenate([c, a[..., None]], -1)
    return to_img(out)


def bevel(im, k):
    """Borde claro arriba-izquierda y oscuro abajo-derecha."""
    a = np.asarray(im).astype(np.float32)
    h, w = a.shape[:2]
    a[:k, :, :3] *= 1.25
    a[:, :k, :3] *= 1.15
    a[-k:, :, :3] *= 0.65
    a[:, -k:, :3] *= 0.72
    return to_img(a)


PIX = {
    'R': ['1110', '1001', '1001', '1110', '1010', '1001', '1001'],
    'I': ['111', '010', '010', '010', '010', '010', '111'],
    'P': ['1110', '1001', '1001', '1110', '1000', '1000', '1000'],
    '.': ['0', '0', '0', '0', '0', '0', '1'],
}


def grave_front(w, h):
    im = bevel(stone(w, h, 21), max(2, w // 24))
    a = np.asarray(im).astype(np.float32)
    px = max(2, w // 26)

    def carve(x, y, ww, hh):
        a[y:y + hh, x:x + ww, :3] *= 0.3
        a[y + hh:y + hh + max(1, px // 2), x:x + ww, :3] = np.minimum(255, a[y + hh:y + hh + max(1, px // 2), x:x + ww, :3] * 1.6)
    # Cruz.
    cx = w // 2
    carve(cx - px, int(h * 0.12), px * 2, int(h * 0.22))
    carve(cx - px * 4, int(h * 0.18), px * 8, px * 2)
    # R.I.P
    text = 'R.I.P'
    widths = [len(PIX[ch][0]) for ch in text]
    total = (sum(widths) + len(text) - 1) * px
    x = (w - total) // 2
    y0 = int(h * 0.47)
    for ch, cw in zip(text, widths):
        for j, row in enumerate(PIX[ch]):
            for i, bit in enumerate(row):
                if bit == '1':
                    carve(x + i * px, y0 + j * px, px, px)
        x += (cw + 1) * px
    # Grieta.
    pts = [(int(w * 0.78), int(h * 0.05))]
    for k in range(6):
        pts.append((pts[-1][0] + int(rng.integers(-px * 2, px * 2)), pts[-1][1] + int(h * 0.07)))
    img = to_img(a)
    dr = ImageDraw.Draw(img)
    dr.line(pts, fill=(52, 52, 58, 255), width=max(1, px // 2))
    return img


def dirt(w, h, grass=False):
    n = fbm(w, h, w / 4, 31)
    c = np.stack([88 + 50 * n, 60 + 34 * n, 40 + 22 * n], -1)
    peb = rng.random((h, w)) > 0.97
    c[peb] = np.array([130, 118, 104])
    if grass:
        g = fbm(w, h, w / 5, 33) > 0.48
        blades = rng.random((h, w)) > 0.55
        green = np.stack([60 + 40 * n, 128 + 50 * n, 44 + 20 * n], -1)
        c = np.where((g & blades)[..., None], green, c)
    return to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))


def flower(w, h, petal, center):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    dr.line([(W / 2, H), (W / 2 + W * 0.04, H * 0.42)], fill=(40, 120, 40, 255), width=int(W * 0.07))
    dr.ellipse([W * 0.15, H * 0.68, W * 0.5, H * 0.8], fill=(50, 140, 50, 255))
    dr.ellipse([W * 0.52, H * 0.6, W * 0.86, H * 0.72], fill=(46, 130, 46, 255))
    cx, cy, r = W / 2 + W * 0.04, H * 0.32, W * 0.2
    for k in range(5):
        a = k / 5 * math.tau
        px, py = cx + math.cos(a) * r * 0.75, cy + math.sin(a) * r * 0.75
        dr.ellipse([px - r * 0.62, py - r * 0.62, px + r * 0.62, py + r * 0.62], fill=petal + (255,))
    dr.ellipse([cx - r * 0.38, cy - r * 0.38, cx + r * 0.38, cy + r * 0.38], fill=center + (255,))
    return small(im, w, h)


# ------------------------------------------------------------------ alma
def ghost(w, h, fade=False, face=False):
    u, v = grid(w, h)
    n = noise(w, h, w / 3, 41)
    c = np.zeros((h, w, 4), np.float32)
    c[..., 0] = 196 + 40 * (1 - v) + 10 * n
    c[..., 1] = 236 + 18 * (1 - v)
    c[..., 2] = 255
    c[..., 3] = 205 + 30 * (1 - v)
    if fade:
        c[..., 3] *= np.clip(1.15 - v, 0, 1)
    img = to_img(c)
    if face:
        im, dr = big(w, h)
        W, H = w * SS, h * SS
        for ex in (0.3, 0.7):
            dr.rounded_rectangle([W * (ex - 0.11), H * 0.3, W * (ex + 0.11), H * 0.62], radius=W * 0.08, fill=(28, 40, 78, 255))
            dr.ellipse([W * (ex - 0.07), H * 0.33, W * (ex - 0.01), H * 0.42], fill=(255, 255, 255, 255))
        dr.ellipse([W * 0.44, H * 0.7, W * 0.56, H * 0.82], fill=(40, 52, 96, 255))
        for ex in (0.16, 0.84):
            dr.ellipse([W * (ex - 0.08), H * 0.62, W * (ex + 0.08), H * 0.72], fill=(255, 150, 190, 170))
        img = over(img, small(im, w, h))
    return img


def wing(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    # Plumas en tres filas, de la raiz (izquierda) a la punta.
    rows = [(0.95, (255, 255, 255)), (0.72, (232, 244, 255)), (0.5, (205, 228, 255))]
    for ri, (length, col) in enumerate(rows):
        for k in range(6):
            t = k / 5
            x0 = W * 0.04
            y0 = H * (0.18 + ri * 0.08)
            x1 = W * (0.12 + length * (0.35 + 0.65 * t))
            y1 = H * (0.18 + ri * 0.1 + t * (0.55 - ri * 0.06))
            dr.polygon([(x0, y0), (x1, y1 - H * 0.07), (x1 + W * 0.04, y1 + H * 0.04), (x0, y0 + H * 0.16)], fill=col + (255,), outline=(150, 190, 230, 255))
    dr.ellipse([0, H * 0.08, W * 0.3, H * 0.42], fill=(255, 255, 255, 255))
    return small(im, w, h)


def halo(w, h):
    u, v = grid(w, h)
    r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
    ring = np.exp(-((r - 0.78) / 0.09) ** 2)
    glow = np.exp(-((r - 0.78) / 0.25) ** 2) * 0.45
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array([255, 214, 90]) * (1 - ring[..., None] * 0.3) + np.array([255, 255, 220]) * ring[..., None] * 0.3
    c[..., 3] = np.clip(ring + glow, 0, 1) * 255
    return to_img(c)


def radial_glow(w, h, color, strength=150):
    u, v = grid(w, h)
    r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
    c = rgba(color, np.clip(1 - r, 0, 1) ** 2 * strength, (h, w))
    return to_img(c)


# ------------------------------------------------------------------ calavera
def bone(w, h, seed=51):
    n = fbm(w, h, w / 3, seed)
    c = np.stack([214 + 30 * n, 206 + 28 * n, 180 + 24 * n], -1)
    a = np.concatenate([c, np.full((h, w, 1), 255.0)], -1)
    img = bevel(to_img(a), max(2, w // 20))
    dr = ImageDraw.Draw(img)
    for _ in range(2):
        x, y = int(rng.integers(w // 5, w * 4 // 5)), int(rng.integers(0, h // 3))
        pts = [(x, y)]
        for _ in range(4):
            pts.append((pts[-1][0] + int(rng.integers(-w // 10, w // 10)), pts[-1][1] + h // 9))
        dr.line(pts, fill=(140, 128, 104, 255), width=max(1, w // 60))
    return img


def skull_face(w, h):
    img = bone(w, h, 53)
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    for ex in (0.29, 0.71):
        dr.rounded_rectangle([W * (ex - 0.17), H * 0.24, W * (ex + 0.17), H * 0.56], radius=W * 0.1, fill=(22, 14, 22, 255))
    nose = [(W * 0.5, H * 0.58), (W * 0.43, H * 0.72), (W * 0.57, H * 0.72)]
    dr.polygon(nose, fill=(30, 20, 26, 255))
    # Dientes.
    dr.rectangle([W * 0.18, H * 0.8, W * 0.82, H * 0.98], fill=(40, 30, 34, 255))
    for k in range(6):
        x = W * (0.2 + k * 0.104)
        dr.rounded_rectangle([x, H * 0.8, x + W * 0.085, H * 0.95], radius=W * 0.02, fill=(244, 238, 216, 255))
    img = over(img, small(im, w, h))
    # Ojos que brillan.
    glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    for ex in (0.29, 0.71):
        g.ellipse([W * (ex - 0.08), H * 0.33, W * (ex + 0.08), H * 0.49], fill=(210, 90, 255, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(W * 0.03))
    g = ImageDraw.Draw(glow)
    for ex in (0.29, 0.71):
        g.ellipse([W * (ex - 0.035), H * 0.37, W * (ex + 0.035), H * 0.45], fill=(255, 235, 255, 255))
    return over(img, small(glow, w, h))


def jaw(w, h):
    img = bone(w, h, 57)
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    dr.rectangle([W * 0.12, 0, W * 0.88, H * 0.5], fill=(40, 30, 34, 255))
    for k in range(6):
        x = W * (0.15 + k * 0.122)
        dr.rounded_rectangle([x, H * 0.05, x + W * 0.1, H * 0.5], radius=W * 0.02, fill=(244, 238, 216, 255))
    return over(img, small(im, w, h))


def flame(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    for k, col in ((1.0, (90, 20, 160)), (0.78, (170, 60, 255)), (0.55, (230, 140, 255)), (0.3, (255, 235, 255))):
        cx = W / 2
        dr.polygon([(cx, H * (1 - 0.98 * k)), (cx + W * 0.42 * k, H * (1 - 0.35 * k)), (cx + W * 0.3 * k, H), (cx - W * 0.3 * k, H), (cx - W * 0.42 * k, H * (1 - 0.35 * k))], fill=col + (235,))
    im = im.filter(ImageFilter.GaussianBlur(W * 0.02))
    return small(im, w, h)


def rune_ring(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    cx, cy = W / 2, H / 2
    for r, wd, col in ((0.47, 0.025, (170, 70, 255)), (0.36, 0.015, (220, 140, 255)), (0.18, 0.012, (170, 70, 255))):
        R = W * r
        dr.ellipse([cx - R, cy - R, cx + R, cy + R], outline=col + (255,), width=int(W * wd))
    for k in range(12):
        a = k / 12 * math.tau
        R = W * 0.415
        x, y = cx + math.cos(a) * R, cy + math.sin(a) * R
        s = W * 0.025
        if k % 3 == 0:
            dr.polygon([(x, y - s), (x + s, y), (x, y + s), (x - s, y)], fill=(255, 210, 255, 255))
        else:
            dr.line([(x - s, y - s), (x + s, y + s)], fill=(230, 160, 255, 255), width=int(W * 0.01))
    for k in range(3):
        a0 = k / 3 * math.tau - math.pi / 2
        p = [(cx + math.cos(a0 + j * math.tau / 3) * W * 0.36, cy + math.sin(a0 + j * math.tau / 3) * W * 0.36) for j in range(3)]
        dr.polygon(p, outline=(200, 110, 255, 200))
    glow = im.filter(ImageFilter.GaussianBlur(W * 0.02))
    im = Image.alpha_composite(glow, im)
    return small(im, w, h)


# ------------------------------------------------------------------ golpes
def star(w, h, fill=(255, 214, 70), edge=(150, 90, 10)):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    cx, cy = W / 2, H / 2
    for scale, col in ((1.0, edge), (0.78, fill), (0.4, (255, 250, 210))):
        pts = []
        for k in range(10):
            r = (0.48 if k % 2 == 0 else 0.2) * W * scale
            a = -math.pi / 2 + k * math.pi / 5
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        dr.polygon(pts, fill=col + (255,))
    return small(im, w, h)


def spark_cross(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    for k, col in ((0.12, (255, 230, 120)), (0.05, (255, 255, 255))):
        dr.polygon([(W / 2, 0), (W / 2 + W * k, H / 2), (W / 2, H), (W / 2 - W * k, H / 2)], fill=col + (255,))
        dr.polygon([(0, H / 2), (W / 2, H / 2 - H * k), (W, H / 2), (W / 2, H / 2 + H * k)], fill=col + (255,))
    return small(im, w, h)


def daze_ring(w, h):
    u, v = grid(w, h)
    d = np.abs(v - 0.5) * 2
    a = np.clip(1 - d, 0, 1) ** 2 * 140 * (0.6 + 0.4 * np.sin(u * math.tau * 6) ** 2)
    return to_img(rgba((255, 226, 120), a, (h, w)))


def slash_arc(w, h, core=(255, 255, 255), edge=(120, 210, 255)):
    u, v = grid(w, h)
    # Media luna fina: gruesa en el centro y afilada en las puntas.
    thick = np.sin(u * math.pi) ** 1.3 * 0.42
    center = 0.5 + (u - 0.5) ** 2 * 0.9
    d = np.abs(v - center) / np.maximum(thick, 1e-3)
    t = np.clip(1 - d, 0, 1)
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array(edge) * (1 - t[..., None]) + np.array(core) * t[..., None]
    c[..., 3] = np.where(d < 1, 255 * np.clip(t * 3, 0, 1), 0)
    return to_img(c)


def slash_glow(w, h):
    u, v = grid(w, h)
    r = np.sqrt(((u - 0.5) * 1.2) ** 2 + (v - 0.5) ** 2) * 2
    return to_img(rgba((150, 220, 255), np.clip(1 - r, 0, 1) ** 2 * 110, (h, w)))


def shock_ring(w, h):
    u, v = grid(w, h)
    r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
    ring = np.exp(-((r - 0.8) / 0.07) ** 2)
    inner = np.exp(-((r - 0.62) / 0.12) ** 2) * 0.45
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array([255, 255, 255]) * ring[..., None] + np.array([255, 190, 90]) * (1 - ring[..., None])
    c[..., 3] = np.clip(ring + inner, 0, 1) * 255
    return to_img(c)


def shard(w, h):
    u, v = grid(w, h)
    t = np.clip(1 - np.maximum(np.abs(u - 0.5), np.abs(v - 0.5)) * 2, 0, 1)[..., None]
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array([200, 110, 40]) * (1 - t) + np.array([255, 236, 170]) * t
    c[..., 3] = 255
    return to_img(c)



# ------------------------------------------------------------------ fuegos artificiales
def rocket(w, h):
    u, v = grid(w, h)
    band = (np.floor(v * 6) % 2 == 0)
    c = np.where(band[..., None], np.array([210, 40, 50], np.float32), np.array([245, 240, 230], np.float32))
    c = c * (0.75 + 0.35 * np.sin(u * math.pi))[..., None]
    out = to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    dr.polygon([(0, 0), (W, 0), (W / 2, -H * 0.2)], fill=(255, 220, 80, 255))
    return over(out, small(im, w, h))


def stick(w, h):
    u, v = grid(w, h)
    c = np.zeros((h, w, 3), np.float32) + np.array([150, 110, 70])
    c = c * (0.8 + 0.2 * noise(w, h, 3, 71))[..., None]
    return to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))


def glow_ball(color):
    def f(w, h):
        u, v = grid(w, h)
        r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
        t = np.clip(1 - r, 0, 1)
        c = np.zeros((h, w, 4), np.float32)
        c[..., :3] = np.array(color) * (1 - t[..., None] ** 2) + np.array([255, 255, 255]) * t[..., None] ** 2
        c[..., 3] = 255
        return to_img(c)
    return f


def streak(w, h):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    a = np.clip(1 - d, 0, 1) ** 1.5 * np.clip(1 - v, 0, 1) * 230
    return to_img(rgba((255, 200, 120), a, (h, w)))


# ------------------------------------------------------------------ corona
def gold(w, h, seed=81):
    n = fbm(w, h, w / 3, seed)
    u, v = grid(w, h)
    c = np.stack([210 + 40 * n, 160 + 40 * n, 40 + 30 * n], -1) * (0.85 + 0.3 * (1 - v))[..., None]
    img = bevel(to_img(np.concatenate([np.clip(c, 0, 255), np.full((h, w, 1), 255.0)], -1)), max(2, w // 16))
    return img


def gem(color):
    def f(w, h):
        im, dr = big(w, h)
        W, H = w * SS, h * SS
        dr.rectangle([0, 0, W, H], fill=(120, 80, 20, 255))
        dr.polygon([(W / 2, H * 0.08), (W * 0.92, H / 2), (W / 2, H * 0.92), (W * 0.08, H / 2)], fill=color + (255,))
        dr.polygon([(W / 2, H * 0.2), (W * 0.7, H / 2), (W / 2, H * 0.45)], fill=(255, 255, 255, 200))
        return small(im, w, h)
    return f


def crown_band(w, h):
    img = gold(w, h, 83)
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    dr.line([(0, H * 0.18), (W, H * 0.18)], fill=(120, 80, 20, 255), width=int(H * 0.06))
    dr.line([(0, H * 0.82), (W, H * 0.82)], fill=(120, 80, 20, 255), width=int(H * 0.06))
    for k, col in ((0.25, (220, 30, 50)), (0.5, (40, 120, 255)), (0.75, (40, 200, 90))):
        dr.ellipse([W * k - H * 0.18, H * 0.32, W * k + H * 0.18, H * 0.68], fill=col + (255,))
        dr.ellipse([W * k - H * 0.07, H * 0.36, W * k + H * 0.02, H * 0.45], fill=(255, 255, 255, 220))
    return over(img, small(im, w, h))


def gold_ring(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    cx, cy = W / 2, H / 2
    for r, wd, col in ((0.46, 0.03, (255, 210, 80)), (0.36, 0.012, (255, 240, 180))):
        R = W * r
        dr.ellipse([cx - R, cy - R, cx + R, cy + R], outline=col + (255,), width=int(W * wd))
    for k in range(16):
        a = k / 16 * math.tau
        x, y = cx + math.cos(a) * W * 0.41, cy + math.sin(a) * W * 0.41
        s_ = W * (0.03 if k % 2 else 0.018)
        dr.polygon([(x, y - s_), (x + s_ * 0.35, y), (x, y + s_), (x - s_ * 0.35, y)], fill=(255, 250, 220, 255))
        dr.polygon([(x - s_, y), (x, y - s_ * 0.35), (x + s_, y), (x, y + s_ * 0.35)], fill=(255, 250, 220, 255))
    glow = im.filter(ImageFilter.GaussianBlur(W * 0.015))
    return small(Image.alpha_composite(glow, im), w, h)


# ------------------------------------------------------------------ ataud
def coffin_wood(w, h, seed=91):
    u, v = grid(w, h)
    n = fbm(w, h, w / 2, seed)
    grain = np.sin(u * 14 + n * 6) * 0.5 + 0.5
    c = np.stack([60 + 40 * grain, 34 + 24 * grain, 22 + 14 * grain], -1)
    return bevel(to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1)), max(2, w // 18))


def coffin_lid(w, h):
    img = coffin_wood(w, h, 93)
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    dr.rectangle([W * 0.46, H * 0.18, W * 0.54, H * 0.6], fill=(220, 180, 70, 255))
    dr.rectangle([W * 0.3, H * 0.28, W * 0.7, H * 0.34], fill=(220, 180, 70, 255))
    dr.rectangle([W * 0.06, H * 0.03, W * 0.94, H * 0.97], outline=(30, 16, 10, 255), width=int(W * 0.03))
    return over(img, small(im, w, h))


def velvet(w, h):
    u, v = grid(w, h)
    n = fbm(w, h, w / 4, 95)
    folds = np.sin(u * 18 + n * 4) * 0.5 + 0.5
    c = np.stack([120 + 60 * folds, 10 + 14 * folds, 24 + 16 * folds], -1)
    return to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))


# ------------------------------------------------------------------ ascension
def beam(w, h):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    a = (np.exp(-(d / 0.35) ** 2) * 150 + np.exp(-(d / 0.1) ** 2) * 105) * np.clip(1 - v * 0.6, 0, 1)
    c = rgba((255, 246, 200), a, (h, w))
    return to_img(c)


def feather_wing(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    for row, (length, col) in enumerate([(1.0, (255, 255, 255)), (0.78, (250, 246, 230)), (0.56, (240, 232, 205))]):
        for k in range(7):
            t = k / 6
            x0, y0 = W * 0.02, H * (0.12 + row * 0.07)
            x1 = W * (0.15 + length * (0.3 + 0.55 * t))
            y1 = H * (0.15 + row * 0.08 + t * (0.6 - row * 0.08))
            dr.polygon([(x0, y0), (x1, y1 - H * 0.08), (x1 + W * 0.05, y1 + H * 0.05), (x0, y0 + H * 0.16)], fill=col + (255,), outline=(210, 190, 140, 255))
    dr.ellipse([0, H * 0.04, W * 0.24, H * 0.36], fill=(255, 255, 255, 255))
    return small(im, w, h)


PAINT = {
    'firework': {'rocket': rocket, 'stick': stick, 'gold': glow_ball((255, 190, 40)), 'cyan': glow_ball((40, 220, 255)), 'pink': glow_ball((255, 70, 190)),
                 'trail': streak, 'flash': lambda w, h: radial_glow(w, h, (255, 240, 200), 200)},
    'crown': {'gold': lambda w, h: gold(w, h), 'gem': gem((220, 30, 50)), 'gemb': gem((40, 120, 255)), 'band': crown_band,
              'spark': lambda w, h: star(w, h, (255, 240, 160), (200, 140, 30)), 'ring': gold_ring},
    'coffin': {'wood': lambda w, h: coffin_wood(w, h), 'lid': coffin_lid, 'velvet': velvet, 'gold': lambda w, h: gold(w, h, 97),
               'dirt': lambda w, h: dirt(w, h, True), 'glow': lambda w, h: radial_glow(w, h, (180, 120, 255), 150)},
    'angel': {'beam': beam, 'feather': feather_wing, 'halo': halo, 'glow': lambda w, h: radial_glow(w, h, (255, 240, 180), 160)},
    'daze': {'star': lambda w, h: star(w, h), 'spark': spark_cross, 'ring': daze_ring},
    'slash': {'slash': lambda w, h: slash_arc(w, h), 'glow': slash_glow},
    'impact': {'ring': shock_ring, 'shard': shard, 'spark': lambda w, h: star(w, h, (255, 150, 60), (140, 50, 10))},
    'bolt': {'core': lambda w, h: bolt_core(w, h), 'branch': lambda w, h: bolt_core(w, h, True), 'glow': bolt_glow, 'scorch': scorch, 'spark': spark},
    'grave': {'stone': lambda w, h: stone(w, h, 22), 'front': grave_front, 'top': lambda w, h: stone(w, h, 23, 1.12, False),
              'dirt': lambda w, h: dirt(w, h), 'grass': lambda w, h: dirt(w, h, True),
              'poppy': lambda w, h: flower(w, h, (220, 30, 40), (40, 30, 20)), 'orchid': lambda w, h: flower(w, h, (60, 170, 255), (255, 230, 120))},
    'soul': {'ghost': lambda w, h: ghost(w, h), 'face': lambda w, h: ghost(w, h, face=True), 'tail': lambda w, h: ghost(w, h, fade=True),
             'wing': wing, 'halo': halo, 'glow': lambda w, h: radial_glow(w, h, (170, 230, 255), 130)},
    'skull': {'bone': lambda w, h: bone(w, h), 'face': skull_face, 'jaw': jaw, 'dark': lambda w, h: to_img(rgba((26, 18, 24), 255, (h, w))),
              'flame': flame, 'ring': rune_ring},
}


# ------------------------------------------------------------------ bajas y muertes
def blade(w, h):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    c = np.zeros((h, w, 4), np.float32)
    steel = np.array([170, 180, 196]) * (1 - d[..., None] * 0.35) + np.array([255, 255, 255]) * np.clip(1 - d * 4, 0, 1)[..., None] * 0.5
    c[..., :3] = steel * (0.85 + 0.15 * noise(w, h, 8, 201))[..., None]
    c[..., 3] = 255
    return bevel(to_img(c), max(2, w // 10))


def rune_strip(w, h):
    im, dr = big(w, h)
    W, H = w * SS, h * SS
    for k in range(5):
        y = H * (0.1 + k * 0.18)
        dr.line([(W * 0.2, y), (W * 0.8, y + H * 0.06)], fill=(120, 220, 255, 255), width=int(W * 0.12))
        dr.line([(W * 0.5, y - H * 0.04), (W * 0.5, y + H * 0.1)], fill=(160, 240, 255, 255), width=int(W * 0.1))
    glow = im.filter(ImageFilter.GaussianBlur(W * 0.15))
    return small(Image.alpha_composite(glow, im), w, h)


def leather(w, h):
    u, v = grid(w, h)
    wrap = (np.floor(v * 10) % 2 == 0)
    c = np.where(wrap[..., None], np.array([90, 54, 30], np.float32), np.array([60, 34, 18], np.float32))
    return to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))


def blood_pool(w, h, seed=211):
    u, v = grid(w, h)
    x, y = (u - 0.5) * 2, (v - 0.5) * 2
    r = np.sqrt(x * x + y * y)
    n = fbm(w, h, w / 4, seed)
    edge = 0.72 + (n - 0.5) * 0.45
    inside = r < edge
    c = np.zeros((h, w, 4), np.float32)
    deep = np.clip((edge - r) * 3, 0, 1)
    c[..., 0] = 120 + 70 * (1 - deep)
    c[..., 1] = 6 + 10 * (1 - deep)
    c[..., 2] = 10 + 8 * (1 - deep)
    hl = np.exp(-(((u - 0.38) / 0.08) ** 2 + ((v - 0.35) / 0.05) ** 2))
    c[..., :3] = c[..., :3] * (1 - hl[..., None]) + np.array([255, 150, 150]) * hl[..., None]
    c[..., 3] = np.where(inside, 235, 0)
    # Gotas sueltas alrededor.
    rng = np.random.default_rng(seed)
    for _ in range(9):
        a, d = rng.random() * math.tau, 0.78 + rng.random() * 0.18
        cx, cy, rr = 0.5 + math.cos(a) * d / 2, 0.5 + math.sin(a) * d / 2, 0.02 + rng.random() * 0.03
        m = ((u - cx) ** 2 + (v - cy) ** 2) < rr * rr
        c[m, :3] = (150, 10, 14)
        c[m, 3] = 235
    return to_img(c)


def shadow_ring(w, h):
    u, v = grid(w, h)
    r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
    return to_img(rgba((10, 6, 12), np.clip(1 - r, 0, 1) ** 0.7 * 200, (h, w)))


def red_ring(w, h, color=(255, 60, 40)):
    u, v = grid(w, h)
    r = np.sqrt((u - 0.5) ** 2 + (v - 0.5) ** 2) * 2
    ring = np.exp(-((r - 0.8) / 0.07) ** 2) + np.exp(-((r - 0.62) / 0.14) ** 2) * 0.4
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array(color) * (1 - np.clip(ring, 0, 1)[..., None] * 0.3) + 255 * np.clip(ring - 0.7, 0, 1)[..., None]
    c[..., 3] = np.clip(ring, 0, 1) * 255
    return to_img(c)


def cloud(w, h, dark=False, seed=221):
    n = fbm(w, h, w / 3, seed)
    base = 70 if dark else 120
    c = np.stack([base + 50 * n, base + 50 * n, base + 60 * n], -1)
    u, v = grid(w, h)
    c *= (1.05 - 0.35 * v)[..., None]
    return to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1))


def vortex(w, h, seed=231, color=(150, 60, 255)):
    u, v = grid(w, h)
    x, y = (u - 0.5) * 2, (v - 0.5) * 2
    r = np.sqrt(x * x + y * y)
    a = np.arctan2(y, x)
    arms = np.sin(a * 3 + r * 9) * 0.5 + 0.5
    c = np.zeros((h, w, 4), np.float32)
    t = np.clip(arms * (1 - r) * 1.6, 0, 1)[..., None]
    c[..., :3] = np.array(color) * t + np.array([255, 220, 255]) * np.clip(t - 0.7, 0, 1) * 2
    core = r < 0.22
    c[core, :3] = 0
    c[..., 3] = np.where(r < 1, np.clip((1 - r) * 3, 0, 1) * 240, 0)
    c[core, 3] = 255
    return to_img(c)


def metal_red(w, h):
    u, v = grid(w, h)
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array([200, 30, 40]) * (0.75 + 0.35 * (1 - v))[..., None]
    c[..., 3] = 255
    return bevel(to_img(c), max(2, w // 10))


def steel_dark(w, h):
    n = noise(w, h, 4, 241)
    c = np.stack([60 + 30 * n, 62 + 30 * n, 70 + 30 * n], -1)
    return bevel(to_img(np.concatenate([c, np.full((h, w, 1), 255.0)], -1)), max(2, w // 8))


def flame_grad(w, h):
    u, v = grid(w, h)
    d = np.abs(u - 0.5) * 2
    width = 0.25 + 0.75 * (1 - v)
    t = np.clip(1 - d / width, 0, 1)
    c = np.zeros((h, w, 4), np.float32)
    c[..., :3] = np.array([255, 120, 20]) * (1 - t[..., None]) + np.array([255, 250, 200]) * t[..., None]
    c[..., 3] = np.where(t > 0.02, 230 * np.clip(1 - v * 0.9, 0, 1), 0)
    return to_img(c)


def body_rocket(w, h):
    return rocket(w, h)


def blood_splat(w, h):
    return blood_pool(w, h, 251)


def flipbook(frames, fn, size=64, blur=0.03):
    sheet = Image.new('RGBA', (size * frames, size), (0, 0, 0, 0))
    for i in range(frames):
        im = Image.new('RGBA', (size * 4, size * 4), (0, 0, 0, 0))
        fn(ImageDraw.Draw(im), size * 4, i / max(1, frames - 1), np.random.default_rng(i + 7))
        if blur:
            im = im.filter(ImageFilter.GaussianBlur(size * blur))
        sheet.paste(im.resize((size, size), Image.LANCZOS), (i * size, 0))
    return sheet


def p_drop(d, s, t, rng):
    r = s * (0.12 + t * 0.06)
    a = int(255 * (1 - t * 0.3))
    d.ellipse([s / 2 - r, s / 2 - r * 0.6, s / 2 + r, s / 2 + r * 1.2], fill=(255, 255, 255, a))
    d.polygon([(s / 2 - r * 0.8, s / 2), (s / 2 + r * 0.8, s / 2), (s / 2, s / 2 - r * 2.2)], fill=(255, 255, 255, a))


def p_puff(d, s, t, rng):
    r = s * (0.2 + t * 0.22)
    for k in range(6):
        a, dd = rng.random() * math.tau, rng.random() * r * 0.6
        rr = r * (0.5 + rng.random() * 0.5)
        x, y = s / 2 + math.cos(a) * dd, s / 2 + math.sin(a) * dd
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255, int(200 * (1 - t * 0.6))))


def p_dot(d, s, t, rng):
    r = s * 0.12 * (1 - t * 0.5)
    d.ellipse([s / 2 - r, s / 2 - r, s / 2 + r, s / 2 + r], fill=(255, 255, 255, 255))
    d.ellipse([s / 2 - r * 2, s / 2 - r * 2, s / 2 + r * 2, s / 2 + r * 2], fill=(255, 255, 255, 70))


def p_flake(d, s, t, rng):
    w, h = s * 0.18, s * 0.1
    a = t * math.pi * 2
    pts = [(s / 2 + math.cos(a + k * math.pi / 2) * (w if k % 2 == 0 else h), s / 2 + math.sin(a + k * math.pi / 2) * (w if k % 2 == 0 else h)) for k in range(4)]
    d.polygon(pts, fill=(255, 255, 255, 255))


def p_confetti(d, s, t, rng):
    cols = [(255, 60, 80), (60, 200, 255), (255, 220, 60), (120, 255, 120), (220, 100, 255)]
    col = cols[int(t * 7) % len(cols)]
    w, h = s * 0.2, s * 0.09 * abs(math.cos(t * math.pi * 3)) + s * 0.02
    d.rectangle([s / 2 - w, s / 2 - h, s / 2 + w, s / 2 + h], fill=col + (255,))


def p_feather(d, s, t, rng):
    d.ellipse([s * 0.3, s * 0.15, s * 0.7, s * 0.85], fill=(255, 255, 255, 255))
    d.line([(s / 2, s * 0.1), (s / 2, s * 0.9)], fill=(200, 200, 210, 255), width=int(s * 0.03))


FX = {'blood': p_drop, 'spray': p_drop, 'drip': p_drop, 'smoke': p_puff, 'ember': p_dot, 'ash': p_flake, 'void': p_dot, 'spark': p_dot,
      'flame': p_puff, 'confetti': p_confetti, 'feather': p_feather}

KPAINT = {
    'execution': {'blade': blade, 'guard': lambda w, h: gold(w, h), 'hilt': leather, 'pommel': gem((220, 30, 50)), 'pool': lambda w, h: blood_pool(w, h),
                  'shadow': shadow_ring, 'ring': lambda w, h: red_ring(w, h, (255, 255, 255)), 'rune': rune_strip},
    'wrath': {'cloud': lambda w, h: cloud(w, h), 'clouddark': lambda w, h: cloud(w, h, True, 223), 'bolt': lambda w, h: bolt_core(w, h), 'scorch': scorch,
              'glow': lambda w, h: radial_glow(w, h, (220, 240, 255), 220), 'ember': spark},
    'blackhole': {'vortex': lambda w, h: vortex(w, h), 'vortex2': lambda w, h: vortex(w, h, 233, (90, 30, 200)), 'ring': lambda w, h: red_ring(w, h, (200, 120, 255)),
                  'star': lambda w, h: star(w, h, (220, 160, 255), (90, 30, 160))},
    'rocket': {'fin': metal_red, 'nozzle': steel_dark, 'flame': flame_grad, 'gold': glow_ball((255, 190, 40)), 'cyan': glow_ball((40, 220, 255)),
               'pink': glow_ball((255, 70, 190)), 'flash': lambda w, h: radial_glow(w, h, (255, 240, 200), 230), 'body': body_rocket},
    'ascension': {'beam': beam, 'feather': feather_wing, 'halo': halo, 'glow': lambda w, h: radial_glow(w, h, (255, 240, 180), 170)},
    'bleed': {'pool': lambda w, h: blood_pool(w, h, 213), 'stone': lambda w, h: stone(w, h, 22), 'front': grave_front,
              'flower': lambda w, h: flower(w, h, (220, 30, 40), (40, 30, 20)), 'drip': lambda w, h: glow_ball((170, 10, 20))(w, h)},
    'implode': {'ring': lambda w, h: red_ring(w, h), 'splat': blood_splat, 'shard': lambda w, h: glow_ball((255, 60, 30))(w, h), 'glow': lambda w, h: radial_glow(w, h, (255, 90, 60), 220)},
}


def klayout():
    code = "import('./shared/killfx.mjs').then(m => console.log(JSON.stringify({ kinds: m.KILL_KINDS, sw: m.KILL_SWATCHES, fx: Object.keys(m.KILL_PARTICLES) })))"
    out = subprocess.run(['node', '-e', code], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def main():
    lay = layout()
    for kind in lay['kinds']:
        key, slot, (_, _, sw, sh) = kind['key'], kind['slot'], kind['rect']
        sheet = Image.new('RGBA', (sw * D, sh * D), (0, 0, 0, 0))
        for name, (x, y, w, h) in lay['sw'][key].items():
            sheet.paste(PAINT[key][name](w * D, h * D), (x * D, y * D))
        sheet.save(os.path.join(OUT, f'{slot}-{key}.png'))
        print(slot, key, sheet.size)
    kl = klayout()
    for kind in kl['kinds']:
        key, slot, (_, _, sw, sh) = kind['key'], kind['slot'], kind['rect']
        sheet = Image.new('RGBA', (sw * D, sh * D), (0, 0, 0, 0))
        for name, (x, y, w, h) in kl['sw'][key].items():
            sheet.paste(KPAINT[key][name](w * D, h * D), (x * D, y * D))
        sheet.save(os.path.join(OUT, f'{slot}-{key}.png'))
        print(slot, key, sheet.size)
    fxdir = os.path.join(ROOT, 'assets', 'fx')
    os.makedirs(fxdir, exist_ok=True)
    for name in kl['fx']:
        flipbook(8, FX[name]).save(os.path.join(fxdir, f'{name}.png'))
    print('particulas', len(kl['fx']))


main()
