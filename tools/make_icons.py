"""Iconos de los mods: solidos y blancos (python tools/make_icons.py).

Un PNG de 96 px por mod; el nombre es el del mod sin tildes ni espacios.
"""
import math
import os
import unicodedata

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), '..', 'launcher', 'assets', 'icons')
os.makedirs(OUT, exist_ok=True)
S = 384
W = (255, 255, 255, 255)
G = (255, 255, 255, 120)  # segundo tono
C = (0, 0, 0, 0)
TAU = math.tau


class Pen:
    def __init__(self):
        self.im = Image.new('RGBA', (S, S))
        self.d = ImageDraw.Draw(self.im)

    def v(self, *pts):
        return [(x * S, y * S) for x, y in pts]

    def poly(self, *pts, c=W):
        self.d.polygon(self.v(*pts), fill=c)

    def rect(self, x0, y0, x1, y1, r=0.0, c=W, w=0.0):
        box = [x0 * S, y0 * S, x1 * S, y1 * S]
        if w:
            self.d.rounded_rectangle(box, radius=r * S, outline=c, width=int(w * S))
        else:
            self.d.rounded_rectangle(box, radius=r * S, fill=c)

    def disc(self, x, y, r, c=W, w=0.0):
        box = [(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S]
        if w:
            self.d.ellipse(box, outline=c, width=int(w * S))
        else:
            self.d.ellipse(box, fill=c)

    def line(self, *pts, w=0.07, c=W):
        p = self.v(*pts)
        self.d.line(p, fill=c, width=int(w * S), joint='curve')
        r = w * S / 2
        for x, y in (p[0], p[-1]):
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=c)

    def arc(self, x, y, r, a0, a1, w=0.07, c=W):
        self.d.arc([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], a0, a1, fill=c, width=int(w * S))

    def heart(self, x, y, s, c=W):
        pts = []
        for i in range(80):
            t = TAU * i / 80
            pts.append((x + s * 16 * math.sin(t) ** 3 / 17, y - s * (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)) / 17))
        self.poly(*pts, c=c)

    def star(self, x, y, r, n=5, k=0.45, c=W, turn=-math.pi / 2):
        self.poly(*[(x + r * (1 if i % 2 == 0 else k) * math.cos(turn + TAU * i / (2 * n)), y + r * (1 if i % 2 == 0 else k) * math.sin(turn + TAU * i / (2 * n))) for i in range(2 * n)], c=c)

    def cube(self, x, y, s):
        h = s * 0.5
        self.poly((x, y - s), (x + s * .87, y - h), (x, y), (x - s * .87, y - h), c=W)
        self.poly((x - s * .87, y - h + .02), (x - .012, y + .012), (x - .012, y + s), (x - s * .87, y + h), c=(255, 255, 255, 190))
        self.poly((x + s * .87, y - h + .02), (x + .012, y + .012), (x + .012, y + s), (x + s * .87, y + h), c=G)

    def sword(self, flip=1, c=W):
        f = lambda x, y: (0.5 + flip * (x - 0.5), y)
        self.poly(f(.86, .10), f(.90, .14), f(.46, .60), f(.38, .52), c=c)
        self.poly(f(.86, .10), f(.74, .12), f(.36, .50), f(.40, .54), c=c)
        self.line(f(.28, .50), f(.48, .70), w=.075, c=c)
        self.line(f(.36, .62), f(.20, .78), w=.085, c=c)
        self.disc(*f(.17, .81), .055, c=c)

    def player(self, x, y, s, c=W):
        self.rect(x - s * .42, y - s, x + s * .42, y - s * .16, s * .1, c)
        self.rect(x - s * .62, y - s * .06, x + s * .62, y + s * .62, s * .14, c)


ICONS = {}


def icon(name):
    def wrap(f):
        ICONS[name] = f
        return f
    return wrap


@icon('FPS')
def _(p):
    p.rect(.06, .12, .94, .72, .07); p.rect(.14, .20, .86, .64, .03, C)
    p.line((.20, .52), (.34, .40), (.46, .50), (.62, .30), (.80, .36), w=.055)
    p.rect(.42, .72, .58, .82); p.rect(.28, .82, .72, .90, .04)


