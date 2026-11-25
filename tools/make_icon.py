"""Icono del programa a partir de renderer/logo.png (python tools/make_icon.py).

Solo la silueta del felino, sin fondo y ocupando todo el lienzo.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

R = os.path.join(os.path.dirname(__file__), '..', 'launcher', 'renderer')
src = Image.open(os.path.join(R, 'logo.png')).convert('RGB')
lum = np.array(src).max(-1)
art = Image.fromarray((lum > 30).astype(np.uint8) * 255)

# Silueta: cierra las lineas negras entre piezas sin unir las orejas.
def disk(mask, r, grow):
    """Dilata o erosiona con un disco (convolucion por FFT)."""
    h, w = mask.shape
    yy, xx = np.ogrid[:h, :w]
    kernel = ((np.minimum(yy, h - yy) ** 2 + np.minimum(xx, w - xx) ** 2) <= r * r).astype(float)
    hits = np.fft.irfft2(np.fft.rfft2(mask if grow else ~mask) * np.fft.rfft2(kernel), s=mask.shape) > 0.5
    return hits if grow else ~hits


closed = disk(disk(np.array(art) > 0, 25, True), 24, False)
shape = Image.fromarray((closed * 255).astype(np.uint8))
# Rellena lo que quede encerrado.
pad = Image.new('L', (shape.width + 2, shape.height + 2))
pad.paste(shape, (1, 1))
ImageDraw.floodfill(pad, (0, 0), 128)
shape = Image.fromarray(((np.array(pad) != 128) * 255).astype(np.uint8)).crop((1, 1, shape.width + 1, shape.height + 1))
alpha = shape.filter(ImageFilter.GaussianBlur(1.2))

icon = np.dstack([np.array(src), np.array(alpha)])
dark = lum < 30  # el negro del dibujo pasa a azul noche, visible en cualquier fondo
icon[dark, :3] = (7, 12, 24)
icon = Image.fromarray(icon)
box = shape.getbbox()
side = max(box[2] - box[0], box[3] - box[1])
cx, cy = (box[0] + box[2]) // 2, (box[1] + box[3]) // 2
icon = icon.crop((cx - side // 2, cy - side // 2, cx - side // 2 + side, cy - side // 2 + side))
big = icon.resize((1024, 1024), Image.LANCZOS)
big.resize((512, 512), Image.LANCZOS).save(os.path.join(R, 'icon.png'))
sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
frames = [big.resize((s, s), Image.LANCZOS) for s in sizes]
frames[-1].save(os.path.join(R, 'icon.ico'), sizes=[(s, s) for s in sizes], append_images=frames[:-1])
