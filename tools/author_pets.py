"""Herramienta de autor: escribe las carpetas de launcher/cosmetics/ para las
mascotas, hechas como mobs de Minecraft (cubos de medidas enteras, huesos de
mob y textura en pixel art a 1 px por texel), a tamano de mob y reducidas con
'scale' a version mini.

Produce los archivos que exportaria Blockbench (model.geo.json, texture.png,
animation.json, particulas con flipbook) y cosmetic.json. El cliente NO ejecuta
esto: solo carga los archivos.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(__file__), '..', 'launcher', 'cosmetics')
FACES = ['north', 'south', 'east', 'west', 'up', 'down']


# ------------------------------------------------------------------ modelo
class Model:
    def __init__(self, tex_w, tex_h):
        self.tw, self.th = tex_w, tex_h
        self.bones, self.order, self.pending, self.regions = {}, [], [], []
        self.sx = self.sy = self.row = 0
        self.packed = False

    def bone(self, name, parent=None, pivot=(0, 0, 0), locators=None):
        b = {'name': name, 'pivot': list(pivot), 'cubes': []}
        if parent:
            b['parent'] = parent
        if locators:
            b['locators'] = locators
        self.bones[name] = b
        self.order.append(name)
        return name

    def cube(self, bone, origin, size, mat, faces=None, rotation=None, pivot=None, only=None):
        x, y, z = size
        dims = {'north': (x, y), 'south': (x, y), 'east': (z, y), 'west': (z, y), 'up': (x, z), 'down': (x, z)}
        flat = [i for i, n in enumerate(size) if n == 0]
        keep = only or [f for f in FACES if all(d > 0 for d in dims[f])]
        if flat and not only:
            keep = {0: ['east'], 1: ['up'], 2: ['north']}[flat[0]]
        c = {'origin': list(origin), 'size': list(size), 'uv': {}}
        for f in keep:
            self.pending.append((dims[f][0], dims[f][1], c['uv'], f, (faces or {}).get(f, mat), c))
        if rotation:
            c['rotation'] = list(rotation)
            c['pivot'] = list(pivot)
        self.bones[bone]['cubes'].append(c)
        return c

    def pack(self):
        if self.packed:
            return
        self.packed = True
        for w, h, uv, f, mat, c in sorted(self.pending, key=lambda t: (-math.ceil(t[1]), -t[0])):
            W, H = max(1, math.ceil(w - 1e-6)), max(1, math.ceil(h - 1e-6))
            if self.sx + W > self.tw:
                self.sx, self.sy, self.row = 0, self.sy + self.row, 0
            if self.sy + H > self.th:
                raise RuntimeError('textura llena')
            uv[f] = {'uv': [self.sx, self.sy], 'uv_size': [w, h]}
            self.regions.append((self.sx, self.sy, W, H, mat, f, c))
            self.sx += W
            self.row = max(self.row, H)

    def geo(self, ident):
        self.pack()
        return {'format_version': '1.12.0', 'minecraft:geometry': [{
            'description': {'identifier': ident, 'texture_width': self.tw, 'texture_height': self.th,
                            'visible_bounds_width': 3, 'visible_bounds_height': 3, 'visible_bounds_offset': [0, 1, 0]},
            'bones': [self.bones[n] for n in self.order]}]}

    def paint(self, painter, seed=3):
        self.pack()
        img = np.zeros((self.th, self.tw, 4), np.uint8)
        rng = np.random.default_rng(seed)
        for (x, y, w, h, mat, face, c) in self.regions:
            img[y:y + h, x:x + w] = painter(mat, face, w, h, c, rng)
        return Image.fromarray(img, 'RGBA')


# ------------------------------------------------------------------ pixel art
def hexc(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], np.int32)


def ramp(base, n=5, spread=0.42):
    """Tonos de oscuro a claro alrededor de un color, como en las texturas del juego."""
    b = hexc(base).astype(np.float32)
    out = []
    for i in range(n):
        t = (i - (n - 1) / 2) / ((n - 1) / 2) * spread
        c = b * (1 + t) if t < 0 else b + (255 - b) * t * 0.7
        out.append(np.clip(c, 0, 255).astype(np.uint8))
    return out


def solid(w, h, col, a=255):
    out = np.zeros((h, w, 4), np.uint8)
    out[..., :3] = col
    out[..., 3] = a
    return out


def tones(idx, pal):
    idx = np.clip(idx, 0, len(pal) - 1)
    out = np.zeros(idx.shape + (4,), np.uint8)
    for i, c in enumerate(pal):
        out[idx == i, :3] = c
    out[..., 3] = 255
    return out


def side_light(w, h, face):
    """Indice base por fila: mas claro arriba, mas oscuro abajo; tapas aparte."""
    if face == 'up':
        return np.full((h, w), 3)
    if face == 'down':
        return np.full((h, w), 1)
    rows = np.full((h, w), 2)
    rows[0, :] = 3
    if h > 2:
        rows[-1, :] = 1
    return rows


def scales(w, h, face, pal, rng):
    idx = side_light(w, h, face)
    yy, xx = np.mgrid[0:h, 0:w]
    edge = ((xx + (yy // 2) * 2) % 4 == 0) & (yy % 2 == 1)
    idx = idx - edge.astype(int)
    idx = idx + (rng.random((h, w)) < 0.12).astype(int) - (rng.random((h, w)) < 0.08).astype(int)
    return tones(idx, pal)


def plates(w, h, face, pal, rng):
    yy, xx = np.mgrid[0:h, 0:w]
    idx = np.where(yy % 2 == 0, 3, 2) if face in ('down', 'up') else np.where(xx % 3 == 0, 2, 3)
    idx = idx + (rng.random((h, w)) < 0.1).astype(int) - (rng.random((h, w)) < 0.1).astype(int)
    return tones(idx, pal)


def gradient(w, h, face, pal, rng):
    yy, _ = np.mgrid[0:h, 0:w]
    idx = (len(pal) - 1 - (yy * len(pal) // max(1, h))).astype(int)
    if face == 'up':
        idx[:] = len(pal) - 1
    return tones(idx, pal)


def fur(w, h, face, pal, rng, patches=None, cube=None):
    idx = side_light(w, h, face) + (rng.random((h, w)) < 0.18).astype(int) - (rng.random((h, w)) < 0.14).astype(int)
    out = tones(idx, pal)
    if patches is not None:
        # Manchas por posicion en el espacio: coinciden entre caras vecinas.
        o, s = np.array(cube['origin'], float), np.array(cube['size'], float)
        yy, xx = np.mgrid[0:h, 0:w].astype(float)
        u, v = (xx + 0.5) / w, (yy + 0.5) / h
        p = {'north': (o[0] + s[0] * (1 - u), o[1] + s[1] * (1 - v), o[2] + 0 * u),
             'south': (o[0] + s[0] * u, o[1] + s[1] * (1 - v), o[2] + s[2] + 0 * u),
             'east': (o[0] + s[0] + 0 * u, o[1] + s[1] * (1 - v), o[2] + s[2] * (1 - u)),
             'west': (o[0] + 0 * u, o[1] + s[1] * (1 - v), o[2] + s[2] * u),
             'up': (o[0] + s[0] * u, o[1] + s[1] + 0 * u, o[2] + s[2] * v),
             'down': (o[0] + s[0] * u, o[1] + 0 * u, o[2] + s[2] * (1 - v))}[face]
        for k, (pal2, freq, thr) in enumerate(patches):
            n = (np.sin(p[0] * freq + k * 3.1) + np.sin(p[1] * freq * 0.9 + k * 1.7) + np.sin(p[2] * freq * 1.1 + k * 5.3)) / 3
            mask = n > thr
            sub = tones(side_light(w, h, face) + (rng.random((h, w)) < 0.15).astype(int), pal2)
            out[mask] = sub[mask]
    return out


def put(img, x, y, col):
    h, w = img.shape[:2]
    if 0 <= x < w and 0 <= y < h:
        img[y, x, :3] = col
        img[y, x, 3] = 255


def clear(img, x, y):
    h, w = img.shape[:2]
    if 0 <= x < w and 0 <= y < h:
        img[y, x, 3] = 0


# ------------------------------------------------------------------ utilidades de animacion
L = 'query.life_time'


def osc(speed, amp, phase=0, base=0):
    """base + amp * sin(t * speed - phase): todas las piezas usan su propio desfase."""
    return f'{base} + math.sin({L} * {speed} - {phase}) * {amp}'


def blink(at=4.0):
    return {'0.0': [0, 0, 0], f'{at:.2f}': [0, 0, 0], f'{at + 0.08:.2f}': [1, 1, 1], f'{at + 0.2:.2f}': [1, 1, 1], f'{at + 0.28:.2f}': [0, 0, 0]}


# ------------------------------------------------------------------ dragon
def dragon_model():
    m = Model(128, 64)
    m.bone('root', pivot=(0, 8, 0))
    m.bone('body', 'root', (0, 8, 0))
    m.cube('body', (-3.5, 5, -5), (7, 6, 10), 'scale', {'down': 'belly'})
    m.cube('body', (-3, 4.5, -6.5), (6, 5, 3), 'scale', {'down': 'belly', 'north': 'belly'})
    for z in (-4, -1, 2):
        m.cube('body', (-0.5, 11, z), (1, 2, 2), 'spike')
    m.bone('neck0', 'body', (0, 9.5, -6))
    m.cube('neck0', (-2, 8, -9.5), (4, 4, 3.5), 'scale', {'down': 'belly'})
    m.cube('neck0', (-0.5, 12, -8.5), (1, 1.5, 1.5), 'spike')
    m.bone('neck1', 'neck0', (0, 10.5, -9))
    m.cube('neck1', (-1.75, 9, -12.5), (3.5, 3.5, 3.5), 'scale', {'down': 'belly'})
    m.cube('neck1', (-0.5, 12.5, -11.5), (1, 1.5, 1.5), 'spike')
    m.bone('head', 'neck1', (0, 12, -12), locators={'mouth': [0, 10.8, -22.6]})
    m.cube('head', (-3.5, 10, -18), (7, 6, 6), 'skull', {'down': 'belly'})
    m.cube('head', (-2.5, 10, -22), (5, 3, 4), 'snout', {'down': 'belly'})
    m.cube('head', (-3.6, 15, -18.4), (7.2, 1, 2), 'scale')
    for s in (1, -1):
        m.cube('head', (s * 2.2 - 0.6, 15, -14.5), (1.2, 1.2, 4.5), 'horn', rotation=(35, s * 18, 0), pivot=(s * 2.2, 15.6, -14.5))
        m.cube('head', (s * 3.4 - 0.4, 13.5, -14), (0.8, 0.8, 3), 'horn', rotation=(15, s * 45, 0), pivot=(s * 3.4, 13.9, -14))
        m.cube('head', (s * 3.5 - (0 if s > 0 else 0), 10.5, -16.5), (0, 3, 3), 'frill', rotation=(0, s * -25, 0), pivot=(s * 3.5, 12, -16.5))
    m.bone('jaw', 'head', (0, 10.2, -17))
    m.cube('jaw', (-2.3, 9.2, -22), (4.6, 1, 5), 'snout', {'up': 'mouth', 'down': 'belly'})
    m.cube('jaw', (-2.1, 10.2, -21.8), (4.2, 0.5, 0.4), 'teeth')
    m.bone('lids', 'head', (0, 12.5, -18))
    m.cube('lids', (-3.5, 11.5, -18.05), (7, 2, 0), 'lid')
    for s, side in ((1, 'L'), (-1, 'R')):
        # Ala de tres tramos: brazo, antebrazo y dedo, cada uno con su membrana
        # inclinada hacia atras (asi se ve desde delante y de lado).
        sh, el, wr = s * 3.5, s * 9.5, s * 15.5
        w1 = m.bone(f'wing{side}', 'body', (sh, 10.5, -3))
        x0 = 3.5 if s > 0 else -9.5
        m.cube(w1, (x0, 10, -3.5), (6, 1, 1), 'bone')
        m.cube(w1, (x0, 4, -3), (6, 6.5, 0), 'membrane', rotation=(40, 0, 0), pivot=(sh, 10.5, -3))
        w2 = m.bone(f'wing{side}2', w1, (el, 10.5, -3))
        x1 = 9.5 if s > 0 else -15.5
        m.cube(w2, (x1, 10.1, -3.4), (6, 0.8, 0.8), 'bone')
        m.cube(w2, (x1, 5, -3), (6, 5.6, 0), 'membrane', rotation=(40, 0, 0), pivot=(el, 10.5, -3))
        m.cube(w2, (el - 0.4, 10.8, -3.6), (0.8, 1.2, 0.8), 'claw')
        w3 = m.bone(f'wing{side}3', w2, (wr, 10.5, -3))
        x2 = 15.5 if s > 0 else -20.5
        m.cube(w3, (x2, 10.2, -3.3), (5, 0.6, 0.6), 'bone')
        m.cube(w3, (x2, 6.5, -3), (5, 4.1, 0), 'membranetip', rotation=(40, 0, 0), pivot=(wr, 10.5, -3))
        a = m.bone(f'arm{side}', 'body', (s * 2.4, 6, -4.5))
        m.cube(a, (s * 2.4 - 1, 3, -5.5), (2, 3.5, 2), 'scale')
        f = m.bone(f'forearm{side}', a, (s * 2.4, 3.2, -4.5))
        m.cube(f, (s * 2.4 - 0.9, 1, -5.4), (1.8, 2.4, 1.8), 'scale')
        m.cube(f, (s * 2.4 - 1.2, 0.2, -6), (2.4, 1, 2.6), 'claw')
        g = m.bone(f'leg{side}', 'body', (s * 2.7, 7, 2.5))
        m.cube(g, (s * 2.7 - 1.3, 3.5, 1), (2.6, 4, 3.5), 'scale')
        h = m.bone(f'shin{side}', g, (s * 2.7, 4, 2))
        m.cube(h, (s * 2.7 - 0.9, 1.2, 2.2), (1.8, 3, 1.8), 'scale')
        m.cube(h, (s * 2.7 - 1.3, 0.2, 1), (2.6, 1, 3), 'claw')
    prev, z = 'body', 5
    for i, (w, hh) in enumerate([(4.4, 4), (3.6, 3.2), (2.8, 2.6), (2, 2), (1.2, 1.4)]):
        b = m.bone(f'tail{i}', prev, (0, 8, z))
        m.cube(b, (-w / 2, 8 - hh / 2, z), (w, hh, 4), 'scale', {'down': 'belly'})
        if i < 3:
            m.cube(b, (-0.5, 8 + hh / 2, z + 1), (1, 1.2, 1.5), 'spike')
        prev, z = b, z + 4
    b = m.bone('tail5', prev, (0, 8, z))
    m.cube(b, (-2, 7.8, z - 0.5), (4, 0.4, 3), 'fin', only=['up', 'down'])
    return m


DRAGONS = {'': ('rojo', 0xb3261e, 0xe8b46a, 0x3a1210, 0xffd040), 'black': ('negro', 0x2b2730, 0xc0602a, 0x121014, 0xb060ff)}


def dragon_painter(base, belly, dark, eye_hex):
    sc = ramp(base)
    be = ramp(belly, spread=0.25)
    hn = [hexc(0x3a2a1a), hexc(0x6a5034), hexc(0xa89070), hexc(0xd8c8a8), hexc(0xf0e6d0)]
    mem = ramp(((base >> 16 & 255) * 6 // 10) << 16 | ((base >> 8 & 255) * 6 // 10) << 8 | (base & 255) * 6 // 10, spread=0.35)
    eye, pupil, white = hexc(eye_hex), np.array([16, 10, 8]), np.array([255, 255, 240])

    def paint(mat, face, w, h, cube, rng):
        if mat in ('scale', 'skull', 'snout'):
            img = scales(w, h, face, sc, rng)
            if mat == 'skull' and face == 'north':
                # Ojos grandes de 2x2 con pupila de reptil y brillo.
                for x0 in (1, w - 3):
                    for dx in (0, 1):
                        for dy in (2, 3):
                            put(img, x0 + dx, dy, eye)
                    put(img, x0 + (1 if x0 == 1 else 0), 2, white)
                    put(img, x0 + (0 if x0 == 1 else 1), 3, pupil)
                    put(img, x0, 1, sc[0]); put(img, x0 + 1, 1, sc[0])
            if mat == 'skull' and face in ('east', 'west'):
                ex = 0 if face == 'east' else w - 1
                put(img, ex, 2, eye); put(img, ex, 3, pupil if face == 'east' else eye)
            if mat == 'snout' and face == 'north':
                put(img, 1, 0, sc[0]); put(img, w - 2, 0, sc[0])
            if mat == 'snout' and face in ('east', 'west'):
                for x in range(0, w, 2):
                    put(img, x, h - 1, hn[4])
            return img
        if mat == 'mouth':
            img = solid(w, h, hexc(0x5a1418))
            for x in range(0, w, 2):
                put(img, x, 0, hn[4])
            return img
        if mat == 'teeth':
            return solid(w, h, hn[4])
        if mat == 'belly':
            return plates(w, h, face, be, rng)
        if mat in ('horn', 'claw'):
            return gradient(w, h, face, hn, rng)
        if mat in ('spike', 'bone', 'frill'):
            return gradient(w, h, face, ramp(dark, spread=0.5) if mat != 'bone' else sc, rng)
        if mat == 'lid':
            img = scales(w, h, 'north', sc, rng)
            img[:, :, 3] = 0
            for x0 in (1, w - 3):
                img[:, x0:x0 + 2, 3] = 255
                img[1, x0:x0 + 2, :3] = sc[0]
            return img
        if mat in ('membrane', 'membranetip', 'fin'):
            idx = np.full((h, w), 2)
            yy, xx = np.mgrid[0:h, 0:w]
            idx[(xx + yy) % 4 == 0] = 1  # venas
            idx[0, :] = 0
            img = tones(idx, mem)
            for x in range(w):
                cut = h - (1 if x % 3 == 1 else 0) - (2 if x % 3 == 2 else 0)
                if mat == 'membranetip':
                    cut = min(cut, max(1, int(h * (1 - x / (w + 0.5)))))
                if mat == 'fin':
                    cut = min(cut, h - abs(x - w // 2))
                img[cut:, x, 3] = 0
            return img
        return solid(w, h, sc[2])
    return paint


def dragon_anims():
    tuck = lambda k=1: {'armL': {'rotation': [osc(200, 10, 60, 25 * k), 0, 0]}, 'armR': {'rotation': [osc(200, 10, 90, 25 * k), 0, 0]},
                        'forearmL': {'rotation': [osc(200, 12, 100, 15 * k), 0, 0]}, 'forearmR': {'rotation': [osc(200, 12, 130, 15 * k), 0, 0]},
                        'legL': {'rotation': [osc(200, 8, 90, 25 * k), 0, 0]}, 'legR': {'rotation': [osc(200, 8, 120, 25 * k), 0, 0]},
                        'shinL': {'rotation': [osc(200, 12, 130, 20 * k), 0, 0]}, 'shinR': {'rotation': [osc(200, 12, 160, 20 * k), 0, 0]}}

    def wings(sp, amp, base=8):
        # El brazo baja primero, el antebrazo y el dedo le siguen con retraso.
        return {'wingL': {'rotation': [0, 0, f'-({osc(sp, amp, 0, base)})']}, 'wingR': {'rotation': [0, 0, osc(sp, amp, 0, base)]},
                'wingL2': {'rotation': [0, 0, f'-({osc(sp, amp * 0.8, 45)})']}, 'wingR2': {'rotation': [0, 0, osc(sp, amp * 0.8, 45)]},
                'wingL3': {'rotation': [0, 0, f'-({osc(sp, amp * 0.6, 90)})']}, 'wingR3': {'rotation': [0, 0, osc(sp, amp * 0.6, 90)]}}
    # Plegadas hacia atras a lo largo del cuerpo (en Bedrock, y negativo lleva
    # el ala izquierda hacia la cola); el antebrazo y el dedo se doblan en zigzag.
    folded = {'wingL': {'rotation': [0, -75, 18]}, 'wingR': {'rotation': [0, 75, -18]},
              'wingL2': {'rotation': [0, 150, 0]}, 'wingR2': {'rotation': [0, -150, 0]},
              'wingL3': {'rotation': [0, -140, 0]}, 'wingR3': {'rotation': [0, 140, 0]}}

    def tail(sp, amp, droop=0):
        return {f'tail{i}': {'rotation': [osc(sp * 0.7, 3, i * 30, droop if i == 0 else 0), osc(sp, amp + i * 2, i * 45), 0]} for i in range(6)}
    neck = lambda x=0: {'neck0': {'rotation': [osc(60, 5, 0, x), osc(35, 6), 0]}, 'neck1': {'rotation': [osc(60, 5, 40, x * 0.6), osc(35, 6, 30), 0]},
                        'head': {'rotation': [osc(60, 4, 80), 0, osc(45, 4)]}}
    jaw = {'jaw': {'rotation': [f'3 + math.pow(math.max(0, math.sin({L} * 35)), 4) * 16', 0, 0]}}
    # Hombro: unidades del modelo (escala 0.4); en Bedrock la x de la animacion
    # va en el mismo sentido que la del modelo (comprobado en el juego).
    perch = [30, 15, 4]
    grip = {'armL': {'rotation': [-15, 0, 0]}, 'armR': {'rotation': [-15, 0, 0]}, 'forearmL': {'rotation': [30, 0, 0]}, 'forearmR': {'rotation': [30, 0, 0]},
            'legL': {'rotation': [-20, 0, 0]}, 'legR': {'rotation': [-20, 0, 0]}, 'shinL': {'rotation': [35, 0, 0]}, 'shinR': {'rotation': [35, 0, 0]}}
    anims = {
        'look': {'loop': True, 'bones': {'head': {'rotation': ['math.clamp(query.target_x_rotation, -40, 40) * 0.5', 'math.clamp(query.target_y_rotation, -60, 60) * 0.6', 0]}}},
        'idle': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, osc(100, 1.5), 0], 'rotation': [osc(100, 3, -60), osc(30, 8), osc(50, 3)]},
            'body': {'rotation': [osc(200, 2, 30), 0, 0]}, **wings(200, 32), **tail(110, 8), **neck(), **jaw, **tuck(), 'lids': {'scale': blink(4.2)}}},
        'walk': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, osc(300, 1.2), 0], 'rotation': [osc(300, 2, 0, 12), 0, osc(80, 4)]},
            **wings(300, 38), **tail(160, 6), **neck(-8), **tuck(1.6), 'lids': {'scale': blink(3.1)}}},
        'run': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, osc(380, 1.5), 0], 'rotation': [osc(380, 3, 0, 22), 0, 0]},
            **wings(380, 46), **tail(200, 4), **neck(-14), **tuck(2.2), 'lids': {'scale': blink(3.1)}}},
        'fly': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, osc(150, 2.5), 0], 'rotation': [osc(150, 4, 0, -4), 0, osc(75, 6)]},
            **wings(150, 40, 14), **tail(90, 10), **neck(-4), **tuck(2), 'lids': {'scale': blink(2.5)}}},
        'sit': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': perch}, 'body': {'rotation': [osc(80, 1.5, 0, -8), 0, 0], 'scale': [1, osc(80, 0.02, 0, 1), 1]},
            **folded, **grip, 'tail0': {'rotation': [-55, osc(50, 12), 0]}, **{f'tail{i}': {'rotation': [-8, osc(50, 10 + i * 3, i * 40), 0]} for i in range(1, 6)},
            'neck0': {'rotation': [-10, osc(20, 10), 0]}, 'neck1': {'rotation': [-6, osc(20, 10, 40), 0]}, 'head': {'rotation': [osc(30, 6, 0, 6), osc(17, 25, 80), osc(23, 6)]},
            **jaw, 'lids': {'scale': blink(3.6)}}},
        'sneak': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': perch}, **folded, **grip, **tail(50, 14), 'neck0': {'rotation': [-14, 0, 0]}, 'lids': {'scale': blink(3.6)}}},
        'sleep': {'loop': True, 'bones': {
            'root': {'position': [perch[0], perch[1] - 1, perch[2]]}, 'body': {'rotation': [10, 0, 0], 'scale': [1, osc(60, 0.035, 0, 1), 1]},
            **folded, **grip, 'tail0': {'rotation': [-50, 20, 0]}, **{f'tail{i}': {'rotation': [-6, 28, 0]} for i in range(1, 6)},
            'neck0': {'rotation': [30, 35, 0]}, 'neck1': {'rotation': [24, 30, 0]}, 'head': {'rotation': [22, 25, 12]},
            'lids': {'scale': [1, 1, 1]}}},
        # Te hacen dano: abre las alas, ruge y sacude la cabeza (sin fuego).
        'hurt': {'loop': False, 'animation_length': 1.2, 'bones': {
            'root': {'position': [osc(2000, 0.4), 2, 0], 'rotation': {'0.0': [0, 0, 0], '0.2': [-22, 0, 0], '1.0': [-10, 0, 0], '1.2': [0, 0, 0]}},
            'wingL': {'rotation': [0, 0, -45]}, 'wingR': {'rotation': [0, 0, 45]}, 'wingL2': {'rotation': [0, 0, 20]}, 'wingR2': {'rotation': [0, 0, -20]},
            **tail(300, 14), **tuck(),
            'neck0': {'rotation': {'0.0': [0, 0, 0], '0.2': [-30, 0, 0], '0.5': [10, 0, 0], '1.2': [0, 0, 0]}},
            'head': {'rotation': {'0.0': [0, 0, 0], '0.2': [-20, 0, 0], '0.6': [6, -14, 0], '0.9': [6, 14, 0], '1.2': [0, 0, 0]}},
            'jaw': {'rotation': {'0.0': [0, 0, 0], '0.15': [42, 0, 0], '1.0': [40, 0, 0], '1.2': [0, 0, 0]}}, 'lids': {'scale': [0, 0, 0]}}},
        # Cada 20 s: se echa atras y escupe fuego (desde el hombro si estaba posado).
        'breath': {'loop': False, 'animation_length': 2.4, 'bones': {
            'root': {'position': [f'variable.lex_still > 6 ? {perch[0]} : 0', f'variable.lex_still > 6 ? {perch[1]} : {osc(160, 1.2)}', f'variable.lex_still > 6 ? {perch[2]} : 0'],
                     'rotation': {'0.0': [0, 0, 0], '0.45': [-14, 0, 0], '0.7': [8, 0, 0], '2.0': [8, 0, 0], '2.4': [0, 0, 0]}},
            **wings(260, 22), **tail(110, 8), **tuck(),
            'neck0': {'rotation': {'0.0': [0, 0, 0], '0.45': [-26, 0, 0], '0.7': [14, 0, 0], '1.3': [14, -10, 0], '2.0': [14, 10, 0], '2.4': [0, 0, 0]}},
            'neck1': {'rotation': {'0.0': [0, 0, 0], '0.45': [-20, 0, 0], '0.7': [10, 0, 0], '2.0': [10, 0, 0], '2.4': [0, 0, 0]}},
            'jaw': {'rotation': {'0.0': [2, 0, 0], '0.45': [14, 0, 0], '0.65': [38, 0, 0], '2.0': [38, 0, 0], '2.4': [2, 0, 0]}}, 'lids': {'scale': [0, 0, 0]}},
            'particle_effects': {'0.62': {'effect': 'breath', 'locator': 'mouth'}}},
    }
    return {'format_version': '1.8.0', 'animations': {f'animation.dragon.{k}': v for k, v in anims.items()}}


# ------------------------------------------------------------------ gato
def cat_model():
    m = Model(128, 64)
    m.bone('root', pivot=(0, 0, 0), locators={'zzz': [3, 15, -8], 'hearts': [0, 14, -8]})
    m.bone('body', 'root', (0, 7, 1))
    m.cube('body', (-3, 4, -5), (6, 5.5, 11), 'fur')
    m.cube('body', (-3.3, 4.3, -6), (6.6, 5, 2.5), 'fur')
    m.bone('head', 'body', (0, 9, -5.5))
    m.cube('head', (-3.5, 7, -10.5), (7, 6, 5), 'fur', {'north': 'face'})
    m.cube('head', (-1.5, 7, -11.5), (3, 2, 1), 'muzzle')
    for s, side in ((1, 'L'), (-1, 'R')):
        e = m.bone(f'ear{side}', 'head', (s * 2, 13, -8))
        m.cube(e, (s * 2 - 1, 13, -8.5), (2, 2, 1), 'fur', {'north': 'ear'})
    m.bone('lids', 'head', (0, 10.5, -10.5))
    m.cube('lids', (-3, 9.5, -10.55), (6, 1.5, 0), 'lid')
    for s, side in ((1, 'L'), (-1, 'R')):
        for end, z in (('F', -4), ('B', 4)):
            up = m.bone(f'leg{end}{side}', 'body', (s * 1.8, 5, z))
            m.cube(up, (s * 1.8 - 1, 2, z - 1), (2, 3, 2), 'fur')
            lo = m.bone(f'leg{end}{side}2', up, (s * 1.8, 2, z))
            m.cube(lo, (s * 1.8 - 1, 0, z - 1), (2, 2, 2), 'paw')
    prev = 'body'
    for i, (w, z) in enumerate([(1.5, 6), (1.4, 9), (1.3, 12), (1.2, 15)]):
        b = m.bone(f'tail{i}', prev, (0, 8.5, z))
        m.cube(b, (-w / 2, 8.5 - w / 2, z), (w, w, 3), 'tailtip' if i == 3 else 'fur')
        prev = b
    return m


CATS = {'': ('calicó', 0xf2eee6, [(0xe08a3a, 0.6, 0.35), (0x2e2a2a, 0.75, 0.45)], 0x6cc84a),
        'orange': ('naranja atigrado', 0xe89a48, [(0xc06a24, 1.3, 0.2)], 0xe8c040)}


def cat_painter(base, patches, eye):
    pal = ramp(base, spread=0.3)
    pp = [(ramp(c, spread=0.3), f, t) for c, f, t in patches]
    pink = hexc(0xf090a0)

    def paint(mat, face, w, h, cube, rng):
        if mat in ('fur', 'face', 'ear', 'tailtip'):
            img = fur(w, h, face, pal, rng, pp, cube)
            if mat == 'face':
                for x0 in (1, w - 3):
                    put(img, x0, 2, hexc(eye)); put(img, x0 + 1, 2, hexc(eye))
                    put(img, x0, 3, hexc(eye)); put(img, x0 + 1, 3, hexc(eye))
                    put(img, x0 + (1 if x0 == 1 else 0), 3, np.array([16, 16, 16]))
                    put(img, x0 + (0 if x0 == 1 else 1), 2, np.array([255, 255, 255]))
            if mat == 'ear' and face == 'north':
                put(img, w // 2, h - 1, pink); put(img, w // 2 - 1, h - 1, pink)
            if mat == 'tailtip':
                img[:, :, :3] = np.clip(img[:, :, :3].astype(int) * 0.8, 0, 255).astype(np.uint8)
            return img
        if mat == 'muzzle':
            img = solid(w, h, ramp(0xfaf6ee, spread=0.15)[3])
            if face == 'north':
                put(img, w // 2, 0, pink)
                put(img, 0, 1, np.array([60, 40, 44])); put(img, w - 1, 1, np.array([60, 40, 44]))
            return img
        if mat == 'paw':
            return tones(side_light(w, h, face), ramp(0xfaf6f0, spread=0.2))
        if mat == 'lid':
            img = fur(w, h, 'north', pal, rng)
            img[:, :, 3] = 0
            for x in (0, 1, w - 2, w - 1):
                img[:, x, 3] = 255
                put(img, x, 1, pal[0])
            return img
        return solid(w, h, pal[2])
    return paint


def cat_anims():
    legs = ['FL', 'FR', 'BL', 'BR']

    def gait(sp, amp, knee, pairs):
        out = {}
        for leg, ph in pairs.items():
            out[f'leg{leg}'] = {'rotation': [osc(sp, amp, ph), 0, 0]}
            # La rodilla solo se dobla cuando la pata vuelve hacia delante.
            sign = -1 if leg[0] == 'F' else 1
            out[f'leg{leg}2'] = {'rotation': [f'{sign} * math.max(0, math.sin({L} * {sp} - {ph} - 70)) * {knee}', 0, 0]}
        return out
    tail = lambda sp, amp, up: {f'tail{i}': {'rotation': [up if i == 0 else 10, osc(sp, amp + i * 4, i * 40), 0]} for i in range(4)}
    ears = {'earL': {'rotation': [f'math.max(0, math.sin({L} * 37)) * math.max(0, math.sin({L} * 211)) * 25', 0, 0]},
            'earR': {'rotation': [f'math.max(0, math.sin({L} * 29 + 1)) * math.max(0, math.sin({L} * 197)) * 25', 0, 0]}}
    still = {f'leg{l}': {'rotation': [0, 0, 0]} for l in legs}
    anims = {
        'look': {'loop': True, 'bones': {'head': {'rotation': ['math.clamp(query.target_x_rotation, -30, 30) * 0.5', 'math.clamp(query.target_y_rotation, -70, 70) * 0.7', 0]}}},
        # De pie: respira, mueve la cola en S, las orejas y parpadea.
        'idle': {'loop': True, 'animation_length': 5, 'bones': {
            'body': {'scale': [1, osc(90, 0.015, 0, 1), 1]}, **still,
            **tail(70, 14, 35), **ears, 'head': {'rotation': [osc(40, 3), 0, osc(28, 5)]}, 'lids': {'scale': blink(4.0)}}},
        'walk': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, f'math.abs(math.sin({L} * 500)) * 0.4', 0]}, 'body': {'rotation': [0, 0, osc(250, 2)]},
            **gait(500, 28, 35, {'FL': 0, 'BR': 0, 'FR': 180, 'BL': 180}), **tail(250, 8, 50), **ears, 'lids': {'scale': blink(3.5)}}},
        # Galope: patas delanteras juntas, traseras juntas y el lomo se mece.
        'run': {'loop': True, 'animation_length': 5, 'bones': {
            'root': {'position': [0, f'math.abs(math.sin({L} * 800)) * 1.2', 0]}, 'body': {'rotation': [osc(800, 8, -60), 0, 0]},
            'head': {'rotation': [osc(800, 6, 120), 0, 0]},
            **gait(800, 48, 50, {'FL': 0, 'FR': 20, 'BL': 170, 'BR': 190}), **tail(400, 4, 10), 'lids': {'scale': [0, 0, 0]}}},
        'fly': {'loop': True, 'animation_length': 5, 'bones': {
            'body': {'rotation': [-10, 0, 0]}, 'legFL': {'rotation': [-55, 0, 0]}, 'legFR': {'rotation': [-50, 0, 0]},
            'legBL': {'rotation': [55, 0, 0]}, 'legBR': {'rotation': [50, 0, 0]}, **tail(200, 10, 60), 'lids': {'scale': [0, 0, 0]}}},
        # Sentado: el lomo se levanta, las patas de atras se doblan y la cola rodea.
        'sit': {'loop': True, 'animation_length': 6, 'bones': {
            'body': {'rotation': [-32, 0, 0], 'position': [0, -1.8, 1], 'scale': [1, osc(90, 0.015, 0, 1), 1]},
            'legFL': {'rotation': [32, 0, 0]}, 'legFR': {'rotation': [32, 0, 0]},
            'legBL': {'rotation': [-30, 0, 0]}, 'legBR': {'rotation': [-30, 0, 0]}, 'legBL2': {'rotation': [90, 0, 0]}, 'legBR2': {'rotation': [90, 0, 0]},
            'head': {'rotation': [osc(30, 4, 0, 28), 0, osc(21, 6)]},
            'tail0': {'rotation': [-40, 45, 0]}, 'tail1': {'rotation': [0, 40, 0]}, 'tail2': {'rotation': [0, 40, 0]}, 'tail3': {'rotation': [0, osc(90, 25, 0, 30), 0]},
            **ears, 'lids': {'scale': blink(5.0)}}},
        'sleep': {'loop': True, 'animation_length': 1.8, 'bones': {
            'body': {'position': [0, -2.6, 0], 'scale': [1, osc(70, 0.035, 0, 1), 1]},
            'legFL': {'rotation': [-80, 0, 0]}, 'legFR': {'rotation': [-80, 0, 0]}, 'legBL': {'rotation': [80, 0, 0]}, 'legBR': {'rotation': [80, 0, 0]},
            'head': {'rotation': [22, 28, 10], 'position': [0, -1.5, 0.5]},
            **{f'tail{i}': {'rotation': [0, -40 - i * 10, 0]} for i in range(4)},
            'earL': {'rotation': [18, 0, 0]}, 'earR': {'rotation': [18, 0, 0]}, 'lids': {'scale': [1, 1, 1]}},
            'particle_effects': {'0.0': {'effect': 'zzz', 'locator': 'zzz'}}},
        # Agachado: acecha pegado al suelo con la punta de la cola nerviosa.
        'sneak': {'loop': True, 'animation_length': 2, 'bones': {
            'body': {'position': [0, -1.4, 0]}, **{f'leg{l}': {'rotation': [-25, 0, 0]} for l in legs}, **{f'leg{l}2': {'rotation': [45, 0, 0]} for l in legs},
            'head': {'rotation': [8, 0, 0]}, 'tail0': {'rotation': [-20, 0, 0]}, 'tail3': {'rotation': [0, osc(700, 25), 0]}, 'lids': {'scale': blink(1.6)}},
            'particle_effects': {'0.0': {'effect': 'hearts', 'locator': 'hearts'}}},
        # Te hacen dano: bufa con el lomo arqueado, cola erizada y orejas atras.
        'hurt': {'loop': False, 'animation_length': 1.2, 'bones': {
            'root': {'position': [osc(2000, 0.3), 0, 0]},
            'body': {'rotation': [-6, 0, 0], 'position': [0, 0.8, 0], 'scale': {'0.0': [1, 1, 1], '0.15': [0.96, 1.2, 1.0], '1.0': [0.96, 1.18, 1.0], '1.2': [1, 1, 1]}},
            'head': {'rotation': [-12, 0, 0]}, 'tail0': {'rotation': [80, 0, 0], 'scale': [1.4, 1.4, 1]}, 'tail1': {'rotation': [0, 0, 0], 'scale': [1.3, 1.3, 1]},
            'earL': {'rotation': [50, 0, -20]}, 'earR': {'rotation': [50, 0, 20]}, 'lids': {'scale': [0, 0, 0]}}},
    }
    return {'format_version': '1.8.0', 'animations': {f'animation.cat.{k}': v for k, v in anims.items()}}


# ------------------------------------------------------------------ fantasma
def ghost_model():
    m = Model(128, 64)
    m.bone('root', pivot=(0, 8, 0), locators={'zzz': [4, 20, -4], 'wisp': [0, 18, 0]})
    m.bone('body', 'root', (0, 8, 0))
    m.cube('body', (-4, 6, -4), (8, 9, 8), 'sheet', {'north': 'face'})
    m.cube('body', (-1, 15, -1), (2, 1, 2), 'sheet')
    for i, x in enumerate((-4, -2, 0, 2)):
        b = m.bone(f'strip{i}', 'body', (x + 1, 6, 0))
        m.cube(b, (x, 3.5, -4), (2, 2.5, 8), 'sheet')
        c = m.bone(f'strip{i}b', b, (x + 1, 3.5, 0))
        m.cube(c, (x, 1, -4), (2, 2.5, 8), 'tatter')
    for s, side in ((1, 'L'), (-1, 'R')):
        a = m.bone(f'arm{side}', 'body', (s * 4, 12, 0))
        m.cube(a, (s * 4 - (0 if s > 0 else 2), 10, -1), (2, 2, 2), 'sheet')
        h = m.bone(f'hand{side}', a, (s * 5, 10, 0))
        m.cube(h, (s * 5 - (0 if s > 0 else 2) + (0 if s > 0 else 1), 8, -1), (1.6 if s > 0 else 1.6, 2, 2), 'sheet')
    m.bone('lids', 'body', (0, 11, -4))
    m.cube('lids', (-3, 10, -4.02), (6, 2, 0), 'lid')
    return m


GHOSTS = {'': ('blanco', 0xe8eef8, 0x9ab0d8)}


def ghost_anims():
    strips = lambda sp, amp, base=0: {**{f'strip{i}': {'rotation': [osc(sp, amp, i * 70, base), 0, 0]} for i in range(4)},
                                      **{f'strip{i}b': {'rotation': [osc(sp, amp * 1.3, i * 70 + 60), 0, 0]} for i in range(4)}}
    arms = lambda sp, amp, base=0: {'armL': {'rotation': [0, 0, osc(sp, amp, 0, base)]}, 'armR': {'rotation': [0, 0, f'-({osc(sp, amp, 0, base)})']},
                                    'handL': {'rotation': [0, 0, osc(sp, amp, 50)]}, 'handR': {'rotation': [0, 0, f'-({osc(sp, amp, 50)})']}}
    anims = {
        'look': {'loop': True, 'bones': {'body': {'rotation': ['math.clamp(query.target_x_rotation, -30, 30) * 0.3', 'math.clamp(query.target_y_rotation, -60, 60) * 0.5', 0]}}},
        'idle': {'loop': True, 'animation_length': 8, 'bones': {
            'root': {'position': [0, osc(100, 1.5), 0], 'rotation': [0, 'math.clamp((math.mod(query.anim_time, 8) - 6.2) * 400, 0, 360)', osc(60, 5)]},
            **strips(200, 14), **arms(120, 20), 'lids': {'scale': blink(3.5)}},
            'particle_effects': {'1.0': {'effect': 'wisp', 'locator': 'wisp'}, '5.0': {'effect': 'wisp', 'locator': 'wisp'}}},
        'walk': {'loop': True, 'animation_length': 8, 'bones': {
            'root': {'position': [0, osc(160, 1), 0], 'rotation': [16, 0, osc(90, 4)]}, **strips(320, 10, 28), **arms(200, 10, 30), 'lids': {'scale': blink(3.5)}}},
        'run': {'loop': True, 'animation_length': 8, 'bones': {
            'root': {'position': [0, osc(200, 1), 0], 'rotation': [26, 0, osc(120, 5)]}, **strips(420, 8, 40), **arms(300, 10, 50), 'lids': {'scale': blink(3.5)}}},
        'fly': {'loop': True, 'animation_length': 8, 'bones': {
            'root': {'position': [0, osc(200, 2), 0], 'rotation': [6, 'query.anim_time * 360', 0]}, **strips(380, 20, 10), **arms(300, 10, 70), 'lids': {'scale': [0, 0, 0]}}},
        'sit': {'loop': True, 'animation_length': 8, 'bones': {
            'root': {'position': [0, osc(60, 1, 0, -2), 0], 'rotation': [0, 0, osc(30, 8)]}, **strips(90, 8, 20), **arms(60, 8, -10), 'lids': {'scale': blink(5)}}},
        'sneak': {'loop': True, 'animation_length': 4, 'bones': {
            'root': {'position': [6, -4, 0]}, 'body': {'scale': [1.1, 0.8, 1.1], 'rotation': [10, 0, osc(40, 6)]}, **strips(80, 6, 30), **arms(60, 6, -40), 'lids': {'scale': blink(3)}}},
        'hurt': {'loop': False, 'animation_length': 1.2, 'bones': {
            'root': {'position': {'0.0': [0, 0, 0], '0.3': [-30, 4, 20], '0.9': [-30, 4, 20], '1.2': [0, 0, 0]}, 'rotation': [0, osc(2200, 6), 0]},
            **strips(600, 20), **arms(900, 30, 60), 'lids': {'scale': [0, 0, 0]}}},
        'sleep': {'loop': True, 'animation_length': 1.8, 'bones': {
            'root': {'position': [0, osc(40, 1, 0, -3), 0], 'rotation': [8, 0, 10]}, 'body': {'scale': [1, osc(70, 0.03, 0, 1), 1]},
            **strips(60, 6), **arms(40, 4, -20), 'lids': {'scale': [1, 1, 1]}},
            'particle_effects': {'0.0': {'effect': 'zzz', 'locator': 'zzz'}}},
    }
    return {'format_version': '1.8.0', 'animations': {f'animation.ghost.{k}': v for k, v in anims.items()}}


def ghost_painter(body, shade):
    pal = ramp(body, spread=0.18)
    dark = ramp(shade, spread=0.2)

    def paint(mat, face, w, h, cube, rng):
        idx = side_light(w, h, face) + (rng.random((h, w)) < 0.08).astype(int)
        if mat in ('sheet', 'face'):
            img = tones(idx, pal)
            if face != 'up':
                img[-1, :, :3] = dark[2]
            if mat == 'face':
                for x0 in (1, w - 3):
                    for y in (3, 4, 5):
                        put(img, x0, y, np.array([24, 26, 44])); put(img, x0 + 1, y, np.array([24, 26, 44]))
                    put(img, x0 + 1, 3, np.array([255, 255, 255]))
                put(img, w // 2 - 1, 7, np.array([40, 30, 60])); put(img, w // 2, 7, np.array([40, 30, 60]))
                put(img, 0, 6, np.array([255, 150, 180])); put(img, w - 1, 6, np.array([255, 150, 180]))
            return img
        if mat == 'tatter':
            img = tones(np.clip(idx - 1, 0, 4), pal)
            img[-2:, :, :3] = dark[2]
            for x in range(w):
                cut = h - (1 + (x * 7 + int(cube['origin'][0] * 3)) % 3)
                img[cut:, x, 3] = 0
            return img
        if mat == 'lid':
            img = tones(idx, pal)
            img[:, :, 3] = 0
            for x in (0, 1, w - 2, w - 1):
                img[1, x, :3] = (24, 26, 44)
                img[:, x, 3] = 255
            return img
        return tones(idx, pal)
    return paint


# ------------------------------------------------------------------ particulas
def flipbook(frames, fn, size=64, blur=0.04):
    sheet = Image.new('RGBA', (size * frames, size), (0, 0, 0, 0))
    for i in range(frames):
        im = Image.new('RGBA', (size * 4, size * 4), (0, 0, 0, 0))
        fn(ImageDraw.Draw(im), size * 4, i / max(1, frames - 1), im)
        if blur:
            im = im.filter(ImageFilter.GaussianBlur(size * blur))
        sheet.paste(im.resize((size, size), Image.LANCZOS), (i * size, 0))
    return sheet


def flame_frame(d, s, t, im):
    r = s * (0.22 + t * 0.2)
    rng = np.random.default_rng(int(t * 100) + 5)
    for k in range(9):
        a, dd = rng.random() * math.tau, rng.random() * r * 0.6
        x, y = s / 2 + math.cos(a) * dd, s / 2 + math.sin(a) * dd
        rr = r * (0.5 + rng.random() * 0.5)
        g = int(255 * (1 - t * 0.6))
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, g, g, int(255 * (1 - t * 0.35))))
    d.ellipse([s / 2 - r * 0.5, s / 2 - r * 0.5, s / 2 + r * 0.5, s / 2 + r * 0.5], fill=(255, 255, 255, int(255 * (1 - t))))


def frost_frame(d, s, t, im):
    rng = np.random.default_rng(int(t * 100) + 9)
    r = s * (0.18 + t * 0.26)
    for k in range(7):
        a, dd = rng.random() * math.tau, rng.random() * r * 0.7
        x, y = s / 2 + math.cos(a) * dd, s / 2 + math.sin(a) * dd
        rr = r * (0.45 + rng.random() * 0.4)
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255, int(200 * (1 - t * 0.5))))
    for k in range(6):
        a = k / 6 * math.tau + t
        L = s * 0.28 * (1 - t * 0.4)
        d.line([(s / 2, s / 2), (s / 2 + math.cos(a) * L, s / 2 + math.sin(a) * L)], fill=(255, 255, 255, 255), width=int(s * 0.03))


def spark_frame(d, s, t, im):
    rng = np.random.default_rng(int(t * 100) + 13)
    for k in range(3):
        pts = [(s / 2, s / 2)]
        a = rng.random() * math.tau
        for j in range(5):
            a += rng.uniform(-1, 1)
            L = s * 0.09 * (1 - t * 0.5)
            pts.append((pts[-1][0] + math.cos(a) * L, pts[-1][1] + math.sin(a) * L))
        d.line(pts, fill=(255, 255, 255, int(255 * (1 - t * 0.6))), width=int(s * 0.035), joint='curve')
    r = s * 0.08 * (1 - t)
    d.ellipse([s / 2 - r, s / 2 - r, s / 2 + r, s / 2 + r], fill=(255, 255, 255, 255))


def heart_frame(d, s, t, im):
    r = s * 0.16
    a = int(255 * min(1, (1 - t) * 1.8))
    cx, cy = s / 2, s / 2
    d.ellipse([cx - 2 * r, cy - r * 1.4, cx, cy + r * 0.6], fill=(255, 255, 255, a))
    d.ellipse([cx, cy - r * 1.4, cx + 2 * r, cy + r * 0.6], fill=(255, 255, 255, a))
    d.polygon([(cx - 2 * r + r * 0.15, cy), (cx + 2 * r - r * 0.15, cy), (cx, cy + r * 2.2)], fill=(255, 255, 255, a))


def wisp_frame(d, s, t, im):
    a = int(255 * (1 - t))
    w = s * (0.16 - t * 0.06)
    d.polygon([(s / 2, s * (0.15 + t * 0.1)), (s / 2 + w, s * 0.6), (s / 2, s * 0.8), (s / 2 - w, s * 0.6)], fill=(255, 255, 255, a))
    d.ellipse([s / 2 - w * 0.9, s * 0.5, s / 2 + w * 0.9, s * 0.82], fill=(255, 255, 255, a))


def z_frame(d, s, t, im):
    w = s * 0.42
    x, y = s / 2 - w / 2, s / 2 - w / 2
    d.line([(x, y), (x + w, y), (x, y + w), (x + w, y + w)], fill=(255, 255, 255, int(255 * min(1, (1 - t) * 1.6))), width=int(s * 0.07), joint='curve')


def particle(ident, tint, rate=48, active=1.3, speed='math.random(3.2, 4.6)', life='math.random(0.45, 0.75)', size='0.07 + variable.particle_age * 0.85',
             accel=(0, 1.6, 0), drag=2.4, local_rot=True, direction=('math.random(-0.18, 0.18)', 'math.random(-0.3, 0.05)', -1), instant=None):
    comps = {
        'minecraft:emitter_local_space': {'position': True, 'rotation': local_rot},
        **({'minecraft:emitter_rate_instant': {'num_particles': instant}} if instant else {'minecraft:emitter_rate_steady': {'spawn_rate': rate, 'max_particles': 120}}),
        'minecraft:emitter_lifetime_once': {'active_time': active},
        'minecraft:emitter_shape_point': {'direction': list(direction)},
        'minecraft:particle_initial_speed': speed,
        'minecraft:particle_lifetime_expression': {'max_lifetime': life},
        'minecraft:particle_motion_dynamic': {'linear_acceleration': list(accel), 'linear_drag_coefficient': drag},
        'minecraft:particle_initial_spin': {'rotation': 'math.random(0, 360)', 'rotation_rate': 'math.random(-90, 90)'},
        'minecraft:particle_appearance_billboard': {'size': [size, size], 'facing_camera_mode': 'rotate_xyz',
                                                    'uv': {'texture_width': 512, 'texture_height': 64, 'flipbook': {'base_UV': [0, 0], 'size_UV': [64, 64], 'step_UV': [64, 0], 'max_frame': 8, 'stretch_to_lifetime': True}}},
        'minecraft:particle_appearance_tinting': {'color': {'interpolant': 'variable.particle_age / variable.particle_lifetime', 'gradient': tint}},
    }
    return {'format_version': '1.10.0', 'particle_effect': {'description': {'identifier': ident, 'basic_render_parameters': {'material': 'particles_blend', 'texture': 'x'}}, 'components': comps}}


BREATH = {
    'fire': (particle('lex:fire', {'0.0': '#FFFFF6C8', '0.2': '#FFFFC040', '0.45': '#FFFF6A18', '0.7': '#E0B02810', '1.0': '#00302826'}), flame_frame),
    'frost': (particle('lex:frost', {'0.0': '#FFFFFFFF', '0.3': '#FFC8F0FF', '0.7': '#B090C8F0', '1.0': '#0080A8D8'}, speed='math.random(2.4, 3.4)', life='math.random(0.7, 1.0)',
                       size='0.08 + variable.particle_age * 0.7', accel=(0, -0.4, 0), drag=1.8), frost_frame),
    'spark': (particle('lex:spark', {'0.0': '#FFFFFFFF', '0.3': '#FFFFF070', '0.6': '#FFB070FF', '1.0': '#006030C0'}, rate=70, speed='math.random(4.5, 6)', life='math.random(0.25, 0.45)',
                       size='0.1 - variable.particle_age * 0.05', accel=(0, 0, 0), drag=1.2), spark_frame),
}
ZZZ = particle('lex:zzz', {'0.0': '#00FFFFFF', '0.15': '#FFE8F0FF', '0.8': '#C0C8D8FF', '1.0': '#00C8D8FF'}, instant=1, active=0.1, speed=0.35, life=1.7,
               size='0.06 + variable.particle_age * 0.06', accel=(0.05, 0.1, 0), drag=0, local_rot=False, direction=(0.3, 1, 0))
HEARTS = particle('lex:hearts', {'0.0': '#00FF5A80', '0.15': '#FFFF5A80', '0.8': '#E0FF80A0', '1.0': '#00FF80A0'}, instant=1, active=0.1, speed=0.5, life=1.3,
                  size='0.07', accel=(0, 0.4, 0), drag=0.5, local_rot=False, direction=('math.random(-0.3, 0.3)', 1, 0))
WISP = particle('lex:wisp', {'0.0': '#0060D0FF', '0.2': '#E060D0FF', '0.7': '#A0A0E8FF', '1.0': '#0080C0FF'}, instant=3, active=0.1, speed='math.random(0.2, 0.5)', life=1.8,
                size='0.08 - variable.particle_age * 0.03', accel=(0, 0.3, 0), drag=0.6, local_rot=False, direction=('math.random(-1, 1)', 1, 'math.random(-1, 1)'))


# ------------------------------------------------------------------ escribir
def write(folder, meta, model, painter_for, palettes, anims, particles=()):
    out = os.path.join(ROOT, folder)
    os.makedirs(os.path.join(out, 'particles'), exist_ok=True)
    for f in os.listdir(out):
        if f.endswith('.png'):
            os.remove(os.path.join(out, f))
    with open(os.path.join(out, 'model.geo.json'), 'w', encoding='utf8') as f:
        json.dump(model.geo(f'geometry.{meta["id"]}'), f, indent=1)
    with open(os.path.join(out, 'animation.json'), 'w', encoding='utf8') as f:
        json.dump(anims, f, indent=1)
    variants = []
    for key, pal in palettes.items():
        name = 'texture.png' if not key else f'texture_{key}.png'
        model.paint(painter_for(pal)).save(os.path.join(out, name))
        variants.append({'key': key, 'name': pal[0], **({'texture': name} if key else {})})
    meta = dict(meta, variants=variants, particles=[])
    for name, json_, fn in particles:
        with open(os.path.join(out, 'particles', f'{name}.json'), 'w', encoding='utf8') as f:
            json.dump(json_, f, indent=1)
        flipbook(8, fn).save(os.path.join(out, 'particles', f'{name}.png'))
        meta['particles'].append({'name': name, 'file': f'particles/{name}.json', 'texture': f'particles/{name}.png', 'frames': 8})
    with open(os.path.join(out, 'cosmetic.json'), 'w', encoding='utf8') as f:
        json.dump(meta, f, indent=1, ensure_ascii=False)
    print(folder, 'caras', len(model.regions))


def states(prefix, names):
    return {k: [f'animation.{prefix}.{k}'] for k in names}


def main():
    import shutil
    for old in ('wyvern', 'dragonfire', 'dragonice', 'dragonstorm'):
        path = os.path.join(ROOT, old)
        if os.path.isdir(path):
            shutil.rmtree(path)
    every = ['idle', 'walk', 'run', 'fly', 'sit', 'sneak', 'sleep', 'hurt']
    breath_json, breath_fn = BREATH['fire']
    write('dragon', {
        'id': 'dragon', 'slot': 'pet', 'name': 'Dragón', 'scale': 0.4, 'offset': [-20, 18, 2],
        'sitAfter': 6, 'sleepAfter': 25, 'breathEvery': 20, 'breathTime': 2.4, 'hiddenAtRest': ['lids'], 'preview': 'idle',
        'states': states('dragon', every + ['breath']) | {'always': ['animation.dragon.look']},
    }, dragon_model(), lambda p: dragon_painter(p[1], p[2], p[3], p[4]), DRAGONS, dragon_anims(), [('breath', breath_json, breath_fn)])
    write('cat', {
        'id': 'cat', 'slot': 'pet', 'name': 'Gato', 'scale': 0.5, 'offset': [-16, 0, 4], 'sitAfter': 4, 'sleepAfter': 20,
        'hiddenAtRest': ['lids'], 'preview': 'walk',
        'states': states('cat', every) | {'always': ['animation.cat.look']},
    }, cat_model(), lambda p: cat_painter(p[1], p[2], p[3]), CATS, cat_anims(), [('zzz', ZZZ, z_frame), ('hearts', HEARTS, heart_frame)])
    write('ghost', {
        'id': 'ghost', 'slot': 'pet', 'name': 'Fantasma', 'scale': 0.6, 'offset': [-18, 16, 2], 'sitAfter': 6, 'sleepAfter': 25,
        'hiddenAtRest': ['lids'], 'preview': 'idle',
        'states': states('ghost', every) | {'always': ['animation.ghost.look']},
    }, ghost_model(), lambda p: ghost_painter(p[1], p[2]), GHOSTS, ghost_anims(), [('wisp', WISP, wisp_frame), ('zzz', ZZZ, z_frame)])


main()