@icon('FPS mínimos')
def _(p):
    p.rect(.06, .12, .94, .72, .07); p.rect(.14, .20, .86, .64, .03, C)
    p.line((.22, .28), (.40, .34), (.54, .54), (.64, .46), (.78, .54), w=.055)
    p.rect(.42, .72, .58, .82); p.rect(.28, .82, .72, .90, .04)


@icon('CPS')
def _(p):
    p.rect(.24, .06, .76, .94, .26); p.rect(.47, .10, .53, .46, 0, C); p.rect(.24, .44, .76, .50, 0, C)
    p.rect(.53, .06, .76, .44, 0, G); p.im.paste(C, (int(.53 * S), int(.06 * S), int(.53 * S) + 1, int(.07 * S)))
    q = Pen(); q.rect(.24, .06, .76, .94, .26)
    a = p.im.getchannel('A'); from PIL import ImageChops
    p.im.putalpha(ImageChops.darker(a, q.im.getchannel('A')))


@icon('Teclas')
def _(p):
    p.rect(.355, .10, .645, .44, .05); p.rect(.04, .50, .33, .84, .05, G); p.rect(.355, .50, .645, .84, .05); p.rect(.67, .50, .96, .84, .05, G)
    p.line((.42, .20), (.46, .34), (.50, .25), (.54, .34), (.58, .20), w=.03, c=C)


@icon('Armadura')
def _(p):
    p.poly((.10, .18), (.32, .08), (.40, .18), (.60, .18), (.68, .08), (.90, .18), (.94, .42), (.78, .46), (.78, .92), (.22, .92), (.22, .46), (.06, .42))
    p.poly((.5, .34), (.64, .42), (.60, .66), (.5, .76), (.40, .66), (.36, .42), c=C); p.poly((.5, .42), (.57, .46), (.55, .62), (.5, .67), c=G)


@icon('Combo')
def _(p):
    p.sword(1); p.sword(-1, G); p.sword(1)


@icon('Alcance')
def _(p):
    p.rect(.04, .36, .96, .64, .05)
    for i in range(7):
        x = .13 + i * .123; p.rect(x - .018, .36, x + .018, .53 if i % 2 == 0 else .47, 0, C)
    p.line((.10, .80), (.90, .80), w=.04, c=G); p.poly((.04, .80), (.16, .73), (.16, .87), c=G); p.poly((.96, .80), (.84, .73), (.84, .87), c=G)


@icon('Bloques')
def _(p):
    p.cube(.5, .5, .46)


@icon('Vida')
def _(p):
    p.heart(.5, .48, .46); p.heart(.36, .34, .10, G)


@icon('Coordenadas')
def _(p):
    p.poly((.5, .96), (.22, .52), (.78, .52)); p.disc(.5, .36, .30); p.disc(.5, .36, .12, C)


@icon('Rumbo')
def _(p):
    p.disc(.5, .5, .45, W, .07); p.poly((.5, .12), (.62, .5), (.38, .5)); p.poly((.5, .88), (.62, .5), (.38, .5), c=G); p.disc(.5, .5, .05, C)


@icon('Velocidad')
def _(p):
    p.arc(.5, .58, .44, 180, 360, .09); p.line((.5, .58), (.74, .34), w=.07); p.disc(.5, .58, .08)
    for a in (180, 225, 270, 315, 360):
        x, y = math.cos(math.radians(a)), math.sin(math.radians(a)); p.line((.5 + x * .28, .58 + y * .28), (.5 + x * .32, .58 + y * .32), w=.04, c=G)
    p.rect(.30, .74, .70, .86, .05, G)


@icon('Precisión')
def _(p):
    p.disc(.46, .54, .40, W, .08); p.disc(.46, .54, .22, W, .07); p.disc(.46, .54, .07)
    p.line((.46, .54), (.80, .20), w=.055); p.poly((.74, .26), (.74, .10), (.90, .02), (.90, .18), (.98, .10), (.98, .26), (.82, .26))


