"""Skin por defecto de LeX (original) para la vista previa del APK cuando aun
no hay skin del jugador. Uso: python tools/make_default_skin.py"""
from PIL import Image

SKIN = (222, 170, 135)
HAIR = (38, 30, 28)
HOOD = (22, 34, 58)
ICE = (96, 192, 255)
PANTS = (30, 30, 40)
SHOE = (90, 96, 110)

img = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
px = img.load()

def fill(x0, y0, x1, y1, c):
    for y in range(y0, y1):
        for x in range(x0, x1):
            px[x, y] = c + (255,)

# Cabeza: piel, pelo arriba y detras, cara delante.
fill(0, 0, 32, 16, SKIN)
fill(8, 0, 16, 8, HAIR)
fill(24, 8, 32, 16, HAIR)
for x0 in (0, 16):
    fill(x0, 8, x0 + 8, 11, HAIR)
fill(8, 8, 16, 10, HAIR)
for x in (9, 13):
    fill(x, 12, x + 2, 13, (255, 255, 255))
    fill(x + (1 if x == 9 else 0), 12, x + (2 if x == 9 else 1), 13, (40, 90, 160))
fill(11, 14, 13, 15, (170, 110, 90))
# Cuerpo y brazos: sudadera con franja de color.
fill(16, 16, 40, 32, HOOD)
fill(23, 20, 25, 32, ICE)
fill(40, 16, 56, 32, HOOD)
fill(32, 48, 48, 64, HOOD)
fill(44, 28, 48, 32, SKIN)
fill(40, 28, 56, 32, SKIN)
fill(32, 60, 48, 64, SKIN)
fill(40, 26, 56, 27, ICE)
fill(32, 58, 48, 59, ICE)
# Piernas y zapatillas.
fill(0, 16, 16, 32, PANTS)
fill(16, 48, 32, 64, PANTS)
fill(0, 29, 16, 32, SHOE)
fill(16, 61, 32, 64, SHOE)
img.save('android/ui/default-skin.png')
print('ok')