@icon('Tiempo de combate')
def _(p):
    p.disc(.5, .56, .38); p.disc(.5, .56, .29, C); p.line((.5, .56), (.5, .36), w=.06); p.line((.5, .56), (.64, .64), w=.06); p.rect(.40, .04, .60, .12, .03); p.rect(.46, .10, .54, .20); p.line((.80, .20), (.88, .28), w=.07)


@icon('Reloj')
def _(p):
    p.disc(.5, .5, .46); p.disc(.5, .5, .36, C); p.line((.5, .5), (.5, .24), w=.07); p.line((.5, .5), (.68, .60), w=.07); p.disc(.5, .5, .05)


@icon('Kills')
def _(p):
    p.disc(.5, .42, .36); p.rect(.30, .60, .70, .88, .06); p.disc(.36, .44, .11, C); p.disc(.64, .44, .11, C); p.poly((.5, .54), (.56, .66), (.44, .66), c=C)
    for x in (.40, .5, .60): p.rect(x - .018, .74, x + .018, .90, 0, C)


@icon('Crosshair')
def _(p):
    for a, b in (((.44, .04), (.56, .38)), ((.44, .62), (.56, .96)), ((.04, .44), (.38, .56)), ((.62, .44), (.96, .56))): p.rect(*a, *b, .03)
    p.disc(.5, .5, .05, G)


@icon('Vida de jugadores')
def _(p):
    p.heart(.5, .22, .22); p.player(.5, .76, .30)


@icon('Borde de bloque')
def _(p):
    x, y, s = .5, .5, .44; h = s * .5
    o = [(x, y - s), (x + s * .87, y - h), (x + s * .87, y + h), (x, y + s), (x - s * .87, y + h), (x - s * .87, y - h)]
    p.line(*o, o[0], w=.06); p.line((x - s * .87, y - h), (x, y), (x + s * .87, y - h), w=.05, c=G); p.line((x, y), (x, y + s), w=.05, c=G)


@icon('Brillo total')
def _(p):
    p.disc(.5, .5, .22)
    for i in range(8):
        a = TAU * i / 8; p.line((.5 + .32 * math.cos(a), .5 + .32 * math.sin(a)), (.5 + .43 * math.cos(a), .5 + .43 * math.sin(a)), w=.075)


@icon('Campo de visión')
def _(p):
    p.poly((.5, .86), (.06, .22), (.94, .22), c=G); p.arc(.5, .86, .72, 235, 305, .06); p.disc(.5, .82, .13); p.line((.5, .82), (.08, .22), w=.05); p.line((.5, .82), (.92, .22), w=.05)


@icon('Sin sacudida')
def _(p):
    p.rect(.08, .28, .72, .78, .08); p.poly((.72, .46), (.94, .32), (.94, .74), (.72, .60)); p.disc(.40, .53, .14, C); p.disc(.40, .53, .07, G); p.rect(.18, .18, .42, .28, .03)


@icon('Sin balanceo')
def _(p):
    p.line((.06, .40), (.22, .28), (.40, .40), (.60, .28), (.78, .40), (.94, .30), w=.06, c=G); p.line((.06, .62), (.94, .62), w=.09); p.line((.06, .80), (.94, .80), w=.09)


@icon('Aviso de vida baja')
def _(p):
    p.heart(.5, .50, .46); p.rect(.455, .30, .545, .58, .04, C); p.disc(.5, .68, .055, C)


@icon('Lista de jugadores')
def _(p):
    for i, y in enumerate((.10, .40, .70)):
        p.rect(.06, y, .28, y + .22, .04); p.rect(.36, y + .03, .94 - i * .10, y + .19, .05, G if i else W)


@icon('Física de capa')
def _(p):
    p.poly((.30, .08), (.70, .08), (.80, .30), (.94, .86), (.74, .78), (.58, .94), (.42, .80), (.24, .92), (.10, .78), (.22, .34))
    p.line((.42, .24), (.36, .74), w=.04, c=C); p.line((.60, .24), (.66, .74), w=.04, c=C); p.rect(.26, .04, .74, .14, .05)


@icon('Física de objetos')
def _(p):
    p.cube(.5, .36, .27); p.line((.08, .80), (.92, .80), w=.07); p.arc(.5, .36, .40, 150, 220, .05, G); p.arc(.5, .36, .40, -40, 30, .05, G); p.line((.24, .92), (.76, .92), w=.05, c=G)


@icon('Marcador de golpe')
def _(p):
    for sx in (-1, 1):
        for sy in (-1, 1): p.line((.5 + sx * .16, .5 + sy * .16), (.5 + sx * .40, .5 + sy * .40), w=.10)


@icon('Color de golpe')
def _(p):
    p.player(.42, .60, .42); p.poly((.66, .04), (.96, .34), (.70, .60), (.40, .30), c=C); p.poly((.69, .10), (.92, .33), (.70, .54), (.47, .31), c=G); p.poly((.90, .46), (.98, .62), (.90, .72), (.82, .62))


@icon('Aviso de armadura')
def _(p):
    p.poly((.5, .04), (.90, .18), (.88, .52), (.74, .78), (.5, .96), (.26, .78), (.12, .52), (.10, .18)); p.rect(.45, .24, .55, .56, .04, C); p.disc(.5, .68, .06, C)


@icon('Brillo de encantamiento')
def _(p):
    p.star(.42, .56, .38, 4, .30); p.star(.80, .24, .20, 4, .30); p.star(.80, .82, .13, 4, .30, G)


@icon('Sin mano')
def _(p):
    p.rect(.22, .40, .70, .84, .14)
    for i, h in enumerate((.22, .12, .14, .22)): p.rect(.22 + i * .125, h, .32 + i * .125, .56, .05)
    p.line((.74, .62), (.84, .48), w=.10); p.line((.10, .10), (.92, .92), w=.14, c=C); p.line((.10, .10), (.92, .92), w=.07)


@icon('Interfaz transparente')
def _(p):
    p.rect(.04, .16, .72, .64, .06, G); p.rect(.26, .34, .96, .86, .06); p.rect(.34, .42, .88, .48, .02, C); p.rect(.34, .56, .70, .62, .02, C)


@icon('Visión clara')
def _(p):
    p.poly((.02, .5), (.20, .30), (.5, .18), (.80, .30), (.98, .5), (.80, .70), (.5, .82), (.20, .70)); p.disc(.5, .5, .22, C); p.disc(.5, .5, .14)


@icon('Zoom')
def _(p):
    p.disc(.42, .42, .36); p.disc(.42, .42, .26, C); p.line((.68, .68), (.92, .92), w=.13); p.line((.30, .42), (.54, .42), w=.06); p.line((.42, .30), (.42, .54), w=.06)


@icon('Sprint automático')
def _(p):
    p.disc(.62, .15, .10); p.line((.56, .30), (.44, .56), w=.13); p.line((.56, .32), (.74, .44), (.86, .38), w=.07); p.line((.52, .34), (.34, .38), (.26, .52), w=.07)
    p.line((.46, .56), (.62, .70), (.58, .92), w=.09); p.line((.44, .56), (.34, .74), (.14, .78), w=.09); p.line((.04, .26), (.24, .26), w=.05, c=G); p.line((.02, .42), (.16, .42), w=.05, c=G)


@icon('Optimizador')
def _(p):
    p.poly((.58, .02), (.18, .56), (.46, .56), (.38, .98), (.82, .40), (.54, .40))


def slug(name):
    base = unicodedata.normalize('NFD', name).encode('ascii', 'ignore').decode().lower()
    return ''.join(ch if ch.isalnum() else '-' for ch in base)


for name, draw in ICONS.items():
    pen = Pen()
    draw(pen)
    pen.im.resize((96, 96), Image.LANCZOS).save(os.path.join(OUT, slug(name) + '.png'), optimize=True)
Image.open(os.path.join(OUT, '..', '..', 'renderer', 'icon.png')).resize((128, 128), Image.LANCZOS).save(os.path.join(OUT, 'logo.png'), optimize=True)
print(len(ICONS))
