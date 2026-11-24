// LEX CLIENT — Overlay Engine: implementación Win32 + Direct2D. MinGW-first, MSVC-compatible.
#include "engine.h"
#include "bridge.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <limits>
#include <map>

#if defined(_MSC_VER)
#pragma comment(lib, "dwmapi.lib")
#endif

// ---- Superficie BGRA premultiplicada para UpdateLayeredWindow ----
// La GUI in-game se dibuja como una textura con alfa real: el juego se ve a
// través de los paneles. Nada de colorkey negro ni rectángulos opacos.
namespace lex {
namespace engine {

struct Color {
  uint8_t b = 0, g = 0, r = 0, a = 255;
};

inline Color rgba(COLORREF c, int alpha = 255) {
  const uint8_t a = (uint8_t)std::max(0, std::min(255, alpha));
  return { (uint8_t)GetBValue(c), (uint8_t)GetGValue(c), (uint8_t)GetRValue(c), a };
}
inline Color rgb(uint8_t r, uint8_t g, uint8_t b, int alpha = 255) {
  return { b, g, r, (uint8_t)std::max(0, std::min(255, alpha)) };
}
inline Color lighten(Color c, int d) {
  return { (uint8_t)std::min(255, c.r + d), (uint8_t)std::min(255, c.g + d),
           (uint8_t)std::min(255, c.b + d), c.a };
}
inline Color darken(Color c, int d) {
  return { (uint8_t)std::max(0, c.r - d), (uint8_t)std::max(0, c.g - d),
           (uint8_t)std::max(0, c.b - d), c.a };
}

// Tipografía pixel propia de 5x7 (sin assets copiados): es la misma idea de
// la UI de Minecraft — glyphs cuadrados, sin antialias, legibles sobre el juego.
static wchar_t foldGlyph(wchar_t ch) {
  if (ch >= L'a' && ch <= L'z') return (wchar_t)(ch - 32);
  switch (ch) {
    case L'á': case L'à': case L'ä': return L'A';
    case L'é': case L'è': case L'ë': return L'E';
    case L'í': case L'ì': case L'ï': return L'I';
    case L'ó': case L'ò': case L'ö': return L'O';
    case L'ú': case L'ù': case L'ü': return L'U';
    case L'ñ': return L'N';
    case L'ç': return L'C';
    default: return ch;
  }
}

static const uint8_t* glyphFor(wchar_t raw) {
  static const uint8_t blank[7] = {0,0,0,0,0,0,0};
  static const uint8_t gA[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
  static const uint8_t gB[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
  static const uint8_t gC[7] = {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F};
  static const uint8_t gD[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
  static const uint8_t gE[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
  static const uint8_t gF[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
  static const uint8_t gG[7] = {0x0F,0x10,0x10,0x13,0x11,0x11,0x0F};
  static const uint8_t gH[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
  static const uint8_t gI[7] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
  static const uint8_t gJ[7] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C};
  static const uint8_t gK[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
  static const uint8_t gL[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
  static const uint8_t gM[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
  static const uint8_t gN[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
  static const uint8_t gO[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
  static const uint8_t gP[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
  static const uint8_t gQ[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
  static const uint8_t gR[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
  static const uint8_t gS[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
  static const uint8_t gT[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
  static const uint8_t gU[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
  static const uint8_t gV[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
  static const uint8_t gW[7] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11};
  static const uint8_t gX[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
  static const uint8_t gY[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
  static const uint8_t gZ[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
  static const uint8_t g0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
  static const uint8_t g1[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
  static const uint8_t g2[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
  static const uint8_t g3[7] = {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E};
  static const uint8_t g4[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
  static const uint8_t g5[7] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
  static const uint8_t g6[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
  static const uint8_t g7[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
  static const uint8_t g8[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
  static const uint8_t g9[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};
  static const uint8_t gDot[7] = {0,0,0,0,0,0x04,0x04};
  static const uint8_t gComma[7] = {0,0,0,0,0x04,0x04,0x08};
  static const uint8_t gColon[7] = {0,0,0x04,0,0x04,0,0};
  static const uint8_t gDash[7] = {0,0,0,0x1F,0,0,0};
  static const uint8_t gPlus[7] = {0,0,0x04,0x1F,0x04,0,0};
  static const uint8_t gSlash[7] = {0x01,0x01,0x02,0x04,0x08,0x10,0x10};
  static const uint8_t gBack[7] = {0x10,0x10,0x08,0x04,0x02,0x01,0x01};
  static const uint8_t gStar[7] = {0,0,0x15,0x0E,0x15,0,0};
  static const uint8_t gHash[7] = {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0};
  static const uint8_t gParenL[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02};
  static const uint8_t gParenR[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08};
  static const uint8_t gBracketL[7] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E};
  static const uint8_t gBracketR[7] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E};
  static const uint8_t gPercent[7] = {0x19,0x19,0x02,0x04,0x08,0x13,0x13};
  static const uint8_t gEqual[7] = {0,0,0x1F,0,0x1F,0,0};
  static const uint8_t gLt[7] = {0x02,0x04,0x08,0x10,0x08,0x04,0x02};
  static const uint8_t gGt[7] = {0x08,0x04,0x02,0x01,0x02,0x04,0x08};
  static const uint8_t gBar[7] = {0x04,0x04,0x04,0x04,0x04,0x04,0x04};
  static const uint8_t gCaret[7] = {0x04,0x0A,0x11,0,0,0,0};
  static const uint8_t gUnder[7] = {0,0,0,0,0,0,0x1F};
  static const uint8_t gBang[7] = {0x04,0x04,0x04,0x04,0x04,0,0x04};
  static const uint8_t gQuestion[7] = {0x0E,0x11,0x01,0x02,0x04,0,0x04};
  static const uint8_t gQuote[7] = {0x0A,0x0A,0,0,0,0,0};
  static const uint8_t gApost[7] = {0x04,0x04,0,0,0,0,0};
  static const uint8_t gMiddle[7] = {0,0,0,0x04,0,0,0};
  static const uint8_t gDegree[7] = {0x0E,0x11,0x11,0x0E,0,0,0};

  switch (foldGlyph(raw)) {
    case L'A': return gA; case L'B': return gB; case L'C': return gC;
    case L'D': return gD; case L'E': return gE; case L'F': return gF;
    case L'G': return gG; case L'H': return gH; case L'I': return gI;
    case L'J': return gJ; case L'K': return gK; case L'L': return gL;
    case L'M': return gM; case L'N': return gN; case L'O': return gO;
    case L'P': return gP; case L'Q': return gQ; case L'R': return gR;
    case L'S': return gS; case L'T': return gT; case L'U': return gU;
    case L'V': return gV; case L'W': return gW; case L'X': return gX;
    case L'Y': return gY; case L'Z': return gZ;
    case L'0': return g0; case L'1': return g1; case L'2': return g2;
    case L'3': return g3; case L'4': return g4; case L'5': return g5;
    case L'6': return g6; case L'7': return g7; case L'8': return g8;
    case L'9': return g9;
    case L'.': return gDot; case L',': return gComma; case L':': return gColon;
    case L'-': return gDash; case L'+': return gPlus; case L'/': return gSlash;
    case L'\\': return gBack; case L'*': return gStar; case L'#': return gHash;
    case L'(': return gParenL; case L')': return gParenR;
    case L'[': return gBracketL; case L']': return gBracketR;
    case L'%': return gPercent; case L'=': return gEqual;
    case L'<': return gLt; case L'>': return gGt; case L'|': return gBar;
    case L'^': return gCaret; case L'_': return gUnder;
    case L'!': return gBang; case L'?': return gQuestion;
    case L'"': return gQuote; case L'\'': return gApost;
    case L'·': return gMiddle; case L'°': return gDegree;
    default: return blank;
  }
}

// ---- Tipografía real: glifos antialiasados de un TTF vía GDI ----
struct Glyph {
  int w = 0, h = 0, left = 0, top = 0, advance = 0;
  std::vector<uint8_t> mask;
};

static HDC glyphDc() {
  static HDC dc = CreateCompatibleDC(nullptr);
  return dc;
}

static const Glyph& cachedGlyph(HFONT font, wchar_t ch) {
  static std::map<std::pair<HFONT, wchar_t>, Glyph> cache;
  const auto key = std::make_pair(font, ch);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;

  Glyph g;
  HDC dc = glyphDc();
  HFONT old = (HFONT)SelectObject(dc, font);
  // MinGW FIXED = {WORD fract; short value}; identidad => fract=0, value=1.
  MAT2 mat{ {0, 1}, {0, 0}, {0, 0}, {0, 1} };
  GLYPHMETRICS gm{};
  const DWORD bytes = GetGlyphOutlineW(dc, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &mat);
  if (bytes > 0 && bytes < (1u << 20)) {
    std::vector<uint8_t> raw(bytes);
    if (GetGlyphOutlineW(dc, ch, GGO_GRAY8_BITMAP, &gm, bytes, raw.data(), &mat) == bytes ||
        GetLastError() == 0) {
      g.w = gm.gmBlackBoxX;
      g.h = gm.gmBlackBoxY;
      g.left = gm.gmptGlyphOrigin.x;
      g.top = gm.gmptGlyphOrigin.y;
      g.advance = gm.gmCellIncX;
      const size_t stride = ((size_t)g.w + 3) & ~size_t(3);
      g.mask.assign((size_t)g.w * g.h, 0);
      for (int y = 0; y < g.h; ++y)
        for (int x = 0; x < g.w; ++x)
          g.mask[(size_t)y * g.w + x] = (uint8_t)std::min(255, raw[(size_t)y * stride + x] * 4);
    }
  }
  if (g.advance == 0) {
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    g.advance = tm.tmAveCharWidth;
  }
  SelectObject(dc, old);
  auto inserted = cache.emplace(key, std::move(g));
  return inserted.first->second;
}

struct Painter {
  uint8_t* bits = nullptr;
  HFONT font_ = nullptr;   // si está puesto, text() usa la fuente TTF real
  int w = 0, h = 0;

  void bind(void* buffer, int width, int height) {
    bits = (uint8_t*)buffer; w = width; h = height;
  }
  void useFont(HFONT f) { font_ = f; }
  void clear() {
    if (bits && w > 0 && h > 0) std::memset(bits, 0, (size_t)w * h * 4);
  }

  void pixel(int x, int y, Color c) {
    if (!bits || x < 0 || y < 0 || x >= w || y >= h || c.a == 0) return;
    uint8_t* p = bits + ((size_t)y * w + x) * 4;
    if (c.a >= 255) {
      p[0] = c.b; p[1] = c.g; p[2] = c.r; p[3] = 255;
    } else {
      const uint32_t inv = 255u - c.a;
      p[0] = (uint8_t)((c.b * c.a + p[0] * inv + 127) / 255);
      p[1] = (uint8_t)((c.g * c.a + p[1] * inv + 127) / 255);
      p[2] = (uint8_t)((c.r * c.a + p[2] * inv + 127) / 255);
      p[3] = (uint8_t)((255u * c.a + p[3] * inv + 127) / 255);
    }
  }

  // Igual que pixel(), pero para texturas que ya guardan RGB premultiplicado
  // (LEXTEX1): no se vuelve a multiplicar por el alfa.
  void pixelPremultiplied(int x, int y, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    if (!bits || x < 0 || y < 0 || x >= w || y >= h || a == 0) return;
    uint8_t* p = bits + ((size_t)y * w + x) * 4;
    if (a >= 255) {
      p[0] = b; p[1] = g; p[2] = r; p[3] = 255;
    } else {
      const uint32_t inv = 255u - a;
      p[0] = (uint8_t)((b + p[0] * inv + 127) / 255);
      p[1] = (uint8_t)((g + p[1] * inv + 127) / 255);
      p[2] = (uint8_t)((r + p[2] * inv + 127) / 255);
      p[3] = (uint8_t)((255u * a + p[3] * inv + 127) / 255);
    }
  }

  void fillRect(int x, int y, int rw, int rh, Color c) {
    if (rw <= 0 || rh <= 0 || c.a == 0) return;
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    for (int yy = y0; yy < y1; ++yy) {
      for (int xx = x0; xx < x1; ++xx) pixel(xx, yy, c);
    }
  }

  void frameRect(int x, int y, int rw, int rh, Color c, int t = 1) {
    if (rw <= 0 || rh <= 0 || t <= 0) return;
    fillRect(x, y, rw, std::min(t, rh), c);
    fillRect(x, y + rh - std::min(t, rh), rw, std::min(t, rh), c);
    fillRect(x, y, std::min(t, rw), rh, c);
    fillRect(x + rw - std::min(t, rw), y, std::min(t, rw), rh, c);
  }

  void roundFill(int x, int y, int rw, int rh, int radius, Color c) {
    if (rw <= 0 || rh <= 0 || c.a == 0) return;
    int rad = std::max(0, std::min({radius, rw / 2, rh / 2}));
    for (int yy = 0; yy < rh; ++yy) {
      int inset = 0;
      if (rad > 0 && (yy < rad || yy >= rh - rad)) {
        const int dy = yy < rad ? rad - 1 - yy : yy - (rh - rad);
        const int t = rad - 1 - std::max(0, dy);
        inset = rad - (int)std::sqrt(std::max(0, rad * rad - t * t));
      }
      fillRect(x + inset, y + yy, rw - 2 * inset, 1, c);
    }
  }

  void roundFrame(int x, int y, int rw, int rh, int radius, Color c, int t = 1) {
    if (rw <= 0 || rh <= 0 || t <= 0 || c.a == 0) return;
    int rad = std::max(0, std::min({radius, rw / 2, rh / 2}));
    const int th = std::min(t, rh / 2);
    for (int yy = 0; yy < rh; ++yy) {
      int inset = 0;
      if (rad > 0 && (yy < rad || yy >= rh - rad)) {
        const int dy = yy < rad ? rad - 1 - yy : yy - (rh - rad);
        const int q = rad - 1 - std::max(0, dy);
        inset = rad - (int)std::sqrt(std::max(0, rad * rad - q * q));
      }
      if (yy < th || yy >= rh - th) {
        fillRect(x + inset, y + yy, rw - 2 * inset, 1, c);
      } else {
        fillRect(x + inset, y + yy, th, 1, c);
        fillRect(x + rw - inset - th, y + yy, th, 1, c);
      }
    }
  }

  // Panel/botón profesional: esquina redondeada, sombra suave y bisel sutil.
  void bevel(int x, int y, int rw, int rh, Color fill, bool raised = true) {
    if (rw <= 0 || rh <= 0 || fill.a == 0) return;
    const int rad = std::max(3, std::min(7, std::min(rw, rh) / 4));
    roundFill(x + 2, y + 3, rw, rh, rad, rgba(RGB(0x00, 0x00, 0x00), 72));
    roundFill(x, y, rw, rh, rad, fill);
    roundFrame(x, y, rw, rh, rad, rgba(RGB(0x00, 0x00, 0x00), 85), 1);
    if (raised) {
      roundFill(x + 4, y + 2, rw - 8, 2, 1, lighten(fill, 38));
      roundFill(x + 2, y + 5, 2, rh - 10, 1, lighten(fill, 24));
      roundFill(x + 4, y + rh - 4, rw - 8, 2, 1, darken(fill, 28));
      roundFill(x + rw - 4, y + 5, 2, rh - 10, 1, darken(fill, 20));
    } else {
      roundFill(x + 4, y + 3, rw - 8, 2, 1, darken(fill, 32));
      roundFill(x + 4, y + rh - 5, rw - 8, 2, 1, lighten(fill, 20));
    }
  }

  void line(int x1, int y1, int x2, int y2, Color c) {
    int dx = std::abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -std::abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      pixel(x1, y1, c);
      if (x1 == x2 && y1 == y2) break;
      const int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x1 += sx; }
      if (e2 <= dx) { err += dx; y1 += sy; }
    }
  }

  void ring(int cx, int cy, int radius, Color c) {
    int x = radius, y = 0, err = 0;
    while (x >= y) {
      pixel(cx + x, cy + y, c); pixel(cx + y, cy + x, c);
      pixel(cx - y, cy + x, c); pixel(cx - x, cy + y, c);
      pixel(cx - x, cy - y, c); pixel(cx - y, cy - x, c);
      pixel(cx + y, cy - x, c); pixel(cx + x, cy - y, c);
      y++; err += 1 + 2 * y;
      if (err > x) { x--; err -= 2 * x; }
    }
  }

  void dot(int cx, int cy, int radius, Color c) {
    for (int y = -radius; y <= radius; ++y)
      for (int x = -radius; x <= radius; ++x)
        if (x*x + y*y <= radius*radius) pixel(cx + x, cy + y, c);
  }

  int textWidth(const std::wstring& s, int scale) const {
    if (s.empty() || scale <= 0) return 0;
    (void)scale;
    if (font_) {
      int total = 0;
      for (wchar_t ch : s) total += cachedGlyph(font_, ch).advance;
      return total;
    }
    return (int)s.size() * 6 * scale - scale;
  }

  void textGdi(int x, int y, const std::wstring& s, Color c) {
    if (!font_ || s.empty() || c.a == 0) return;
    HDC dc = glyphDc();
    HFONT old = (HFONT)SelectObject(dc, font_);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int baseline = y + tm.tmAscent;
    int pen = x;
    for (wchar_t ch : s) {
      const Glyph& g = cachedGlyph(font_, ch);
      const int gx = pen + g.left, gy = baseline - g.top;
      for (int yy = 0; yy < g.h; ++yy) {
        for (int xx = 0; xx < g.w; ++xx) {
          const int a = g.mask[(size_t)yy * g.w + xx];
          if (a) pixel(gx + xx, gy + yy, rgb(c.r, c.g, c.b, a * c.a / 255));
        }
      }
      pen += g.advance;
    }
    SelectObject(dc, old);
  }

  void text(int x, int y, const std::wstring& s, int scale, Color c) {
    if (s.empty() || scale <= 0 || c.a == 0) return;
    if (font_) { textGdi(x, y, s, c); return; }
    int cx = x;
    for (wchar_t ch : s) {
      const uint8_t* g = glyphFor(ch);
      for (int row = 0; row < 7; ++row) {
        const uint8_t bitsRow = g[row];
        for (int col = 0; col < 5; ++col) {
          if (bitsRow & (1u << (4 - col)))
            fillRect(cx + col * scale, y + row * scale, scale, scale, c);
        }
      }
      cx += 6 * scale;
    }
  }

  void textCentered(int x, int y, int rw, int rh, const std::wstring& s, int scale, Color c) {
    const int tw = textWidth(s, scale);
    int th = font_ ? 16 : 7 * scale;
    if (font_) {
      HDC dc = glyphDc();
      HFONT old = (HFONT)SelectObject(dc, font_);
      TEXTMETRICW tm{};
      GetTextMetricsW(dc, &tm);
      th = tm.tmHeight;
      SelectObject(dc, old);
    }
    text(x + (rw - tw) / 2, y + (rh - th) / 2, s, scale, c);
  }

  std::wstring ellipsis(const std::wstring& s, int maxW, int scale) const {
    if (maxW <= scale * 6) return L"";
    if (textWidth(s, scale) <= maxW) return s;
    std::wstring t = s;
    while (!t.empty() && textWidth(t + L"...", scale) > maxW) t.pop_back();
    return t + L"...";
  }

  void button(int x, int y, int rw, int rh, const std::wstring& label,
              Color bg, Color fg, bool active = false, int scale = 1) {
    // Con fondo desactivado (alpha 0) el botón es sólo texto: ni sombra ni bisel.
    if (bg.a > 0) {
      bevel(x, y, rw, rh, bg, !active);
      if (active) frameRect(x + 3, y + 3, rw - 6, rh - 6, rgba(RGB(0xFF,0xFF,0xFF), 70), 1);
    }
    textCentered(x, y, rw, rh, label, scale, fg);
  }
};

// Logotipo oficial importado por el launcher: BGRA crudo con header w,h.
struct ImportedLogo {
  int w = 0, h = 0;
  std::vector<uint8_t> bgra;
  bool valid = false;
};
static const ImportedLogo& importedLogo() {
  static ImportedLogo logo;
  static bool tried = false;
  if (tried) return logo;
  tried = true;
  std::ifstream f(bridge::getLogoPath(), std::ios::binary);
  if (!f) return logo;
  f.read((char*)&logo.w, 4);
  f.read((char*)&logo.h, 4);
  if (logo.w <= 0 || logo.h <= 0 || logo.w > 1024 || logo.h > 1024) return logo;
  logo.bgra.assign((size_t)logo.w * logo.h * 4, 0);
  f.read((char*)logo.bgra.data(), logo.bgra.size());
  logo.valid = (size_t)f.gcount() == logo.bgra.size();
  return logo;
}
static void drawImportedLogo(Painter& p, int x, int y, int box) {
  const ImportedLogo& logo = importedLogo();
  if (!logo.valid) return;
  const double scale = std::min((double)box / logo.w, (double)box / logo.h);
  const int dw = std::max(1, (int)(logo.w * scale));
  const int dh = std::max(1, (int)(logo.h * scale));
  const int ox = x + (box - dw) / 2, oy = y + (box - dh) / 2;
  for (int yy = 0; yy < dh; ++yy) {
    for (int xx = 0; xx < dw; ++xx) {
      const int sx = (int)(xx / scale), sy = (int)(yy / scale);
      const size_t i = ((size_t)sy * logo.w + sx) * 4;
      p.pixel(ox + xx, oy + yy,
              rgb(logo.bgra[i + 2], logo.bgra[i + 1], logo.bgra[i], logo.bgra[i + 3]));
    }
  }
}

}  // namespace engine
}  // namespace lex

using lex::engine::Color;
using lex::engine::Painter;
using lex::engine::rgba;
using lex::engine::rgb;

// Glifos propios de LEX CLIENT: rejilla de 32x32 escalada al tamaño disponible,
// dibujada sobre la superficie alfa (sin assets externos ni copias de Flarial).
static void drawModuleGlyph(Painter& p, const std::string& id, int x, int y, int size, Color c) {
  if (size < 8) return;
  const int s = size;
  // El glifo es transparente: el HUD no lleva fondos sólidos.

  auto ux = [&](int v) { return x + (v * s) / 32; };
  auto uy = [&](int v) { return y + (v * s) / 32; };
  auto sc = [&](int v) { return std::max(1, (v * s) / 32); };
  auto L = [&](int a, int b, int cc, int d) { p.line(ux(a), uy(b), ux(cc), uy(d), c); };
  auto R = [&](int a, int b, int ww, int hh) { p.frameRect(ux(a), uy(b), ux(a + ww) - ux(a), uy(b + hh) - uy(b), c, 1); };
  auto F = [&](int a, int b, int ww, int hh) { p.fillRect(ux(a), uy(b), ux(a + ww) - ux(a), uy(b + hh) - uy(b), c); };
  auto O = [&](int a, int b, int r) { p.ring(ux(a), uy(b), sc(r), c); };
  auto D = [&](int a, int b, int r) { p.dot(ux(a), uy(b), sc(r), c); };

  if (id == "hud-fps") {
    R(5, 6, 22, 19);
    L(8, 21, 12, 15); L(12, 15, 16, 18); L(16, 18, 20, 11); L(20, 11, 24, 14);
    D(24, 14, 2); L(8, 24, 16, 24);
  } else if (id == "hud-cps") {
    R(10, 4, 12, 23); L(16, 4, 16, 13); D(14, 8, 1); D(19, 8, 1);
    L(5, 12, 8, 14); L(27, 12, 24, 14); L(6, 21, 9, 20); L(26, 21, 23, 20); L(4, 27, 28, 27);
  } else if (id == "hud-keystrokes") {
    R(12, 3, 8, 8); R(4, 13, 8, 8); R(12, 13, 8, 8); R(20, 13, 8, 8);
    R(7, 22, 18, 5); L(11, 24, 21, 24);
  } else if (id == "hud-clock") {
    O(16, 16, 10); D(16, 16, 2); L(16, 9, 16, 16); L(16, 16, 21, 19);
    L(16, 3, 16, 6); L(16, 26, 16, 29); L(3, 16, 6, 16); L(26, 16, 29, 16);
  } else if (id == "hud-ping") {
    L(6, 25, 6, 19); L(12, 25, 12, 15); L(18, 25, 18, 11); L(24, 25, 24, 6);
    F(23, 6, 3, 19); L(4, 27, 28, 27);
  } else if (id == "hud-memory") {
    R(4, 9, 24, 13); F(7, 12, 5, 6); F(14, 12, 5, 6); F(20, 12, 5, 6);
    L(8, 22, 8, 25); L(13, 22, 13, 25); L(19, 22, 19, 25); L(24, 22, 24, 25);
  } else if (id == "hud-direction") {
    O(16, 16, 11); L(16, 6, 19, 16); L(19, 16, 16, 14); L(16, 14, 13, 16);
    L(13, 16, 16, 26); L(16, 26, 16, 14); D(16, 16, 2);
  } else if (id == "visual-crosshair") {
    L(16, 4, 16, 11); L(16, 21, 16, 28); L(4, 16, 11, 16); L(21, 16, 28, 16);
    O(16, 16, 4); D(16, 16, 2);
    L(5, 5, 8, 5); L(5, 5, 5, 8); L(27, 5, 24, 5); L(27, 5, 27, 8);
    L(5, 27, 8, 27); L(5, 27, 5, 24); L(27, 27, 24, 27); L(27, 27, 27, 24);
  } else if (id == "visual-motionblur") {
    L(4, 10, 15, 10); L(3, 16, 12, 16); L(5, 22, 14, 22);
    L(14, 8, 24, 16); L(24, 16, 14, 24); L(14, 8, 14, 24);
    L(20, 8, 28, 16); L(28, 16, 20, 24);
  } else if (id == "visual-fog") {
    O(22, 9, 3);
    L(4, 17, 8, 15); L(8, 15, 12, 19); L(12, 19, 16, 17); L(16, 17, 20, 15); L(20, 15, 28, 17);
    L(3, 22, 7, 20); L(7, 20, 11, 24); L(11, 24, 15, 22); L(15, 22, 19, 20); L(19, 20, 29, 22);
    L(5, 27, 9, 25); L(9, 25, 13, 28); L(13, 28, 17, 27); L(17, 27, 21, 25); L(21, 25, 27, 27);
  } else if (id == "visual-colors") {
    O(16, 15, 10); D(10, 12, 2); D(16, 10, 2); D(22, 12, 2); L(16, 16, 16, 28); L(10, 24, 22, 24);
  } else if (id == "visual-zoom") {
    O(14, 14, 8); L(20, 20, 27, 27); D(14, 14, 3); L(10, 11, 12, 9);
  } else if (id == "game-drift") {
    L(6, 6, 19, 19); L(26, 6, 13, 19); L(19, 19, 16, 27); L(13, 19, 10, 27);
    L(9, 27, 23, 27); D(6, 6, 2); D(26, 6, 2);
  } else if (id == "game-tps") {
    O(16, 16, 10); O(16, 16, 5); D(16, 16, 2);
    L(16, 3, 16, 6); L(16, 26, 16, 29); L(3, 16, 6, 16); L(26, 16, 29, 16);
  } else if (id == "game-haste") {
    L(7, 8, 20, 12); L(20, 12, 27, 9); L(27, 9, 20, 16); L(20, 12, 20, 16);
    L(17, 16, 8, 27); L(6, 28, 9, 25);
  } else if (id == "util-keybinds") {
    R(3, 8, 26, 16); L(7, 12, 10, 12); L(13, 12, 16, 12); L(19, 12, 25, 12);
    L(7, 16, 11, 16); L(14, 16, 18, 16); L(21, 16, 25, 16); R(18, 3, 10, 7);
  } else if (id == "util-session") {
    O(16, 17, 9); L(13, 4, 19, 4); L(16, 4, 16, 8); L(22, 9, 24, 7);
    L(16, 12, 16, 17); L(16, 17, 20, 20); D(16, 17, 2);
  } else if (id == "util-screens") {
    R(3, 6, 26, 16); O(16, 14, 4); D(16, 14, 2); L(11, 26, 21, 26); L(16, 22, 16, 26);
  } else {
    std::string low = id;
    for (char& ch : low) ch = (char)std::tolower((unsigned char)ch);
    if (low.find("wing") != std::string::npos) {
      L(16, 15, 4, 8); L(4, 8, 3, 13); L(3, 13, 14, 24); L(14, 24, 16, 15);
      L(16, 15, 28, 8); L(28, 8, 29, 13); L(29, 13, 18, 24); L(18, 24, 16, 15); D(16, 14, 2);
    } else if (low.find("cape") != std::string::npos) {
      L(8, 5, 16, 9); L(16, 9, 24, 5); L(24, 5, 27, 26); L(27, 26, 16, 29);
      L(16, 29, 5, 26); L(5, 26, 8, 5); L(12, 12, 12, 23); L(20, 12, 20, 23); D(16, 15, 2);
    } else {
      L(16, 4, 27, 10); L(27, 10, 16, 16); L(16, 16, 5, 10); L(5, 10, 16, 4);
      L(5, 10, 16, 16); L(16, 16, 27, 10); L(16, 16, 16, 28); L(5, 22, 16, 28); L(27, 22, 16, 28);
    }
  }
}

namespace lex {
namespace engine {

static OverlayEngine* g_instance = nullptr;
OverlayEngine* OverlayEngine::instance() { return g_instance; }

static std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n > 0 ? n : 1, L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

// Directorio del ejecutable (en la instalación: resources/overlay/) y carpeta de datos.
static std::string dirOf(const std::string& p) {
  const size_t s = p.find_last_of("/\\");
  return s == std::string::npos ? "" : p.substr(0, s);
}

static std::string exeDir() {
  char path[MAX_PATH] = {0};
  const DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  return dirOf(std::string(path, n > 0 ? n : 0));
}

// Sólo nombre de archivo: un JSON importado no puede escapar de assets/cosmetics.
static std::string safeAssetName(const std::string& value) {
  std::string name = value;
  const size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) name = name.substr(slash + 1);
  std::string out;
  for (unsigned char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') out += (char)c;
  }
  return out;
}

static std::string resolveCosmeticAsset(const std::string& file) {
  const std::string name = safeAssetName(file);
  if (name.empty()) return "";
  const std::string userDir = dirOf(bridge::getCosmeticsPath()) + "/textures";
  const std::string base = exeDir();
  const std::vector<std::string> roots = {
    userDir,
    base + "/assets/cosmetics",
    dirOf(base) + "/assets/cosmetics",
    "assets/cosmetics",
  };
  for (const std::string& root : roots) {
    const std::string path = root + "/" + name;
    std::ifstream f(path, std::ios::binary);
    if (f.good()) return path;
  }
  return "";
}

static std::vector<std::string> activeCosmeticIds(const std::vector<bridge::ModuleState>& modules) {
  std::vector<std::string> out;
  for (const auto& m : modules)
    if (m.enabled && m.id.rfind("cosmetic-", 0) == 0) out.push_back(m.id.substr(9));
  return out;
}

static std::string activeCosmeticId(const std::vector<bridge::ModuleState>& modules) {
  const auto ids = activeCosmeticIds(modules);
  return ids.empty() ? "" : ids[0];
}

// Firma estable de lo que realmente afecta al renderer de cosméticos.
// Así no se liberan HBITMAPs en cada reload del bridge (anti-parpadeo).
static std::string cosmeticSignature(const std::vector<bridge::Cosmetic>& items,
                                     const std::vector<std::string>& active) {
  std::string s = "active=";
  for (const auto& id : active) s += id + ",";
  for (const auto& c : items) {
    s += "|" + c.id + ":" + c.atlas + ":" + std::to_string(c.frameMs);
  }
  return s;
}

// Traza de depuración: qué teclas recibe el motor (solo con el menú abierto).
static std::string tracePath() {
  std::string p = bridge::getModulesStatePath();
  const size_t s = p.find_last_of("/\\");
  if (s == std::string::npos) return "lex-keytrace.txt";
  return p.substr(0, s + 1) + "lex-keytrace.txt";
}

static void traceKey(const char* what, int vk, bool menu, bool adding,
                     int page, int sel, int count, const char* extra = "") {
  std::ofstream f(tracePath(), std::ios::app);
  if (!f) return;
  f << what << " vk=" << vk << " menu=" << menu << " adding=" << adding
    << " page=" << page << " sel=" << sel << " count=" << count;
  if (extra && *extra) f << " " << extra;
  f << "\n";
}

// ---- ClickGUI in-game: tres secciones (MODS / COSMETICS / HUD) ----
// Estética sobria y translúcida, inspirada en la referencia, sin catálogo de color.
namespace {

// Paleta LEX compartida con el launcher (una sola identidad visual).
const COLORREF kLexBase       = RGB(0x08, 0x0B, 0x12);
const COLORREF kLexDeep       = RGB(0x0B, 0x11, 0x1C);
const COLORREF kLexSurface    = RGB(0x10, 0x18, 0x27);
const COLORREF kLexSurface2   = RGB(0x17, 0x23, 0x38);
const COLORREF kLexSurface3   = RGB(0x1D, 0x35, 0x57);
const COLORREF kLexBorder     = RGB(0x26, 0x34, 0x49);
const COLORREF kLexBorderStrong = RGB(0x31, 0x57, 0x83);
const COLORREF kLexAccent     = RGB(0x31, 0x57, 0x83);  // azul profundo, no cian
const COLORREF kLexAccent2    = RGB(0x1D, 0x35, 0x57);
const COLORREF kLexWarm       = RGB(0x9A, 0xA8, 0xBA);
const COLORREF kLexText       = RGB(0xF1, 0xF4, 0xF9);
const COLORREF kLexMid        = RGB(0x9A, 0xA8, 0xBA);
const COLORREF kLexDim        = RGB(0x63, 0x72, 0x8A);

// Geometría: la ventana se redimensiona al alternar entre menú y HUD.
constexpr int kMenuW = 1040;
constexpr int kMenuH = 640;
constexpr int kHudW  = 260;
constexpr int kHudH  = 176;

// ClickGUI: navegación por pestañas + tarjetas +galería visual de cosméticos.
// Todo se maneja con clic; F8 sólo abre/cierra el menú.
constexpr int kHeaderH = 74;
constexpr int kFooterH = 34;
constexpr int kSideW    = 220;
constexpr int kContentX = kSideW + 28;
constexpr int kContentR = kMenuW - 44;   // hueco para la barra de desplazamiento
constexpr int kContentY = kHeaderH + 66;
constexpr int kContentB = kMenuH - kFooterH - 16;
constexpr int kGridCols = 3;
constexpr int kGridRows = 2;
constexpr int kGridGap  = 16;
constexpr int kCosCols = 3;
constexpr int kCosRows = 1;
constexpr int kCosPerPage = kCosCols * kCosRows;
constexpr int kHudPanelW = 164;
constexpr int kHudPanelH = 46;
constexpr int kKeysPanelW = 196;   // keystrokes tipo teclado (Flarial)
constexpr int kKeysPanelH = 122;
constexpr int kEditHeaderH = 42;

struct UiRect { int x = 0, y = 0, w = 0, h = 0; };
struct GridGeom { int x0, y0, cardW, cardH; };
struct KeyDef { UiRect rect; std::string label; char ch = 0; int action = 0; };

enum KeyAction { KEY_NONE = 0, KEY_CHAR = 1, KEY_BACK = 2, KEY_CLEAR = 3, KEY_DONE = 4 };

bool hit(const UiRect& r, int x, int y) {
  return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

UiRect keyboardRect(int winW, int winH) {
  const int w = 620, h = 360;
  return {(winW - w) / 2, (winH - h) / 2, w, h};
}

UiRect detailRect(int winW, int winH) {
  const int w = 780, h = 520;
  return {(winW - w) / 2, (winH - h) / 2, w, h};
}

UiRect editDoneRect(int winW) { return {winW - 196, 8, 180, 26}; }
UiRect searchRect() { return {210, 18, 300, 34}; }
UiRect closeRect(int winW) { return {winW - 46, 18, 28, 28}; }
UiRect detailCloseRect(const UiRect& d) { return {d.x + d.w - 42, d.y + 12, 26, 26}; }
// Panel de ajustes por módulo: mismos rects para pintar y para hacer clic.
UiRect detailSizeMinus(const UiRect& d) { return {d.x + 28, d.y + 116, 34, 28}; }
UiRect detailSizePlus(const UiRect& d) { return {d.x + 118, d.y + 116, 34, 28}; }
UiRect detailOpacityMinus(const UiRect& d) { return {d.x + 28, d.y + 166, 34, 28}; }
UiRect detailOpacityPlus(const UiRect& d) { return {d.x + 118, d.y + 166, 34, 28}; }
UiRect detailBackground(const UiRect& d) { return {d.x + 28, d.y + 216, 260, 32}; }
UiRect detailColor(const UiRect& d, int i) { return {d.x + 28 + i * 32, d.y + 282, 24, 24}; }
UiRect detailMove(const UiRect& d) { return {d.x + 28, d.y + 346, 260, 32}; }
UiRect detailToggle(const UiRect& d) { return {d.x + 28, d.y + 400, 260, 34}; }
UiRect detailReset(const UiRect& d) { return {d.x + 28, d.y + 452, 260, 30}; }
UiRect detailPreset(const UiRect& d, int i) {
  return {d.x + 352 + (i % 6) * 64, d.y + 250 + (i / 6) * 54, 56, 46};
}
// Sólo tres secciones, como la referencia: MODS / COSMETICS / HUD.
UiRect sectionTabRect(int i) { return {kContentX + i * 152, kHeaderH + 8, 142, 36}; }
UiRect menuOpacityRect(int winW) { return {winW - 158, kHeaderH + 8, 130, 36}; }
UiRect moveHudRect(int winW) { return {winW - 158, kHeaderH + 48, 130, 26}; }
// Panel lateral de perfiles (estilo Lunar) y botones de cada tarjeta.
UiRect sidebarRect() { return {14, kHeaderH + 8, kSideW - 28, kMenuH - kFooterH - kHeaderH - 24}; }
UiRect profileDropRect() { return {22, kHeaderH + 40, 192, 32}; }
UiRect profileItemRect(int i) { return {22, kHeaderH + 86 + i * 42, 192, 36}; }
UiRect saveProfileRect() { return {22, kMenuH - kFooterH - 108, 192, 28}; }
UiRect editLayoutRect() { return {22, kMenuH - kFooterH - 72, 192, 32}; }
UiRect cardOptionsRect(int x, int y, int cardW, int cardH) {
  return {x + 8, y + cardH - 84, cardW - 16, 30};
}
UiRect cardStateRect(int x, int y, int cardW, int cardH) {
  return {x + 16, y + cardH - 46, cardW - 32, 36};
}
UiRect scrollUpRect(int winW, int y) { return {winW - 34, y, 20, 20}; }
UiRect scrollDownRect(int winW, int y, int h) { return {winW - 34, y + h - 20, 20, 20}; }

// Teclado en pantalla: permite buscar con el ratón, sin escribir en el juego.
std::vector<KeyDef> keyboardKeys(const UiRect& m) {
  std::vector<KeyDef> out;
  constexpr int kw = 46, gap = 6;
  auto row = [&](const char* labels, int y, int action = KEY_CHAR) {
    std::string s(labels);
    int n = 0;
    for (char c : s) if (c != ' ') ++n;
    const int total = n * kw + (n - 1) * gap;
    int x = m.x + (m.w - total) / 2;
    for (char c : s) {
      if (c == ' ') { x += kw + gap; continue; }
      out.push_back({{x, y, kw, 32}, std::string(1, c), c, action});
      x += kw + gap;
    }
  };
  row("Q W E R T Y U I O P", m.y + 104);
  row("A S D F G H J K L", m.y + 156);
  row("Z X C V B N M", m.y + 208);
  row("1 2 3 4 5 6 7 8 9 0", m.y + 260);

  const int actionY = m.y + 312;
  out.push_back({{m.x + 28, actionY, 180, 32}, "ESPACIO", ' ', KEY_CHAR});
  out.push_back({{m.x + 216, actionY, 48, 32}, "-", '-', KEY_CHAR});
  out.push_back({{m.x + 272, actionY, 48, 32}, "_", '_', KEY_CHAR});
  out.push_back({{m.x + 328, actionY, 92, 32}, "BORRAR", 0, KEY_BACK});
  out.push_back({{m.x + 428, actionY, 72, 32}, "LIMPIAR", 0, KEY_CLEAR});
  out.push_back({{m.x + 508, actionY, 84, 32}, "LISTO", 0, KEY_DONE});
  return out;
}

GridGeom gridGeom() {
  GridGeom g;
  g.x0 = kContentX;
  g.y0 = kContentY;
  g.cardW = (kContentR - kContentX - (kGridCols - 1) * kGridGap) / kGridCols;
  g.cardH = (kContentB - kContentY - (kGridRows - 1) * kGridGap) / kGridRows;
  return g;
}

int panelWFor(const std::string& id) { return id == "hud-keystrokes" ? kKeysPanelW : kHudPanelW; }
int panelHFor(const std::string& id) { return id == "hud-keystrokes" ? kKeysPanelH : kHudPanelH; }

const bridge::ModuleMeta* findMeta(const std::vector<bridge::ModuleMeta>& cat, const std::string& id) {
  for (const auto& m : cat) if (m.id == id) return &m;
  return nullptr;
}

// Un módulo tiene representación real en el HUD sólo si sabemos qué dibujar.
bool isHudModule(const std::string& id) {
  return id.rfind("hud-", 0) == 0 || id.rfind("custom-", 0) == 0;
}

std::wstring lowerWide(std::wstring s) {
  for (wchar_t& c : s) c = (wchar_t)std::towlower(c);
  return s;
}

bool matchesSearch(const std::wstring& needle, const bridge::ModuleMeta* meta, const std::string& id) {
  if (needle.empty()) return true;
  if (lowerWide(utf8ToWide(id)).find(needle) != std::wstring::npos) return true;
  if (meta) {
    if (lowerWide(utf8ToWide(meta->name)).find(needle) != std::wstring::npos) return true;
    if (lowerWide(utf8ToWide(meta->description)).find(needle) != std::wstring::npos) return true;
  }
  return false;
}

bridge::HudPos defaultHudPos(const std::string& id, int index) {
  bridge::HudPos p;
  p.id = id;
  p.x = 32;
  p.y = 72 + (index % 8) * 52;
  if (index >= 8) { p.x = 212; p.y = 72 + (index % 8) * 52; }
  return p;
}

// Ventana de Minecraft (el overlay la vuelve a buscar si se recrea).
BOOL CALLBACK enumMinecraftWindow(HWND hwnd, LPARAM lParam) {
  wchar_t title[512] = {0};
  wchar_t cls[256] = {0};
  const int len = GetWindowTextW(hwnd, title, 512);
  GetClassNameW(hwnd, cls, 256);
  // La clase real de Minecraft Bedrock es "Bedrock". Sólo mirar el título
  // hacía que el overlay se pegara a un navegador con "Minecraft" en el título.
  const bool bedrock = wcscmp(cls, L"Bedrock") == 0;
  const bool named = len > 0 && wcsstr(title, L"Minecraft") != nullptr;
  const bool browser = wcsstr(cls, L"Chrome_WidgetWin") != nullptr;
  if (IsWindowVisible(hwnd) && bedrock && named && !browser) {
    *reinterpret_cast<HWND*>(lParam) = hwnd;
    return FALSE;
  }
  return TRUE;
}

HWND findMinecraftWindow() {
  HWND found = nullptr;
  EnumWindows(enumMinecraftWindow, reinterpret_cast<LPARAM>(&found));
  return found;
}

// ---- Ajustes por módulo: color/opacidad/fondo compartidos por todos los HUD ----
static Color styleInk(const bridge::ModuleStyle& st) {
  return rgba((COLORREF)st.color, std::max(0, std::min(100, st.opacity)));
}
// Panel redondeado con sombra suave y sin borde duro (menos "cuadrado").
static void softPanel(Painter& p, int x, int y, int w, int h, int r, Color fill,
                      bool shadow = true, Color rim = rgba(RGB(0, 0, 0), 0)) {
  if (w <= 0 || h <= 0) return;
  if (shadow) p.roundFill(x + 2, y + 3, w, h, r, rgba(RGB(0x00, 0x00, 0x00), 90));
  p.roundFill(x, y, w, h, r, fill);
  if (rim.a) p.roundFrame(x, y, w, h, r, rim, 1);
  p.fillRect(x + r, y + 1, w - r * 2, 1, rgba(RGB(0xFF, 0xFF, 0xFF), 16));
}

static Color styleFill(const bridge::ModuleStyle& st, int alpha) {
  if (!st.background) return rgba(RGB(0x00, 0x00, 0x00), 0);
  return rgba(RGB(0x10, 0x14, 0x1A), std::max(0, std::min(100, alpha)) * st.opacity / 100);
}

// Caja HUD con relleno negro translúcido y bisel 3D (misma idea de Flarial:
// bg 000000 @0.55, borde/glow opcionales, texto fafafa al 100%).
static void hudBox(Painter& p, int x, int y, int w, int h, const bridge::ModuleStyle& st,
                   int alpha = 150, bool active = false) {
  if (!st.background) return;
  const int a = std::max(0, std::min(255, alpha * st.opacity / 100));
  softPanel(p, x, y, w, h, 7,
            active ? rgba(RGB(0xE8, 0xEC, 0xF2), a) : rgba(RGB(0x05, 0x07, 0x0B), a));
}

static void drawCrosshairPreset(Painter& p, int preset, int cx, int cy, int r, Color c) {
  preset = ((preset % 24) + 24) % 24;
  r = std::max(3, std::min(r, 64));
  const int gap = std::max(1, r / 5);
  const int arm = std::max(2, r / 2);
  auto bar = [&](int x, int y, int w, int h) { p.fillRect(cx + x, cy + y, w, h, c); };
  switch (preset) {
    case 0: p.dot(cx, cy, 1, c); break;
    case 1: p.dot(cx, cy, 2, c); break;
    case 2: bar(-gap, -arm, 1, arm - gap); bar(-gap, gap, 1, arm - gap);
            bar(gap, -arm, 1, arm - gap); bar(gap, gap, 1, arm - gap); break;
    case 3: bar(-arm, -gap, arm - gap, 1); bar(gap, -gap, arm - gap, 1);
            bar(-arm, gap, arm - gap, 1); bar(gap, gap, arm - gap, 1); break;
    case 4: bar(-gap, -arm, 1, arm - gap); bar(-gap, gap, 1, arm - gap);
            bar(gap, -arm, 1, arm - gap); bar(gap, gap, 1, arm - gap); p.dot(cx, cy, 1, c); break;
    case 5: p.ring(cx, cy, std::max(2, r / 3), c); break;
    case 6: p.ring(cx, cy, std::max(2, r / 2), c); p.dot(cx, cy, 1, c); break;
    case 7: bar(-gap, -arm, 1, arm - gap); bar(-gap, gap, 1, arm - gap);
            bar(gap, -arm, 1, arm - gap); bar(gap, gap, 1, arm - gap);
            p.ring(cx, cy, std::max(2, r / 3), c); break;
    case 8: for (int i = -1; i <= 1; ++i) {
              bar(i * 2 - gap, -arm, 1, arm - gap);
              bar(i * 2 - gap, gap, 1, arm - gap);
            } break;
    case 9: p.ring(cx, cy, std::max(2, r / 2), c); bar(-arm, -gap, arm - gap, 1);
            bar(gap, -gap, arm - gap, 1); bar(-arm, gap, arm - gap, 1);
            bar(gap, gap, arm - gap, 1); break;
    case 10: p.dot(cx, cy, std::max(1, r / 4), c); break;
    case 11: {
               for (int i = -1; i <= 1; i += 2)
                 for (int j = -1; j <= 1; j += 2)
                   p.dot(cx + i * gap * 2, cy + j * gap * 2, 1, c);
               break;
             }
    case 12: bar(0, -arm, 1, arm - gap); bar(0, gap, 1, arm - gap);
             bar(-arm, 0, arm - gap, 1); bar(gap, 0, arm - gap, 1); break;
    case 13: bar(-arm, 0, arm - gap, 1); bar(gap, 0, arm - gap, 1);
             bar(0, -arm, 1, arm - gap); bar(0, gap, 1, arm - gap); p.ring(cx, cy, gap + 1, c); break;
    case 14: p.line(cx, cy - r, cx + r, cy, c); p.line(cx + r, cy, cx, cy + r, c);
             p.line(cx, cy + r, cx - r, cy, c); p.line(cx - r, cy, cx, cy - r, c); break;
    case 15: bar(0, -arm, 1, arm - gap); bar(0, gap, 1, arm - gap);
             bar(-arm, 0, arm - gap, 1); bar(gap, 0, arm - gap, 1);
             p.line(cx - gap, cy - gap, cx + gap, cy + gap, c);
             p.line(cx + gap, cy - gap, cx - gap, cy + gap, c); break;
    case 16: bar(-arm, -arm, arm / 2, 1); bar(-arm, -arm, 1, arm / 2);
             bar(arm - arm / 2, -arm, arm / 2, 1); bar(arm - 1, -arm, 1, arm / 2);
             bar(-arm, arm - 1, arm / 2, 1); bar(-arm, arm - arm / 2, 1, arm / 2);
             bar(arm - arm / 2, arm - 1, arm / 2, 1); bar(arm - 1, arm - arm / 2, 1, arm / 2); break;
    case 17: bar(-arm, -gap, arm - gap, 1); bar(gap, -gap, arm - gap, 1);
             bar(-arm, gap, arm - gap, 1); bar(gap, gap, arm - gap, 1);
             bar(-arm, -gap, 1, gap * 2 + 1); bar(arm - 1, -gap, 1, gap * 2 + 1); break;
    case 18: bar(-gap, -arm, 1, arm - gap); bar(-gap, gap, 1, arm - gap);
             bar(gap, -arm, 1, arm - gap); bar(gap, gap, 1, arm - gap);
             bar(-gap - 2, -arm, 1, arm - gap); bar(-gap - 2, gap, 1, arm - gap);
             bar(gap + 2, -arm, 1, arm - gap); bar(gap + 2, gap, 1, arm - gap); break;
    case 19: p.fillRect(cx - gap, cy - gap, gap * 2 + 1, gap * 2 + 1, c); break;
    case 20: p.frameRect(cx - gap * 2, cy - gap * 2, gap * 4 + 1, gap * 4 + 1, c, 1); break;
    case 21: {
               for (int i = -1; i <= 1; i += 2)
                 for (int j = -1; j <= 1; j += 2)
                   p.fillRect(cx + i * gap * 2 - 1, cy + j * gap * 2 - 1, 3, 3, c);
               break;
             }
    case 22: bar(-gap, -arm, 1, arm - gap); bar(-gap, gap, 1, arm - gap);
             bar(gap, -arm, 1, arm - gap); bar(gap, gap, 1, arm - gap);
             bar(-arm, -gap, arm - gap, 1); bar(gap, -gap, arm - gap, 1);
             bar(-arm, gap, arm - gap, 1); bar(gap, gap, arm - gap, 1); break;
    default: p.line(cx - gap, cy - gap, cx + gap, cy + gap, c);
             p.line(cx + gap, cy - gap, cx - gap, cy + gap, c); p.dot(cx, cy, 1, c); break;
  }
}

// Iconos de armadura en píxeles (casco, pecho, piernas, botas, espada).
static void drawArmorIcon(Painter& p, int kind, int x, int y, int s, Color c) {
  const int u = std::max(1, s / 16);
  auto R = [&](int a, int b, int w, int h) {
    p.fillRect(x + a * u, y + b * u, w * u, h * u, c);
  };
  const Color dark = rgba(RGB(0x0C, 0x10, 0x16), 255);
  switch (kind) {
    case 0:  // casco
      R(4, 1, 8, 2); R(2, 3, 12, 2); R(2, 5, 12, 6); R(4, 11, 8, 2);
      p.fillRect(x + 4 * u, y + 7 * u, 8 * u, u, dark);
      break;
    case 1:  // pecho
      R(2, 2, 4, 3); R(10, 2, 4, 3); R(4, 4, 8, 8); R(5, 12, 6, 2);
      p.fillRect(x + 7 * u, y + 5 * u, 2 * u, 6 * u, dark);
      break;
    case 2:  // piernas
      R(3, 2, 4, 12); R(9, 2, 4, 12);
      p.fillRect(x + 3 * u, y + 8 * u, 4 * u, u, dark);
      p.fillRect(x + 9 * u, y + 8 * u, 4 * u, u, dark);
      break;
    case 3:  // botas
      R(3, 2, 4, 8); R(9, 2, 4, 8); R(2, 10, 5, 4); R(9, 10, 5, 4);
      p.fillRect(x + 3 * u, y + 11 * u, 3 * u, u, dark);
      p.fillRect(x + 9 * u, y + 11 * u, 3 * u, u, dark);
      break;
    default:  // espada
      R(7, 1, 2, 9); R(4, 10, 8, 2); R(7, 12, 2, 3); R(6, 15, 4, 1);
      break;
  }
}

static void drawKeystrokesStack(Painter& p, int x, int y, const bridge::ModuleStyle& st,
                                bool w, bool a, bool s, bool d, bool space,
                                bool lmb, bool rmb) {
  const float z = std::max(0.5f, std::min(2.f, st.size / 100.f));
  const int kw = std::max(16, (int)(34 * z));
  const int kh = std::max(12, (int)(24 * z));
  const int gap = std::max(2, (int)(4 * z));
  const int rowW = kw * 3 + gap * 2;              // ancho de A S D
  const int half = (rowW - gap) / 2;              // LMB / RMB juntos
  const Color label = rgba(RGB(0xFF, 0xFF, 0xFF), 255);
  const Color live = rgba(RGB(0x31, 0x57, 0x83), st.opacity);
  auto key = [&](int kx, int ky, int ww, int hh, const wchar_t* text, bool on) {
    hudBox(p, kx, ky, ww, hh, st, 150, on);
    if (on) p.fillRect(kx + 2, ky + hh - 3, ww - 4, 2, live);
    p.textCentered(kx, ky, ww, hh, text, 1,
                   on ? rgba(RGB(0x05, 0x07, 0x0B), 255) : label);
  };
  // W centrada sobre la fila A S D.
  key(x + kw + gap, y, kw, kh, L"W", w);
  key(x, y + kh + gap, kw, kh, L"A", a);
  key(x + kw + gap, y + kh + gap, kw, kh, L"S", s);
  key(x + (kw + gap) * 2, y + kh + gap, kw, kh, L"D", d);
  // Espacio largo: cubre exactamente A S D.
  key(x, y + (kh + gap) * 2, rowW, kh, L"SPACE", space);
  // Clics juntos, uno al lado del otro, debajo del espacio.
  key(x, y + (kh + gap) * 3, half, kh, L"LMB", lmb);
  key(x + half + gap, y + (kh + gap) * 3, rowW - half - gap, kh, L"RMB", rmb);
}

static void drawArmorStack(Painter& p, int x, int y, const bridge::ArmorState& armor,
                           const bridge::ModuleStyle& st) {
  const float z = std::max(0.5f, std::min(2.f, st.size / 100.f));
  const int iconS = std::max(16, (int)(24 * z));
  const int rowH = iconS + 4;
  const int barW = std::max(36, (int)(80 * z));
  const Color ink = rgba(RGB(0xFF, 0xFF, 0xFF), 255);
  const Color live = rgba(RGB(0x31, 0x57, 0x83), st.opacity);
  const int values[5] = {armor.helmet, armor.chest, armor.legs, armor.boots, armor.hand};
  for (int i = 0; i < 5; ++i) {
    const int yy = y + i * rowH;
    hudBox(p, x, yy, iconS, iconS, st, 150);
    drawArmorIcon(p, i, x, yy, iconS, ink);
    const int bx = x + iconS + 6;
    if (values[i] >= 0) {
      const int bw = barW * std::max(0, std::min(100, values[i])) / 100;
      p.fillRect(bx, yy + iconS / 2 - 2, bw, 4, live);
      wchar_t pct[16];
      swprintf(pct, 16, L"%d%%", std::min(100, values[i]));
      p.text(bx, yy + iconS - 9, pct, z >= 1.5f ? 2 : 1, ink);
    } else {
      p.text(bx, yy + iconS - 9, L"—", 1, ink);
    }
  }
}

}  // namespace

// MinGW: sin dependencias de Direct2D/DirectWrite (el overlay pinta con GDI).

OverlayEngine::OverlayEngine() {
  stats_.gamePos.source = "none";
  g_instance = this;
}

OverlayEngine::~OverlayEngine() {
  releaseMenuCursor();
  releaseCosmeticTextures();
  releaseRenderBuffer();
  if (fontTitle_) DeleteObject(fontTitle_);
  if (fontBody_) DeleteObject(fontBody_);
  if (fontValue_) DeleteObject(fontValue_);
  if (fontSmall_) DeleteObject(fontSmall_);
  if (hWnd_) DestroyWindow(hWnd_);
  UnregisterClassW(L"LexOverlayWnd", GetModuleHandleW(nullptr));
  g_instance = nullptr;
}

ATOM OverlayEngine::registerWindowClass(HINSTANCE hInstance) {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = L"LexOverlayWnd";
  wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
  wc.style |= CS_OWNDC;
  return RegisterClassW(&wc);
}

static HWND createWindow() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
  HWND hwnd = CreateWindowExW(exStyle, L"LexOverlayWnd", L"lex-overlay", WS_POPUP, 10, 10, 460, 420, nullptr, nullptr, hInstance, nullptr);
  // Sin LWA_COLORKEY: el contenido se entrega con UpdateLayeredWindow y cada
  // pixel lleva su propio alfa. El juego se ve a través de los paneles.
  return hwnd;
}

static void enableClickThrough(HWND hwnd, bool enable) {
  DWORD ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
  const bool was = (ex & WS_EX_TRANSPARENT) != 0;
  if (was == enable) return;   // cambiar el estilo cada tick hace parpadear el overlay
  if (enable) ex |= WS_EX_TRANSPARENT;
  else ex &= ~WS_EX_TRANSPARENT;
  SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayEngine::setVisible(bool v) {
  if (!hWnd_) return;
  if (v) {
    SetWindowPos(hWnd_, HWND_TOPMOST, config_.positionX, config_.positionY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd_);
  } else {
    ShowWindow(hWnd_, SW_HIDE);
  }
}

void OverlayEngine::updatePosition() {
  if (!hWnd_) return;
  SetWindowPos(hWnd_, HWND_TOPMOST, config_.positionX, config_.positionY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void OverlayEngine::toggleClickThrough() {
  if (!hWnd_) return;
  config_.clickThrough = !config_.clickThrough;
  enableClickThrough(hWnd_, config_.clickThrough);
}

bool OverlayEngine::gameIsForeground() const {
  if (!gameHwnd_ || !IsWindow(gameHwnd_)) return false;
  DWORD gamePid = 0, fgPid = 0;
  GetWindowThreadProcessId(gameHwnd_, &gamePid);
  HWND fg = GetForegroundWindow();
  if (!fg) return false;
  GetWindowThreadProcessId(fg, &fgPid);
  return fgPid == gamePid || fg == gameHwnd_;
}

bool OverlayEngine::overlayNeedsFrame() const {
  if (dirty_) return true;
  if (menuVisible_) {
    // Galería de cosméticos y modales tienen miniaturas animadas.
    return menuPage_ == 1 || keyboardOpen_ || detailOpen_;
  }
  if (!activeCosmeticId(modules_).empty()) return true;
  const auto now = std::chrono::steady_clock::now();
  for (int i = 0; i < 256; ++i) {
    if (keyFlash_[i].time_since_epoch().count() == 0) continue;
    if (now - keyFlash_[i] < std::chrono::milliseconds(220)) return true;
  }
  // HUD: valores dinámicos dos veces por segundo, no 10.
  return tickCount_ % 5 == 0;
}

void OverlayEngine::acquireMenuCursor() {
  if (cursorHeld_) return;
  GetClipCursor(&savedClip_);
  savedCursor_ = GetCursor();
  savedShowCount_ = ShowCursor(TRUE);
  // Minecraft captura el puntero (clip + cursor oculto): liberarlo al abrir F8.
  ClipCursor(nullptr);
  SetCursor(LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW));
  cursorHeld_ = true;

  RECT gr{};
  if (gameHwnd_ && IsWindow(gameHwnd_) && GetWindowRect(gameHwnd_, &gr) && hWnd_) {
    RECT wr{};
    GetWindowRect(hWnd_, &wr);
    SetCursorPos((wr.left + wr.right) / 2, (wr.top + wr.bottom) / 2);
  }
}

void OverlayEngine::releaseMenuCursor() {
  if (!cursorHeld_) return;
  ClipCursor(&savedClip_);
  for (int i = 0; i < 16 && ShowCursor(FALSE) > savedShowCount_; ++i) {}
  if (savedCursor_) SetCursor(savedCursor_);
  cursorHeld_ = false;
}

void OverlayEngine::updateGameFps() {
  // FPS del juego: los escribe el DLL inyectado en el hook de Present.
  // Si el puente no está activo, el overlay muestra su propio FPS (etiquetado OV).
  const bridge::GameBridge gb = bridge::readGameBridge(bridge::getGameBridgePath());
  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  if (gb.valid && gb.fps > 0 && gb.timestamp + 2000 >= now && gb.timestamp <= now + 5000) {
    stats_.gameFpsTga = gb.fps;
    stats_.gameBridgeApi = gb.api;
  } else {
    stats_.gameFpsTga = -1;
    stats_.gameBridgeApi.clear();
  }
  float dt = std::max(0.001f, std::chrono::duration<float>(std::chrono::steady_clock::now() - lastRender_).count());
  float localFps = 1.0f / dt;
  gameFpsEma_ = (gameFpsEma_ < 0) ? localFps : (gameFpsEma_ * 0.75f + localFps * 0.25f);
  stats_.overlayFps = gameFpsEma_;
}

void OverlayEngine::updateCps() {
  // Igual que Flarial: cada pulsación física entra una sola vez y se cuenta
  // en una ventana deslizante de 1000 ms, separando LMB y RMB.
  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  auto prune = [&](std::deque<long long>& q) {
    while (!q.empty() && now - q.front() > 1000) q.pop_front();
    return (int)q.size();
  };
  stats_.cpsLeft = prune(leftClickTimes_);
  stats_.cpsRight = prune(rightClickTimes_);
  stats_.cpsAvg = stats_.cpsLeft;   // compatibilidad con bridge/tests anteriores
  stats_.cpsClickCount = stats_.cpsLeft + stats_.cpsRight;
  stats_.clickCount = (int)totalClicks_;
}

bool OverlayEngine::consumeKey(int vk) {
  // F8 es la única tecla del cliente: abre/cierra el menú y nunca llega al juego.
  if (vk == menuHotkey_) return true;
  // Menú/HUD edit modales: mientras están abiertos NINGUNA tecla llega al juego
  // (si no, 1-9 cambiaría de hotbar, las flechas moverían la cámara
  // y las letras abrirían el chat). Así trabajan los clientes Java.
  return menuVisible_ || hudEdit_;
}

void OverlayEngine::registerKeyState(int vk, bool down) {
  if (vk >= 0 && vk < 256) {
    keyDown_[vk] = down;
    if (down) keyFlash_[vk] = std::chrono::steady_clock::now();
  }
  // Un único estado para SHIFT/CTRL/ALT izquierdos y derechos (keystrokes estilo teclado).
  if (vk == VK_LSHIFT || vk == VK_RSHIFT) {
    keyDown_[VK_SHIFT] = down;
    if (down) keyFlash_[VK_SHIFT] = std::chrono::steady_clock::now();
  }
  if (vk == VK_LCONTROL || vk == VK_RCONTROL) {
    keyDown_[VK_CONTROL] = down;
    if (down) keyFlash_[VK_CONTROL] = std::chrono::steady_clock::now();
  }
  if (vk == VK_LMENU || vk == VK_RMENU) {
    keyDown_[VK_MENU] = down;
    if (down) keyFlash_[VK_MENU] = std::chrono::steady_clock::now();
  }
  requestTick();
}

void OverlayEngine::registerLocalClick(int vk) {
  // Menú/HUD edit abiertos => clic de UI, no del juego. Clics en otras apps tampoco.
  if (menuVisible_ || hudEdit_ || !gameIsForeground()) return;
  std::deque<long long>* q = nullptr;
  if (vk == VK_LBUTTON) q = &leftClickTimes_;
  else if (vk == VK_RBUTTON) q = &rightClickTimes_;
  else return;   // la rueda y el movimiento no son CPS
  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  q->push_back(now);
  totalClicks_++;
  updateCps();
  requestTick();
}

void OverlayEngine::registerKeystroke(int vk) {
  traceKey("recv", vk, menuVisible_, false, menuPage_, menuSel_, hudEdit_ ? 1 : 0);

  // Keystrokes reales del juego: ring de pulsaciones fisicas (sin repeats).
  // Dentro del ClickGUI no se registran teclas: ahi manda el raton.
  if (!menuVisible_ && !hudEdit_ && vk != menuHotkey_ && gameIsForeground()) {
    if (keyLog_.size() >= KEY_LOG_SIZE) keyLog_.erase(keyLog_.begin());
    keyLog_.push_back({vk, true});
  }

  // ESC vuelve del modo "mover HUD" al ClickGUI, igual que F8.
  if (hudEdit_ && vk == VK_ESCAPE) {
    exitHudEdit(true);
    return;
  }
  const bool isMenuHotkey = (vk == menuHotkey_);
  // ClickGUI estricto: F8 sólo abre/cierra. Ninguna otra tecla ejecuta acciones
  // del menú; activar, buscar, mover paneles y cerrar se hace con clic.
  if (!isMenuHotkey) return;

  traceKey("hotkey", vk, menuVisible_, false, menuPage_, menuSel_, hudEdit_ ? 1 : 0);
  hotkeyCount_++;

  if (hudEdit_) {
    // F8 vuelve al ClickGUI; guardar/volver también existe como botón clicable.
    exitHudEdit(true);
  } else if (menuVisible_) {
    menuVisible_ = false;
    keyboardOpen_ = false;
    detailOpen_ = false;
    releaseMenuCursor();
    if (hWnd_) updateGeometry();
    writeUiStateNow();
  } else {
    menuVisible_ = true;
    hudEdit_ = false;
    keyboardOpen_ = false;
    detailOpen_ = false;
    searchText_.clear();
    menuPage_ = 0;
    menuSel_ = 0;
    menuScroll_ = 0;
    cosmeticPage_ = 0;
    reloadCatalog();
    reloadBridgeState();
    reloadHudLayout();
    config_.visible = true;
    if (hWnd_) {
      ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
      updateGeometry();
      acquireMenuCursor();
    }
    writeUiStateNow();
  }
  requestTick();
}
void OverlayEngine::pumpKeys() {
  // Consume a los hooks (stats sync). Timer-side.
}

void OverlayEngine::writeOverlayStatsFile() {
  bridge::OverlayStats st;
  st.overlayFps = stats_.overlayFps;
  st.gameFpsTga = stats_.gameFpsTga;
  st.cpsAvg = stats_.cpsAvg;
  st.cpsLeft = stats_.cpsLeft;
  st.cpsRight = stats_.cpsRight;
  st.cpsClickCount = stats_.clickCount;
  st.keyCount = (int)keyLog_.size();
  st.menuVisible = menuVisible_;
  st.hudEditVisible = hudEdit_;
  st.detailVisible = detailOpen_;
  st.searchActive = keyboardOpen_;
  st.visibleCount = (int)visibleModules().size();
  st.menuPage = menuPage_;
  st.menuScroll = menuScroll_;
  st.cosmeticPage = cosmeticPage_;
  st.moduleCount = (int)modules_.size();
  st.cosmeticCount = (int)cosmetics_.size();
  st.activeCosmetic = activeCosmeticId(modules_);
  st.hotkeyCount = hotkeyCount_;
  st.timestamp = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  bridge::writeOverlayStats(bridge::getOverlayStatsPath(), st);
  writeUiStateNow();
}

void OverlayEngine::writeUiStateNow() {
  bridge::UiState st;
  st.menu = menuVisible_ || hudEdit_;
  st.hudEdit = hudEdit_;
  st.timestamp = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  bridge::writeUiState(bridge::getUiStatePath(), st);
}

LRESULT CALLBACK OverlayEngine::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  OverlayEngine* self = g_instance;
  switch (uMsg) {
    case WM_TIMER:
      if (self) self->onTimerTick();
      break;
    case WM_ERASEBKGND:
      return 1;
    case WM_SETCURSOR:
      // Menú modal: puntero de flecha visible aunque el juego oculte el suyo.
      if (self && (self->menuVisible_ || self->hudEdit_)) {
        SetCursor(LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW));
        return TRUE;
      }
      break;
    case WM_PAINT: {
      // UpdateLayeredWindow ya publica la imagen; BeginPaint sólo valida.
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      if (self) self->renderFrame();
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      if (self) self->registerKeystroke((int)wParam);
      break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      break;
    case WM_MOUSEMOVE:
      if (self) {
        const int mx = (int)(short)LOWORD(lParam);
        const int my = (int)(short)HIWORD(lParam);
        if (self->menuVisible_) self->onMenuMouseMove(mx, my);
        if (self->hudEdit_) self->onHudMouseMove(mx, my);
      }
      break;
    case WM_MOUSEWHEEL: {
      if (self && self->menuVisible_) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT pt{ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        self->onMenuWheel(delta, pt.x, pt.y);
      }
      return 0;
    }
    case WM_LBUTTONDOWN:
      // Menú -> UI; HUD edit -> arrastrar panel; juego -> CPS.
      if (self) {
        const int mx = (int)(short)LOWORD(lParam);
        const int my = (int)(short)HIWORD(lParam);
        if (self->menuVisible_) self->onMenuClick(mx, my);
        else if (self->hudEdit_) self->onHudMouseDown(mx, my);
        // Los clics de juego los cuenta el hook global (una sola vez).
      }
      break;
    case WM_LBUTTONUP:
      if (self) {
        if (self->menuVisible_) self->onMenuMouseUp();
        self->onHudMouseUp();
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void OverlayEngine::onTimerTick() {
  tickCount_++;
  // El overlay sigue al juego: se cierra con él, se oculta si se minimiza
  // y sólo se muestra mientras el juego está en pantalla.
  syncWithGame();
  if (!hWnd_) return;
  const bool shown = IsWindowVisible(hWnd_);

  updateGameFps();
  updateCps();
  // El cursor del juego lo libera el DLL inyectado (hook de ClipCursor/
  // ShowCursor/SetCapture en el hilo de Bedrock), no este proceso.
  const double age = std::chrono::duration<double>(std::chrono::steady_clock::now() - lastBridgeReload_).count();
  if (age > 1.0) reloadBridgeState();
  if (tickCount_ % 25 == 0) ensureGameBridge();
  writeOverlayStatsFile();
  // Un solo repaint por cambio real: los clics sólo marcan dirty_.
  if (shown && overlayNeedsFrame()) {
    InvalidateRect(hWnd_, nullptr, FALSE);
    dirty_ = false;
  }
}

bool OverlayEngine::ensureRenderBuffer(int w, int h) {
  if (w <= 0 || h <= 0) return false;
  if (bufferDc_ && bufferBmp_ && bufferBits_ && bufferW_ == w && bufferH_ == h) return true;
  releaseRenderBuffer();
  HDC screen = GetDC(hWnd_);
  if (!screen) return false;
  bufferDc_ = CreateCompatibleDC(screen);
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;  // top-down: el mismo orden que LEXTEX1
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  bufferBmp_ = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bufferBits_, nullptr, 0);
  ReleaseDC(hWnd_, screen);
  if (!bufferDc_ || !bufferBmp_ || !bufferBits_) {
    releaseRenderBuffer();
    return false;
  }
  bufferOldBmp_ = SelectObject(bufferDc_, bufferBmp_);
  bufferW_ = w;
  bufferH_ = h;
  return true;
}

void OverlayEngine::releaseRenderBuffer() {
  if (bufferDc_ && bufferOldBmp_) SelectObject(bufferDc_, bufferOldBmp_);
  if (bufferBmp_) DeleteObject(bufferBmp_);
  if (bufferDc_) DeleteDC(bufferDc_);
  bufferDc_ = nullptr;
  bufferBmp_ = nullptr;
  bufferOldBmp_ = nullptr;
  bufferBits_ = nullptr;
  bufferW_ = bufferH_ = 0;
}

void OverlayEngine::renderFrame() {
  if (!hWnd_) return;
  lastRender_ = std::chrono::steady_clock::now();

  RECT rc;
  GetClientRect(hWnd_, &rc);
  if (!ensureRenderBuffer(rc.right, rc.bottom)) return;

  Painter p;
  p.bind(bufferBits_, rc.right, rc.bottom);
  p.useFont(fontBody_);   // tipografía TTF real en toda la UI
  p.clear();

  if (menuVisible_) {
    renderMenu(p);
  } else {
    renderHud(p);
    if (hudEdit_) {
      const int w = rc.right;
      p.fillRect(0, 0, w, kEditHeaderH, rgba(RGB(0x10,0x18,0x27), 240));
      p.fillRect(0, kEditHeaderH - 2, w, 2, rgba(RGB(0x31,0x57,0x83), 190));
      p.fillRect(0, kEditHeaderH - 2, 190, 2, rgba(kLexAccent));
      p.bevel(12, 7, 24, 24, rgba(kLexAccent));
      p.textCentered(12, 7, 24, 24, L"L", 2, rgba(RGB(0x06,0x0A,0x0E)));
      p.text(46, 12, L"MOVER HUD", 2, rgba(kLexText));
      p.text(196, 15, L"ARRASTRA EL PANEL · ESQUINA INFERIOR REDIMENSIONA · ESC / LISTO VUELVE", 1,
             rgba(kLexDim));
      const UiRect done = editDoneRect(w);
      p.button(done.x, done.y, done.w, done.h, L"LISTO · VOLVER AL MENÚ",
               rgba(kLexAccent), rgba(RGB(0x04,0x12,0x17)), true, 1);
    }
  }

  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;   // canal alfa real, no colorkey

  RECT wr{};
  GetWindowRect(hWnd_, &wr);
  POINT dst{ wr.left, wr.top };
  POINT src{ 0, 0 };
  SIZE size{ rc.right, rc.bottom };
  UpdateLayeredWindow(hWnd_, nullptr, &dst, &size, bufferDc_, &src, 0, &blend, ULW_ALPHA);
  frameCount_++;
}

std::wstring OverlayEngine::moduleValue(const std::string& id) const {
  wchar_t buf[64] = {0};
  if (id == "hud-fps") {
    // Con puente inyectado: FPS reales del Present de Minecraft. Sin él: OV.
    if (stats_.gameFpsTga >= 0) swprintf(buf, 64, L"FPS: %d", stats_.gameFpsTga);
    else swprintf(buf, 64, L"FPS: OV%d", (int)(stats_.overlayFps + 0.5f));
  } else if (id == "hud-cps") {
    // Mismo formato que Flarial: etiqueta + izquierdo | derecho.
    swprintf(buf, 64, L"CPS: %d | %d", stats_.cpsLeft, stats_.cpsRight);
  } else if (id == "hud-keystrokes") {
    return L"";  // se dibuja un teclado completo en renderHud()
  } else if (id == "hud-clock") {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    swprintf(buf, 64, L"%02d:%02d:%02d", t.wHour, t.wMinute, t.wSecond);
  } else if (id == "hud-memory") {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) swprintf(buf, 64, L"RAM: %lu%%", ms.dwMemoryLoad);
    else swprintf(buf, 64, L"RAM: --");
  } else if (id == "util-session") {
    const int secs = (int)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - started_).count();
    swprintf(buf, 64, L"%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
  } else if (id == "hud-ping" || id == "hud-direction") {
    return L"SIN FUENTE";   // honesto: no hay adaptador validado todavía
  } else if (id.rfind("custom-", 0) == 0) {
    return utf8ToWide(id.substr(7));
  } else {
    return L"ACTIVO";
  }
  return buf;
}

std::vector<size_t> OverlayEngine::hudModules() const {
  std::vector<size_t> out;
  for (size_t i = 0; i < modules_.size(); ++i) {
    if (modules_[i].enabled && isHudModule(modules_[i].id)) out.push_back(i);
  }
  return out;
}

void OverlayEngine::releaseCosmeticTextures() {
  for (auto& t : cosmeticTextures_) {
    if (t.dc) DeleteDC(t.dc);
    if (t.bitmap) DeleteObject(t.bitmap);
  }
  cosmeticTextures_.clear();
}

bool OverlayEngine::ensureCosmeticTexture(const std::string& path) {
  for (const auto& t : cosmeticTextures_) {
    if (t.path == path) return t.bitmap != nullptr;
  }
  if (path.empty()) return false;

  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  char magic[8] = {};
  f.read(magic, 8);
  if (!f || std::memcmp(magic, "LEXTEX1\0", 8) != 0) return false;

  int32_t w = 0, h = 0, frames = 0, frameMs = 0;
  f.read(reinterpret_cast<char*>(&w), 4);
  f.read(reinterpret_cast<char*>(&h), 4);
  f.read(reinterpret_cast<char*>(&frames), 4);
  f.read(reinterpret_cast<char*>(&frameMs), 4);
  if (!f || w <= 0 || h <= 0 || frames <= 0 || frames > 256 ||
      frameMs < 16 || frameMs > 5000 || w > 8192 || h > 8192 ||
      h > std::numeric_limits<int>::max() / frames) return false;

  // El DIB debe contener EL ATLAS COMPLETO, no un solo frame: si sólo
  // reservamos h filas y luego leemos frames*h, sobrescribimos la memoria
  // de GDI y el overlay crashea al blitear (offset c0000005 en blitAtlasFrame).
  const int atlasH = h * frames;
  const size_t pixelBytes = (size_t)w * atlasH * 4;
  f.seekg(0, std::ios::end);
  const std::streamoff total = f.tellg();
  if (total < (std::streamoff)(24 + pixelBytes)) return false;
  f.seekg(24, std::ios::beg);

  HDC screen = GetDC(nullptr);
  if (!screen) return false;
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -atlasH;  // top-down BGRA: todos los frames del atlas
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bmp || !bits) {
    if (bmp) DeleteObject(bmp);
    ReleaseDC(nullptr, screen);
    return false;
  }
  f.read(reinterpret_cast<char*>(bits), pixelBytes);
  HDC dc = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  if (!dc) {
    DeleteObject(bmp);
    return false;
  }
  SelectObject(dc, bmp);

  CosmeticTexture tex;
  tex.path = path;
  tex.bitmap = bmp;
  tex.dc = dc;
  tex.bits = bits;
  tex.width = w;
  tex.height = h;
  tex.frames = frames;
  tex.frameMs = frameMs;
  cosmeticTextures_.push_back(tex);
  return true;
}

static void blitAtlasFrame(Painter& p, const CosmeticTexture& tex, int frame,
                           int dx, int dy, int dw, int dh) {
  if (!tex.bits || tex.width <= 0 || tex.height <= 0 || dw <= 0 || dh <= 0) return;
  const uint8_t* src = (const uint8_t*)tex.bits + (size_t)frame * tex.width * tex.height * 4;
  for (int y = 0; y < dh; ++y) {
    const int sy = (y * tex.height) / dh;
    for (int x = 0; x < dw; ++x) {
      const int sx = (x * tex.width) / dw;
      const uint8_t* px = src + ((size_t)sy * tex.width + sx) * 4;
      p.pixelPremultiplied(dx + x, dy + y, px[0], px[1], px[2], px[3]);
    }
  }
}

void OverlayEngine::renderCosmetics(Painter& p, RECT rc) {
  if (rc.right <= 0 || rc.bottom <= 0) return;
  const std::vector<std::string> ids = activeCosmeticIds(modules_);
  if (ids.empty()) return;

  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  const int count = std::max(1, std::min((int)ids.size(), 8));
  const int gap = 12;
  const int avail = std::max(200, (int)rc.right - 60 - (count - 1) * gap);
  const int dw = std::max(48, std::min(200, avail / count));

  for (int slot = 0; slot < count; ++slot) {
    const std::string& id = ids[(size_t)slot];
    const bridge::Cosmetic* cosmetic = nullptr;
    for (const auto& c : cosmetics_) {
      if (c.id == id) { cosmetic = &c; break; }
    }
    if (!cosmetic || cosmetic->atlas.empty()) continue;

    const std::string path = resolveCosmeticAsset(cosmetic->atlas);
    if (!ensureCosmeticTexture(path)) continue;
    CosmeticTexture* tex = nullptr;
    for (auto& t : cosmeticTextures_) {
      if (t.path == path) { tex = &t; break; }
    }
    if (!tex || tex->frames <= 0 || tex->width <= 0 || tex->height <= 0) continue;

    const int frame = (int)((now / tex->frameMs) % tex->frames);
    const int dh = std::max(32, (int)(tex->height * dw / tex->width));
    const int totalW = count * dw + (count - 1) * gap;
    int dx = (rc.right - totalW) / 2 + slot * (dw + gap);
    int dy = rc.bottom - dh - 24;
    dx = std::max(0, std::min(dx, (int)rc.right - dw));
    dy = std::max(0, std::min(dy, (int)rc.bottom - dh));
    blitAtlasFrame(p, *tex, frame, dx, dy, dw, dh);
  }
}

void OverlayEngine::renderHud(Painter& p) {
  RECT rc{};
  GetClientRect(hWnd_, &rc);

  const Color shadow = rgba(RGB(0x00, 0x00, 0x00), 190);
  auto hudText = [&](int tx, int ty, const std::wstring& s, int scale, Color c) {
    p.text(tx + 1, ty + 1, s, scale, shadow);
    p.text(tx, ty, s, scale, c);
  };
  auto hudTextCentered = [&](int tx, int ty, int tw, int th, const std::wstring& s,
                             int scale, Color c) {
    p.textCentered(tx + 1, ty + 1, tw, th, s, scale, shadow);
    p.textCentered(tx, ty, tw, th, s, scale, c);
  };

  if (!hudEdit_) hudText(6, 6, L"L", 2, rgba(kLexAccent));

  renderCosmetics(p, rc);
  const bridge::ArmorState armor = bridge::readArmorState(bridge::getArmorPath());

  const std::vector<size_t> items = hudModules();
  for (size_t n = 0; n < items.size(); ++n) {
    const bridge::ModuleState& mod = modules_[items[n]];
    const bridge::HudPos pos = [&]() {
      for (const auto& q : hudLayout_) if (q.id == mod.id) return q;
      return defaultHudPos(mod.id, (int)n);
    }();
    const bridge::ModuleStyle st = styleFor(mod.id);
    const float z = std::max(0.5f, std::min(2.f, st.size / 100.f));
    const int pw = std::max(24, (int)(panelWFor(mod.id) * z));
    const int ph = std::max(18, (int)(panelHFor(mod.id) * z));
    const int x = std::max(0, std::min(pos.x, (int)rc.right - pw));
    const int y = std::max(0, std::min(pos.y, (int)rc.bottom - ph));

    // Sin panel grande: sólo el contenido sobre el juego.
    if (hudEdit_) {
      if ((int)n == dragIndex_) {
        p.roundFill(x, y, pw, 3, 2, styleInk(st));
        p.roundFill(x, y + ph - 3, pw, 3, 2, styleInk(st));
      }
      // Asa redondeada de redimensión en la esquina inferior derecha.
      const int hx = x + pw - 16, hy = y + ph - 16;
      const Color grip = rgba(RGB(0xFF, 0xFF, 0xFF), 220);
      softPanel(p, hx, hy, 14, 14, 5, rgba(RGB(0x3F, 0x2A, 0x2D), 245));
      p.line(hx + 4, hy + 9, hx + 10, hy + 3, grip);
      p.line(hx + 7, hy + 10, hx + 11, hy + 6, grip);
    }

    if (mod.id == "hud-keystrokes") {
      const float kz = std::max(0.5f, std::min(2.f, st.size / 100.f));
      const int stackW = std::max(16, (int)(34 * kz)) * 3 +
                         std::max(2, (int)(4 * kz)) * 2;
      const int kx = x + std::max(4, (pw - stackW) / 2);
      drawKeystrokesStack(p, kx, y + 4, st,
                          keyDown_['W'] != 0, keyDown_['A'] != 0,
                          keyDown_['S'] != 0, keyDown_['D'] != 0,
                          keyDown_[VK_SPACE] != 0,
                          keyDown_[VK_LBUTTON] != 0, keyDown_[VK_RBUTTON] != 0);
    } else if (mod.id == "hud-armor") {
      drawArmorStack(p, x + 6, y + 4, armor, st);
    } else {
      hudBox(p, x + 4, y + 4, pw - 8, ph - 8, st, 150);
      p.useFont(fontValue_);
      hudTextCentered(x + 4, y + 4, pw - 8, ph - 8, moduleValue(mod.id),
                      st.size >= 150 ? 3 : 2, rgba(RGB(0xFF, 0xFF, 0xFF), 255));
      p.useFont(fontBody_);
    }
  }

  const bool crosshair = std::any_of(modules_.begin(), modules_.end(),
      [](const bridge::ModuleState& m) { return m.enabled && m.id == "visual-crosshair"; });
  if (crosshair) {
    const bridge::ModuleStyle st = styleFor("visual-crosshair");
    const int r = std::max(3, std::min(40, (int)(10 * st.size / 100.f)));
    const int cx = rc.right / 2, cy = rc.bottom / 2;
    if (st.background) p.fillRect(cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2,
                                  styleFill(st, 120));
    drawCrosshairPreset(p, st.preset, cx, cy, r, styleInk(st));
  }
}

void OverlayEngine::renderMenu(Painter& p) {
  RECT rc{};
  GetClientRect(hWnd_, &rc);
  const int winW = rc.right, winH = rc.bottom;

  const int mo = std::max(0, std::min(100, config_.menuOpacity));
  auto bg = [&](int r, int g, int b, int a) -> Color {
    return rgba(RGB(r, g, b), mo ? (a * mo) / 100 : 0);
  };
  auto edge = [&](int a) -> Color {
    return rgba(RGB(0x8A, 0x96, 0xAE), mo ? std::min(255, (a * mo) / 100 + 24) : 70);
  };
  // Paleta Flarial: negro cálido, nada de azul. Texto blanco, nombre apagado.
  const Color textHi = rgba(RGB(0xFF, 0xFF, 0xFF), 255);
  const Color textMid = rgba(RGB(0x9A, 0xA8, 0xBA), 255);
  const Color textDim = rgba(RGB(0x63, 0x72, 0x8A), 255);
  const Color accent = rgba(RGB(0x31, 0x57, 0x83), 255);
  const Color brand = rgba(RGB(0xF1, 0xF4, 0xF9), 255);
  const Color iconC = rgba(RGB(0xFF, 0xFF, 0xFF), 255);
  const Color green = rgba(RGB(0x31, 0x57, 0x83), 255);   // activo = azul discreto
  const Color red = rgba(RGB(0x17, 0x23, 0x38), 255);     // inactivo = gris azulado

  if (mo > 0) {
    p.fillRect(0, 0, winW, winH, bg(0x00, 0x00, 0x00, 175));
    softPanel(p, 12, 10, winW - 24, winH - 20, 20, bg(0x08, 0x0B, 0x12, 250));
    softPanel(p, 14, 12, winW - 28, kHeaderH - 2, 14, bg(0x10, 0x18, 0x27, 252));
    p.fillRect(30, kHeaderH - 2, winW - 60, 1, bg(0x31, 0x57, 0x83, 190));
  } else {
    p.roundFrame(12, 10, winW - 24, winH - 20, 20, edge(110), 1);
  }

  // ---------------- CABECERA ----------------
  // Zona del logotipo: negro sólido; se dibuja el PNG importado tal cual.
  p.roundFill(12, 12, 52, 52, 10, rgba(RGB(0x00, 0x00, 0x00), 255));
  if (importedLogo().valid) {
    drawImportedLogo(p, 12, 12, 52);
  } else {
    p.useFont(fontTitle_);
    p.textCentered(18, 18, 36, 36, L"L", 3, brand);
  }
  p.text(64, 10, L"LEX CLIENT", 2, textHi);
  p.useFont(fontBody_);
  p.text(66, 40, L"MINECRAFT · BEDROCK", 1, textDim);

  const UiRect search = searchRect();
  if (mo > 0) p.fillRect(search.x, search.y, search.w, search.h, bg(0x0A, 0x0E, 0x13, 235));
  p.frameRect(search.x, search.y, search.w, search.h,
              keyboardOpen_ ? accent : edge(150), keyboardOpen_ ? 2 : 1);
  p.ring(search.x + 18, search.y + 17, 5, keyboardOpen_ ? accent : textDim);
  p.line(search.x + 22, search.y + 21, search.x + 26, search.y + 25,
         keyboardOpen_ ? accent : textDim);
  const std::wstring shown = searchText_.empty() ? L"SEARCH..." : searchText_ + L"|";
  p.text(search.x + 32, search.y + 11, p.ellipsis(shown, search.w - 58, 1), 1,
         searchText_.empty() ? textDim : textHi);

  wchar_t fpsBuf[32], cpsBuf[32];
  swprintf(fpsBuf, 32, L"%d FPS", stats_.gameFpsTga >= 0 ? stats_.gameFpsTga : (int)stats_.overlayFps);
  swprintf(cpsBuf, 32, L"%d | %d", stats_.cpsLeft, stats_.cpsRight);
  p.text(winW - 320, 20, L"GAME", 1, textDim);
  p.text(winW - 320, 36, fpsBuf, 2, textHi);
  p.text(winW - 218, 20, L"CPS", 1, textDim);
  p.text(winW - 218, 36, cpsBuf, 2, textHi);
  const UiRect close = closeRect(winW);
  p.button(close.x, close.y, close.w, close.h, L"X", bg(0x3A, 0x1D, 0x24, 235), red);

  // ---------------- PESTAÑAS (sólo 3 secciones) ----------------
  const wchar_t* tabs[3] = {L"MODS", L"COSMETICS", L"HUD"};
  for (int i = 0; i < 3; ++i) {
    const UiRect t = sectionTabRect(i);
    const bool sel = (menuPage_ == i);
    softPanel(p, t.x, t.y, t.w, t.h, 14, bg(0x17, 0x23, 0x38, sel ? 250 : 190),
              sel, sel ? accent : rgba(RGB(0, 0, 0), 0));
    p.textCentered(t.x, t.y, t.w, t.h, tabs[i], 2, sel ? textHi : textMid);
  }
  const UiRect mr = menuOpacityRect(winW);
  p.button(mr.x, mr.y, mr.w, mr.h,
           (std::wstring(L"PANEL  ") + std::to_wstring(config_.menuOpacity) + L"%").c_str(),
           bg(0x1B, 0x22, 0x2B, 210), textMid, false, 1);

  // ---------------- PANEL LATERAL DE PERFILES ----------------
  const UiRect side = sidebarRect();
  if (mo > 0) softPanel(p, side.x, side.y, side.w, side.h, 14, bg(0x1A, 0x14, 0x17, 235));
  else p.roundFrame(side.x, side.y, side.w, side.h, 14, edge(120), 1);
  p.text(side.x + 8, side.y + 8, L"PERFILES", 2, accent);

  const UiRect drop = profileDropRect();
  if (mo > 0) p.fillRect(drop.x, drop.y, drop.w, drop.h, bg(0x1B, 0x22, 0x2B, 235));
  p.frameRect(drop.x, drop.y, drop.w, drop.h, edge(160), 1);
  const std::wstring activeName = activeProfile_ >= 0 && activeProfile_ < (int)profiles_.size()
      ? utf8ToWide(profiles_[activeProfile_].name) : L"DEFAULT";
  p.text(drop.x + 10, drop.y + 9, activeName, 1, textHi);
  p.text(drop.x + drop.w - 22, drop.y + 8, L"v", 1, textDim);

  for (int i = 0; i < (int)profiles_.size() && i < 6; ++i) {
    const UiRect it = profileItemRect(i);
    const bool sel = (i == activeProfile_);
    if (mo > 0) p.fillRect(it.x, it.y, it.w, it.h, bg(0x1A, 0x20, 0x29, sel ? 250 : 190));
    if (sel) p.frameRect(it.x, it.y, it.w, it.h, accent, 1);
    p.fillRect(it.x + 8, it.y + 9, 18, 18, sel ? accent : edge(170));
    p.text(it.x + 34, it.y + 11, utf8ToWide(profiles_[i].name), 1, sel ? textHi : textMid);
  }
  p.button(saveProfileRect().x, saveProfileRect().y, saveProfileRect().w, saveProfileRect().h,
           L"SAVE AS NEW PROFILE", bg(0x20, 0x26, 0x30, 220), textMid, false, 1);
  const UiRect layout = editLayoutRect();
  p.button(layout.x, layout.y, layout.w, layout.h, L"EDIT HUD LAYOUT",
           bg(0x1F, 0x5A, 0xA8, 240), textHi, false, 1);

  // ---------------- TÍTULO ----------------
  const bool cosTab = (menuPage_ == 1);
  const bool hudTab = (menuPage_ == 2);
  const wchar_t* titles[3] = {L"MODULES", L"COSMETICS", L"HUD"};
  p.text(kContentX, kHeaderH + 48, titles[menuPage_], 2, textHi);
  p.text(kContentX + 130, kHeaderH + 52,
         cosTab ? L"ANIMATED LEXTEX1 ATLASES" : (hudTab ? L"COUNTERS AND PANELS" : L"CLICK TO CONFIGURE"),
         1, textDim);
  if (hudTab) {
    const UiRect mv = moveHudRect(winW);
    p.button(mv.x, mv.y, mv.w, mv.h, L"MOVE HUD", bg(0x20, 0x26, 0x30, 220), accent,
             false, 1);
  }

  const std::vector<size_t> vis = visibleModules();
  const GridGeom gg = gridGeom();

  // ---------------- GALERÍA DE COSMÉTICOS ----------------
  if (cosTab) {
    const int pages = std::max(1, ((int)cosmetics_.size() + kCosPerPage - 1) / kCosPerPage);
    cosmeticPage_ = std::max(0, std::min(cosmeticPage_, pages - 1));
    const int cardH = std::min(kContentB - kContentY, 320);
    const std::string activeId = activeCosmeticId(modules_);
    if (cosmetics_.empty())
      p.text(kContentX + 8, kContentY + 12, L"LIBRARY EMPTY", 2, textMid);
    for (int i = 0; i < kCosPerPage; ++i) {
      const int n = cosmeticPage_ * kCosPerPage + i;
      if (n >= (int)cosmetics_.size()) break;
      const bridge::Cosmetic& c = cosmetics_[n];
      const int x = kContentX + i * (gg.cardW + kGridGap);
      const int y = kContentY;
      const bool isActive = (c.id == activeId);
      softPanel(p, x, y, gg.cardW, cardH, 16, bg(0x20, 0x1A, 0x1B, 244),
                true, isActive ? accent : rgba(RGB(0, 0, 0), 0));
      const UiRect thumb{x + (gg.cardW - 160) / 2, y + 14, 160, 140};
      if (mo > 0) p.fillRect(thumb.x, thumb.y, thumb.w, thumb.h, bg(0x0A, 0x0D, 0x12, 230));
      const std::string path = resolveCosmeticAsset(c.atlas);
      if (!path.empty() && ensureCosmeticTexture(path)) {
        for (auto& t : cosmeticTextures_) {
          if (t.path != path || t.frames <= 0 || !t.bits) continue;
          const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count();
          const int frame = (int)((now / t.frameMs) % t.frames);
          const double scale = std::min((thumb.w - 12) / (double)t.width,
                                        (thumb.h - 12) / (double)t.height);
          const int dw = std::max(1, (int)(t.width * scale));
          const int dh = std::max(1, (int)(t.height * scale));
          blitAtlasFrame(p, t, frame, thumb.x + (thumb.w - dw) / 2,
                         thumb.y + (thumb.h - dh) / 2, dw, dh);
          break;
        }
      }
      p.textCentered(x + 8, y + 162, gg.cardW - 16, 14,
                     p.ellipsis(utf8ToWide(c.name), gg.cardW - 16, 2), 2, textHi);
      wchar_t info[48];
      swprintf(info, 48, L"%d FRAMES · %d MS", c.frames > 0 ? c.frames : 1, c.frameMs);
      p.textCentered(x + 8, y + 190, gg.cardW - 16, 8, info, 1, textDim);
      const UiRect act{x + 16, y + cardH - 46, gg.cardW - 32, 34};
      p.button(act.x, act.y, act.w, act.h, isActive ? L"EQUIPPED" : L"EQUIP",
               isActive ? bg(0x18, 0x52, 0x38, 240) : bg(0x20, 0x26, 0x30, 235),
               isActive ? green : textHi, isActive, 2);
    }
  } else {
    // ---------------- TARJETAS DE MÓDULOS ----------------
    const int count = (int)vis.size();
    const int totalRows = (count + kGridCols - 1) / kGridCols;
    const int maxScroll = std::max(0, totalRows - kGridRows);
    menuScroll_ = std::max(0, std::min(menuScroll_, maxScroll));
    if (count > 0) menuSel_ = std::max(0, std::min(menuSel_, count - 1));
    else menuSel_ = 0;
    if (vis.empty())
      p.text(kContentX + 8, kContentY + 12, L"NO MODULES", 2, textMid);

    for (int row = 0; row < kGridRows; ++row) {
      for (int col = 0; col < kGridCols; ++col) {
        const int idx = (menuScroll_ + row) * kGridCols + col;
        if (idx >= count) break;
        const bridge::ModuleState& mod = modules_[vis[idx]];
        const bridge::ModuleMeta* meta = findMeta(catalog_, mod.id);
        const int x = gg.x0 + col * (gg.cardW + kGridGap);
        const int y = gg.y0 + row * (gg.cardH + kGridGap);
        const bool isSel = (idx == menuSel_);

        // Tarjeta redondeada: sombra suave, sin borde duro salvo selección.
        softPanel(p, x, y, gg.cardW, gg.cardH, 16, bg(0x20, 0x1A, 0x1B, 246),
                  true, isSel ? accent : rgba(RGB(0, 0, 0), 0));

        // Caja del icono: redondeada y con brillo, sin marco negro.
        const int ix = x + gg.cardW / 2 - 34;
        softPanel(p, ix, y + 14, 68, 68, 14, bg(0x1D, 0x35, 0x57, 250));
        drawModuleGlyph(p, mod.id, ix + 6, y + 20, 56,
                        mod.enabled ? iconC : rgba(RGB(0x9A, 0x86, 0x8A), 255));
        const std::wstring name = utf8ToWide(meta && !meta->name.empty() ? meta->name : mod.id);
        const std::wstring shownName = p.ellipsis(name, gg.cardW - 16, 2);
        p.useFont(fontTitle_);
        p.textCentered(x + 8, y + 86, gg.cardW - 16, 22, shownName, 2,
                       mod.enabled ? textHi : textMid);
        p.useFont(fontBody_);
        p.fillRect(x + 12, y + 120, gg.cardW - 24, 1, bg(0x31, 0x57, 0x83, 170));

        const UiRect opt = cardOptionsRect(x, y, gg.cardW, gg.cardH);
        p.button(opt.x, opt.y, opt.w, opt.h, L"OPTIONS",
                 bg(0x17, 0x23, 0x38, 248), textHi, false, 2);
        const int gearX = opt.x + opt.w - 24, gearY = opt.y + opt.h / 2;
        p.ring(gearX, gearY, 5, textHi);
        p.line(gearX - 8, gearY, gearX + 8, gearY, textHi);
        p.line(gearX, gearY - 8, gearX, gearY + 8, textHi);

        const UiRect state = cardStateRect(x, y, gg.cardW, gg.cardH);
        p.button(state.x, state.y, state.w, state.h,
                 mod.enabled ? L"ENABLED" : L"DISABLED",
                 mod.enabled ? bg(0x31, 0x57, 0x83, 252) : bg(0x17, 0x23, 0x38, 252),
                 textHi, mod.enabled, 2);
      }
    }

    const int gridH = gg.cardH * kGridRows + (kGridRows - 1) * kGridGap;
    p.fillRect(winW - 30, gg.y0 + 20, 4, gridH - 40, edge(150));
    if (maxScroll > 0) {
      const int thumbH = (gridH - 40) / (maxScroll + 1);
      const int thumbY = gg.y0 + 20 + (gridH - 40 - thumbH) * menuScroll_ / maxScroll;
      p.fillRect(winW - 30, thumbY, 4, thumbH, accent);
    }
    p.button(scrollUpRect(winW, gg.y0).x, scrollUpRect(winW, gg.y0).y,
             scrollUpRect(winW, gg.y0).w, scrollUpRect(winW, gg.y0).h,
             L"^", bg(0x1B, 0x22, 0x2B, 220), menuScroll_ > 0 ? accent : textDim);
    p.button(scrollDownRect(winW, gg.y0, gridH).x, scrollDownRect(winW, gg.y0, gridH).y,
             scrollDownRect(winW, gg.y0, gridH).w, scrollDownRect(winW, gg.y0, gridH).h,
             L"V", bg(0x1B, 0x22, 0x2B, 220),
             menuScroll_ < maxScroll ? accent : textDim);
  }

  // ---------------- FOOTER ----------------
  p.fillRect(14, winH - kFooterH, winW - 28, kFooterH - 2, bg(0x0D, 0x11, 0x16, 225));
  p.fillRect(14, winH - kFooterH, winW - 28, 2, edge(140));
  p.text(22, winH - kFooterH + 10, L"F8 ABRE / CIERRA", 1, accent);
  p.text(190, winH - kFooterH + 10,
         L"OPTIONS ABRE AJUSTES  ·  EL BOTÓN VERDE/ROJO ACTIVA  ·  TODO CON CLIC",
         1, textDim);

  if (keyboardOpen_) {
    p.fillRect(0, 0, winW, winH, bg(0x05, 0x07, 0x0B, 225));
    const UiRect m = keyboardRect(winW, winH);
    if (mo > 0) p.fillRect(m.x, m.y, m.w, m.h, bg(0x12, 0x16, 0x1D, 250));
    p.frameRect(m.x, m.y, m.w, m.h, accent, 2);
    p.text(m.x + 18, m.y + 12, L"SEARCH  ·  ON-SCREEN KEYBOARD", 2, accent);
    if (mo > 0) p.fillRect(m.x + 18, m.y + 60, m.w - 36, 34, bg(0x0A, 0x0E, 0x13, 235));
    p.text(m.x + 28, m.y + 70, p.ellipsis(searchText_ + L"|", m.w - 56, 2), 2, textHi);
    for (const auto& k : keyboardKeys(m))
      p.button(k.rect.x, k.rect.y, k.rect.w, k.rect.h, utf8ToWide(k.label),
               bg(0x20, 0x26, 0x30, 230), k.action != KEY_CHAR ? accent : textHi);
    p.text(m.x + 18, m.y + m.h - 18, L"CLICK TO TYPE. DONE CLOSES THE KEYBOARD.", 1, textDim);
  }
  if (detailOpen_) renderDetail(p);
}

void OverlayEngine::renderDetail(Painter& p) {
  const std::vector<size_t> vis = visibleModules();
  if (menuPage_ == 1 || vis.empty() || menuSel_ < 0 || (size_t)menuSel_ >= vis.size()) return;
  const bridge::ModuleState& mod = modules_[vis[(size_t)menuSel_]];
  const bridge::ModuleMeta* meta = findMeta(catalog_, mod.id);
  const bridge::ModuleStyle st = styleFor(mod.id);

  RECT rc{};
  GetClientRect(hWnd_, &rc);
  const UiRect d = detailRect(rc.right, rc.bottom);
  const int mo = std::max(0, std::min(100, config_.menuOpacity));
  auto dbg = [&](int r, int g, int b, int a) -> Color {
    return rgba(RGB(r, g, b), mo ? (a * mo) / 100 : 0);
  };
  const Color line = rgba(RGB(0x55, 0x5E, 0x6E), mo ? (mo * 175) / 100 : 80);
  const Color hi = rgba(kLexText);
  const Color mid = rgba(kLexMid);
  const Color dim = rgba(kLexDim);
  const Color accent = rgba(kLexAccent);
  const Color ink = styleInk(st);

  if (mo > 0) {
    p.fillRect(0, 0, rc.right, rc.bottom, dbg(0x04, 0x06, 0x09, 190));
    p.fillRect(d.x, d.y, d.w, d.h, dbg(0x12, 0x16, 0x1D, 250));
  }
  p.frameRect(d.x, d.y, d.w, d.h, line, 1);
  p.fillRect(d.x + 14, d.y, d.w - 28, 2, accent);
  p.fillRect(d.x, d.y + 56, d.w, 1, line);
  p.fillRect(d.x + 350, d.y + 56, 1, d.h - 56, line);

  const std::wstring name = utf8ToWide(meta && !meta->name.empty() ? meta->name : mod.id);
  drawModuleGlyph(p, mod.id, d.x + 18, d.y + 8, 38, ink);
  p.text(d.x + 66, d.y + 8, p.ellipsis(name, d.w - 130, 2), 2, hi);
  p.text(d.x + 68, d.y + 36, p.ellipsis(L"ID  " + utf8ToWide(mod.id), d.w - 136, 1), 1, dim);
  const UiRect close = detailCloseRect(d);
  p.button(close.x, close.y, close.w, close.h, L"X", dbg(0x3A, 0x1D, 0x24, 230), accent);

  // ---------- Columna de ajustes ----------
  p.text(d.x + 28, d.y + 72, L"AJUSTES DEL MÓDULO", 2, accent);

  p.text(d.x + 28, d.y + 100, L"TAMAÑO", 1, mid);
  const UiRect sm = detailSizeMinus(d), sp = detailSizePlus(d);
  p.button(sm.x, sm.y, sm.w, sm.h, L"-", dbg(0x20, 0x26, 0x30, 230), hi);
  p.textCentered(sm.x + sm.w, sm.y, 56, sm.h,
                 std::to_wstring(st.size) + L"%", 1, hi);
  p.button(sp.x, sp.y, sp.w, sp.h, L"+", dbg(0x20, 0x26, 0x30, 230), hi);

  p.text(d.x + 28, d.y + 150, L"OPACIDAD", 1, mid);
  const UiRect om = detailOpacityMinus(d), op = detailOpacityPlus(d);
  p.button(om.x, om.y, om.w, om.h, L"-", dbg(0x20, 0x26, 0x30, 230), hi);
  p.textCentered(om.x + om.w, om.y, 56, om.h,
                 std::to_wstring(st.opacity) + L"%", 1, hi);
  p.button(op.x, op.y, op.w, op.h, L"+", dbg(0x20, 0x26, 0x30, 230), hi);

  const UiRect bgBtn = detailBackground(d);
  p.button(bgBtn.x, bgBtn.y, bgBtn.w, bgBtn.h,
           st.background ? L"FONDO: ACTIVADO" : L"FONDO: SIN FONDO",
           st.background ? dbg(0x1B, 0x4A, 0x3A, 230) : dbg(0x20, 0x26, 0x30, 230),
           st.background ? rgba(RGB(0x6B, 0xE0, 0x9A)) : mid);

  p.text(d.x + 28, d.y + 258, L"COLOR", 1, mid);
  static const COLORREF palette[8] = {
    RGB(0x00, 0xC8, 0xE8), RGB(0xB4, 0x7C, 0xFF), RGB(0xFF, 0x8B, 0xD0),
    RGB(0x65, 0xD6, 0x8F), RGB(0xFF, 0xC8, 0x61), RGB(0xF4, 0xF7, 0xFF),
    RGB(0xFF, 0x6B, 0x78), RGB(0x7E, 0xA6, 0xFF),
  };
  for (int i = 0; i < 8; ++i) {
    const UiRect sw = detailColor(d, i);
    p.fillRect(sw.x, sw.y, sw.w, sw.h, rgba(palette[i], st.opacity));
    if ((unsigned)palette[i] == st.color) p.frameRect(sw.x - 2, sw.y - 2, sw.w + 4, sw.h + 4, accent, 2);
  }

  if (isHudModule(mod.id)) {
    const UiRect mv = detailMove(d);
    p.button(mv.x, mv.y, mv.w, mv.h, L"MOVER EN LA PANTALLA", dbg(0x1B, 0x22, 0x2B, 220), accent);
  }
  const UiRect tog = detailToggle(d);
  p.button(tog.x, tog.y, tog.w, tog.h, mod.enabled ? L"DESACTIVAR MOD" : L"ACTIVAR MOD",
           mod.enabled ? dbg(0x31, 0x57, 0x83, 235) : dbg(0x17, 0x23, 0x38, 235),
           mod.enabled ? rgba(RGB(0x31, 0x57, 0x83)) : rgba(RGB(0x9A, 0xA8, 0xBA)),
           mod.enabled, 2);
  const UiRect rst = detailReset(d);
  p.button(rst.x, rst.y, rst.w, rst.h, L"REINICIAR MOD", dbg(0x20, 0x26, 0x30, 220), mid);

  // ---------- Vista previa en vivo ----------
  const int px = d.x + 368, py = d.y + 72, pw = d.w - 388, ph = 158;
  p.text(px, d.y + 70, L"VISTA PREVIA", 2, accent);
  if (st.background) p.fillRect(px, py + 22, pw, ph, styleFill(st, 150));
  p.frameRect(px, py + 22, pw, ph, line, 1);
  if (mod.id == "visual-crosshair") {
    drawCrosshairPreset(p, st.preset, px + pw / 2, py + 22 + ph / 2,
                        std::max(6, (int)(14 * st.size / 100.f)), ink);
  } else if (mod.id == "hud-keystrokes") {
    drawKeystrokesStack(p, px + 18, py + 38, st, true, true, false, true, false, false, false);
  } else if (mod.id == "hud-armor") {
    drawArmorStack(p, px + 18, py + 38, bridge::readArmorState(bridge::getArmorPath()), st);
  } else {
    p.textCentered(px, py + 22, pw, ph, moduleValue(mod.id), 2, ink);
  }

  if (mod.id == "visual-crosshair") {
    p.text(px, d.y + 226, L"PRESETS  ·  24 MIRAS", 2, mid);
    for (int i = 0; i < 24; ++i) {
      const UiRect tile = detailPreset(d, i);
      p.fillRect(tile.x, tile.y, tile.w, tile.h, dbg(0x16, 0x1B, 0x23, 240));
      if (i == st.preset) p.frameRect(tile.x, tile.y, tile.w, tile.h, accent, 2);
      else p.frameRect(tile.x, tile.y, tile.w, tile.h, line, 1);
      drawCrosshairPreset(p, i, tile.x + tile.w / 2, tile.y + tile.h / 2, 11, hi);
    }
  } else if (meta && !meta->description.empty()) {
    std::wistringstream words(utf8ToWide(meta->description));
    std::wstring word, row;
    int y = d.y + 252;
    while (words >> word && y < d.y + 450) {
      const std::wstring test = row.empty() ? word : row + L" " + word;
      if (!row.empty() && p.textWidth(test, 1) > d.w - 400) {
        p.text(px, y, row, 1, mid);
        row = word;
        y += 17;
      } else {
        row = test;
      }
    }
    if (!row.empty()) p.text(px, y, row, 1, mid);
    p.text(px, d.y + 448, L"SIN CONTORNO. CAMBIA COLOR Y FONDO EN VIVO.", 1, dim);
  }
}

void OverlayEngine::onMenuClick(int x, int y) {
  if (!menuVisible_) return;
  requestTick();
  RECT rc{};
  GetClientRect(hWnd_, &rc);
  const int winW = rc.right, winH = rc.bottom;

  auto closeMenu = [&]() {
    menuVisible_ = false;
    hudEdit_ = false;
    keyboardOpen_ = false;
    detailOpen_ = false;
    releaseMenuCursor();
    if (hWnd_) updateGeometry();
    writeUiStateNow();
  };
  auto toggleModule = [&](size_t idx) {
    if (idx >= modules_.size()) return;
    modules_[idx].enabled = !modules_[idx].enabled;
    bridge::writeModuleStates(bridge::getModulesStatePath(), modules_);
    reloadBridgeState();
    menuSel_ = (int)idx;
  };
  auto toggleCosmetic = [&](int n) {
    if (n < 0 || n >= (int)cosmetics_.size()) return;
    const std::string moduleId = "cosmetic-" + cosmetics_[n].id;
    bool found = false;
    for (auto& m : modules_) {
      if (m.id != moduleId) continue;
      m.enabled = !m.enabled;   // se permiten varios cosméticos activos
      found = true;
      break;
    }
    if (!found) {
      bridge::ModuleState s;
      s.id = moduleId;
      s.enabled = true;
      modules_.push_back(s);
    }
    bridge::writeModuleStates(bridge::getModulesStatePath(), modules_);
    reloadBridgeState();
    menuSel_ = n;
  };
  auto cycleOpacity = [&](int& value) {
    static const int steps[] = {0, 20, 40, 60, 80, 100};
    int next = 0;
    for (int s : steps) if (s > value) { next = s; break; }
    value = next;
    saveOverlayConfig();
  };

  // 1) Teclado en pantalla.
  if (keyboardOpen_) {
    const UiRect m = keyboardRect(winW, winH);
    if (!hit(m, x, y)) { keyboardOpen_ = false; return; }
    for (const auto& k : keyboardKeys(m)) {
      if (!hit(k.rect, x, y)) continue;
      if (k.action == KEY_CHAR && searchText_.size() < 32) {
        searchText_ += (wchar_t)(unsigned char)k.ch;
      } else if (k.action == KEY_BACK && !searchText_.empty()) {
        searchText_.pop_back();
      } else if (k.action == KEY_CLEAR) {
        searchText_.clear();
      } else if (k.action == KEY_DONE) {
        keyboardOpen_ = false;
      }
      menuSel_ = 0;
      menuScroll_ = 0;
      cosmeticPage_ = 0;
      return;
    }
    return;
  }

  // 2) Ajustes del módulo: tamaño / opacidad / color / fondo / presets.
  if (detailOpen_) {
    const UiRect d = detailRect(winW, winH);
    auto selectedId = [&]() -> std::string {
      const std::vector<size_t> vis = visibleModules();
      if (menuSel_ < 0 || (size_t)menuSel_ >= vis.size()) return "";
      return modules_[vis[(size_t)menuSel_]].id;
    };
    auto editStyle = [&](const auto& mutate) {
      const std::string id = selectedId();
      if (id.empty()) return;
      bridge::ModuleStyle st = styleFor(id);
      mutate(st);
      saveModuleStyle(st);
    };

    if (hit(detailCloseRect(d), x, y)) { detailOpen_ = false; return; }
    if (hit(detailSizeMinus(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) { s.size = std::max(50, s.size - 10); });
      return;
    }
    if (hit(detailSizePlus(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) { s.size = std::min(200, s.size + 10); });
      return;
    }
    if (hit(detailOpacityMinus(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) { s.opacity = std::max(0, s.opacity - 10); });
      return;
    }
    if (hit(detailOpacityPlus(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) { s.opacity = std::min(100, s.opacity + 10); });
      return;
    }
    if (hit(detailBackground(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) { s.background = s.background ? 0 : 1; });
      return;
    }
    static const COLORREF palette[8] = {
      RGB(0x00, 0xC8, 0xE8), RGB(0xB4, 0x7C, 0xFF), RGB(0xFF, 0x8B, 0xD0),
      RGB(0x65, 0xD6, 0x8F), RGB(0xFF, 0xC8, 0x61), RGB(0xF4, 0xF7, 0xFF),
      RGB(0xFF, 0x6B, 0x78), RGB(0x7E, 0xA6, 0xFF),
    };
    for (int i = 0; i < 8; ++i) {
      if (!hit(detailColor(d, i), x, y)) continue;
      const unsigned c = (unsigned)palette[i];
      editStyle([c](bridge::ModuleStyle& s) { s.color = c; });
      return;
    }
    if (selectedId() == "visual-crosshair") {
      for (int i = 0; i < 24; ++i) {
        if (!hit(detailPreset(d, i), x, y)) continue;
        editStyle([i](bridge::ModuleStyle& s) { s.preset = i; });
        return;
      }
    }
    if (hit(detailMove(d), x, y)) {
      const std::string id = selectedId();
      if (!id.empty() && isHudModule(id)) enterHudEdit();
      return;
    }
    if (hit(detailToggle(d), x, y)) {
      const std::vector<size_t> vis = visibleModules();
      if (menuSel_ >= 0 && (size_t)menuSel_ < vis.size()) toggleModule(vis[(size_t)menuSel_]);
      return;
    }
    if (hit(detailReset(d), x, y)) {
      editStyle([](bridge::ModuleStyle& s) {
        s.size = 100; s.opacity = 100; s.background = 0;
        s.color = 0xFFFFFF; s.preset = 0;
      });
      return;
    }
    if (!hit(d, x, y)) detailOpen_ = false;
    return;
  }

  // 3) Cabecera: buscador, limpiar y cerrar.
  const UiRect search = searchRect();
  UiRect clr{search.x + search.w - 30, search.y + 6, 24, 22};
  if (hit(clr, x, y) && !searchText_.empty()) {
    searchText_.clear();
    menuSel_ = 0;
    menuScroll_ = 0;
    cosmeticPage_ = 0;
    return;
  }
  if (hit(search, x, y)) { keyboardOpen_ = true; return; }
  if (hit(closeRect(winW), x, y)) { closeMenu(); return; }

  // 4) Sólo tres secciones: MODS / COSMETICS / HUD.
  for (int i = 0; i < 3; ++i) {
    if (!hit(sectionTabRect(i), x, y)) continue;
    menuPage_ = i;
    menuSel_ = 0;
    menuScroll_ = 0;
    cosmeticPage_ = 0;
    detailOpen_ = false;
    keyboardOpen_ = false;
    return;
  }
  if (hit(menuOpacityRect(winW), x, y)) { cycleOpacity(config_.menuOpacity); return; }
  if (menuPage_ == 2 && hit(moveHudRect(winW), x, y)) { enterHudEdit(); return; }

  // Panel de perfiles: aplicar, guardar o editar layout.
  if (x < kSideW && y >= kHeaderH + 8 && y < winH - kFooterH) {
    for (int i = 0; i < (int)profiles_.size() && i < 6; ++i) {
      if (!hit(profileItemRect(i), x, y)) continue;
      applyProfile(i);
      return;
    }
    if (hit(saveProfileRect(), x, y)) { saveNewProfile(); return; }
    if (hit(editLayoutRect(), x, y)) { enterHudEdit(); return; }
    return;
  }

  const GridGeom gg = gridGeom();

  // 5) Galería de cosméticos.
  if (menuPage_ == 1) {
    const int pages = std::max(1, ((int)cosmetics_.size() + kCosPerPage - 1) / kCosPerPage);
    cosmeticPage_ = std::max(0, std::min(cosmeticPage_, pages - 1));
    const int cardH = std::min(kContentB - kContentY, 320);
    if (pages > 1) {
      if (hit(scrollUpRect(winW, kContentY), x, y)) {
        cosmeticPage_ = std::max(0, cosmeticPage_ - 1);
        return;
      }
      if (hit(scrollDownRect(winW, kContentY, cardH), x, y)) {
        cosmeticPage_ = std::min(pages - 1, cosmeticPage_ + 1);
        return;
      }
      if (x >= winW - 34 && x <= winW - 14 && y >= kContentY + 20 && y <= kContentY + cardH - 20) {
        scrollbarDrag_ = true;
        scrollbarAnchorY_ = y;
        scrollbarStart_ = cosmeticPage_;
        return;
      }
    }
    if (x < kContentX || x > kContentR || y < kContentY || y > kContentB) return;
    for (int i = 0; i < kCosPerPage; ++i) {
      const int n = cosmeticPage_ * kCosPerPage + i;
      if (n >= (int)cosmetics_.size()) return;
      const int cx = kContentX + i * (gg.cardW + kGridGap);
      if (!hit({cx, kContentY, gg.cardW, cardH}, x, y)) continue;
      const UiRect act{cx + 16, kContentY + cardH - 46, gg.cardW - 32, 34};
      if (hit(act, x, y)) toggleCosmetic(n);
      else menuSel_ = n;
      return;
    }
    return;
  }

  // 6) Tarjetas grandes: OPCIONES abre el detalle; el resto alterna ON/OFF.
  const std::vector<size_t> vis = visibleModules();
  const int totalRows = ((int)vis.size() + kGridCols - 1) / kGridCols;
  const int maxScroll = std::max(0, totalRows - kGridRows);
  menuScroll_ = std::max(0, std::min(menuScroll_, maxScroll));
  const int gridH = gg.cardH * kGridRows + (kGridRows - 1) * kGridGap;
  if (hit(scrollUpRect(winW, gg.y0), x, y)) {
    menuScroll_ = std::max(0, menuScroll_ - 1);
    return;
  }
  if (hit(scrollDownRect(winW, gg.y0, gridH), x, y)) {
    menuScroll_ = std::min(maxScroll, menuScroll_ + 1);
    return;
  }
  if (maxScroll > 0 && x >= winW - 34 && x <= winW - 14 &&
      y >= gg.y0 + 20 && y <= gg.y0 + gridH - 20) {
    scrollbarDrag_ = true;
    scrollbarAnchorY_ = y;
    scrollbarStart_ = menuScroll_;
    return;
  }
  if (x < gg.x0 || x > kContentR || y < gg.y0) return;
  const int col = (x - gg.x0) / (gg.cardW + kGridGap);
  const int row = (y - gg.y0) / (gg.cardH + kGridGap);
  if (col < 0 || col >= kGridCols || row < 0 || row >= kGridRows) return;
  const int cx = gg.x0 + col * (gg.cardW + kGridGap);
  const int cy = gg.y0 + row * (gg.cardH + kGridGap);
  if (x > cx + gg.cardW || y > cy + gg.cardH) return;
  const int idx = (menuScroll_ + row) * kGridCols + col;
  if (idx < 0 || idx >= (int)vis.size()) return;
  menuSel_ = idx;
  const UiRect opt = cardOptionsRect(cx, cy, gg.cardW, gg.cardH);
  const UiRect state = cardStateRect(cx, cy, gg.cardW, gg.cardH);
  if (hit(opt, x, y)) { detailOpen_ = true; return; }
  if (hit(state, x, y)) { toggleModule(vis[idx]); return; }
  return;  // clic en el cuerpo sólo selecciona la tarjeta
}

void OverlayEngine::onMenuWheel(int delta, int x, int y) {
  (void)x; (void)y;
  if (keyboardOpen_ || detailOpen_) return;
  const int step = delta > 0 ? -1 : 1;
  if (menuPage_ == 1) {
    const int pages = std::max(1, ((int)cosmetics_.size() + kCosPerPage - 1) / kCosPerPage);
    cosmeticPage_ = std::max(0, std::min(pages - 1, cosmeticPage_ + step));
  } else {
    const std::vector<size_t> vis = visibleModules();
    const int totalRows = ((int)vis.size() + kGridCols - 1) / kGridCols;
    const int maxScroll = std::max(0, totalRows - kGridRows);
    menuScroll_ = std::max(0, std::min(maxScroll, menuScroll_ + step));
  }
  requestTick();
}

void OverlayEngine::onMenuMouseMove(int x, int y) {
  (void)x;
  if (!scrollbarDrag_) return;
  const int next = scrollbarStart_ + (y - scrollbarAnchorY_) / 28;
  if (menuPage_ == 1) {
    const int pages = std::max(1, ((int)cosmetics_.size() + kCosPerPage - 1) / kCosPerPage);
    cosmeticPage_ = std::max(0, std::min(pages - 1, next));
  } else {
    const std::vector<size_t> vis = visibleModules();
    const int totalRows = ((int)vis.size() + kGridCols - 1) / kGridCols;
    const int maxScroll = std::max(0, totalRows - kGridRows);
    menuScroll_ = std::max(0, std::min(maxScroll, next));
  }
  requestTick();
}

void OverlayEngine::onMenuMouseUp() {
  if (!scrollbarDrag_) return;
  scrollbarDrag_ = false;
  requestTick();
}

void OverlayEngine::enterHudEdit() {
  reloadHudLayout();
  hudEdit_ = true;
  menuVisible_ = false;
  detailOpen_ = false;
  keyboardOpen_ = false;
  draggingHud_ = false;
  dragIndex_ = -1;
  config_.visible = true;
  if (hWnd_) {
    ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
    updateGeometry();
  }
  writeUiStateNow();
  requestTick();
}

void OverlayEngine::exitHudEdit(bool toMenu) {
  // Liberar la captura ANTES de cambiar el tamaño de la ventana layered.
  if (hWnd_ && GetCapture() == hWnd_) ReleaseCapture();
  draggingHud_ = false;
  resizingHud_ = false;
  resizeIndex_ = -1;
  dragIndex_ = -1;
  saveHudLayout();
  hudEdit_ = false;
  menuVisible_ = toMenu;
  if (!toMenu) {
    keyboardOpen_ = false;
    detailOpen_ = false;
    releaseMenuCursor();
  }
  if (hWnd_) updateGeometry();
  writeUiStateNow();
  requestTick();
}
void OverlayEngine::onHudMouseDown(int x, int y) {
  RECT rc{};
  GetClientRect(hWnd_, &rc);
  if (hit(editDoneRect(rc.right), x, y)) {
    exitHudEdit(true);   // guardar/volver también se hace con clic
    return;
  }
  const std::vector<size_t> items = hudModules();
  for (int n = (int)items.size() - 1; n >= 0; --n) {
    const std::string id = modules_[items[(size_t)n]].id;
    const bridge::ModuleStyle st = styleFor(id);
    const float z = std::max(0.5f, std::min(2.f, st.size / 100.f));
    const int pw = std::max(24, (int)(panelWFor(id) * z));
    const int ph = std::max(18, (int)(panelHFor(id) * z));
    bridge::HudPos pos = defaultHudPos(id, n);
    for (const auto& saved : hudLayout_) if (saved.id == id) { pos = saved; break; }
    const int px = std::max(0, std::min(pos.x, (int)rc.right - pw));
    const int py = std::max(kEditHeaderH, std::min(pos.y, (int)rc.bottom - ph));

    // Esquina inferior derecha: redimensionar con el mismo ratón.
    if (x >= px + pw - 18 && x <= px + pw && y >= py + ph - 18 && y <= py + ph) {
      resizingHud_ = true;
      resizeIndex_ = n;
      resizeStartX_ = x;
      resizeStartY_ = y;
      resizeStartSize_ = st.size;
      SetCapture(hWnd_);
      requestTick();
      return;
    }
    if (x >= px && x <= px + pw && y >= py && y <= py + ph) {
      dragIndex_ = n;
      draggingHud_ = true;
      dragOffX_ = x - px;
      dragOffY_ = y - py;
      SetCapture(hWnd_);
      requestTick();
      return;
    }
  }
  dragIndex_ = -1;
  requestTick();
}

void OverlayEngine::onHudMouseMove(int x, int y) {
  RECT rc{};
  GetClientRect(hWnd_, &rc);
  const std::vector<size_t> items = hudModules();

  if (resizingHud_ && resizeIndex_ >= 0 && (size_t)resizeIndex_ < items.size()) {
    const std::string id = modules_[items[(size_t)resizeIndex_]].id;
    const int delta = ((x - resizeStartX_) + (y - resizeStartY_)) / 2;
    const int baseW = std::max(40, panelWFor(id));
    const int next = resizeStartSize_ + delta * 100 / baseW;
    const int size = std::max(50, std::min(200, next));
    if (size != styleFor(id).size) {
      for (auto& s : styles_) if (s.id == id) { s.size = size; break; }
      requestTick();
    }
    return;
  }

  if (!draggingHud_ || dragIndex_ < 0 || (size_t)dragIndex_ >= items.size()) return;
  const std::string id = modules_[items[(size_t)dragIndex_]].id;
  const bridge::ModuleStyle st = styleFor(id);
  const float z = std::max(0.5f, std::min(2.f, st.size / 100.f));
  const int pw = std::max(24, (int)(panelWFor(id) * z));
  const int ph = std::max(18, (int)(panelHFor(id) * z));
  const int nx = std::max(0, std::min(x - dragOffX_, (int)rc.right - pw));
  const int ny = std::max(kEditHeaderH, std::min(y - dragOffY_, (int)rc.bottom - ph));
  bool found = false;
  for (auto& p : hudLayout_) {
    if (p.id == id) { p.x = nx; p.y = ny; found = true; break; }
  }
  if (!found) hudLayout_.push_back({id, nx, ny});
  requestTick();
}

void OverlayEngine::onHudMouseUp() {
  if (resizingHud_) {
    resizingHud_ = false;
    if (resizeIndex_ >= 0) {
      const std::vector<size_t> items = hudModules();
      if ((size_t)resizeIndex_ < items.size()) {
        const std::string id = modules_[items[(size_t)resizeIndex_]].id;
        saveModuleStyle(styleFor(id));
      }
    }
    resizeIndex_ = -1;
    if (hWnd_ && GetCapture() == hWnd_) ReleaseCapture();
    requestTick();
  }
  if (draggingHud_) {
    saveHudLayout();
    draggingHud_ = false;
    dragIndex_ = -1;
    if (hWnd_ && GetCapture() == hWnd_) ReleaseCapture();
    requestTick();
  }
}

void OverlayEngine::reloadHudLayout() {
  hudLayout_ = bridge::readHudLayout(bridge::getHudLayoutPath());
}

void OverlayEngine::saveHudLayout() {
  bridge::writeHudLayout(bridge::getHudLayoutPath(), hudLayout_);
}

void OverlayEngine::reloadState() {
  // Lee %APPDATA%\lex-client\overlay-state.json (visual/position/click-through).
  bridge::OverlayState st = bridge::readOverlayState(bridge::getOverlayStatePath());
  config_.visible = st.visible;
  config_.clickThrough = st.clickThrough;
  config_.hideOnGameMinimize = st.hideOnGameMinimize;
  if (st.fpsCap > 0 && st.fpsCap <= 1000) config_.fpsCap = st.fpsCap;
  if (st.positionX >= 0 && st.positionX <= 5000) config_.positionX = st.positionX;
  if (st.positionY >= 0 && st.positionY <= 5000) config_.positionY = st.positionY;
  if (st.opacity >= 0.f && st.opacity <= 1.f) config_.opacity = st.opacity;
  if (st.menuOpacity >= 0 && st.menuOpacity <= 100) config_.menuOpacity = st.menuOpacity;
  if (st.hudOpacity >= 0 && st.hudOpacity <= 100) config_.hudOpacity = st.hudOpacity;
  updatePosition();
  if (config_.visible) ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
  else ShowWindow(hWnd_, SW_HIDE);
}

void OverlayEngine::saveOverlayConfig() {
  bridge::OverlayState st;
  st.visible = config_.visible;
  st.clickThrough = config_.clickThrough;
  st.hideOnGameMinimize = config_.hideOnGameMinimize;
  st.fpsCap = config_.fpsCap;
  st.positionX = config_.positionX;
  st.positionY = config_.positionY;
  st.opacity = config_.opacity;
  st.menuOpacity = config_.menuOpacity;
  st.hudOpacity = config_.hudOpacity;
  bridge::writeOverlayState(bridge::getOverlayStatePath(), st);
}

void OverlayEngine::reloadStyles() {
  std::vector<std::string> ids;
  ids.reserve(modules_.size() + 1);
  for (const auto& m : modules_) ids.push_back(m.id);
  if (std::find(ids.begin(), ids.end(), "visual-crosshair") == ids.end())
    ids.push_back("visual-crosshair");
  styles_ = bridge::readModuleStyles(bridge::getModuleStylesPath(), ids);
}

bridge::ModuleStyle OverlayEngine::styleFor(const std::string& id) const {
  for (const auto& st : styles_) if (st.id == id) return st;
  bridge::ModuleStyle fallback;
  fallback.id = id;
  return fallback;
}

void OverlayEngine::saveModuleStyle(const bridge::ModuleStyle& st) {
  bool found = false;
  for (auto& cur : styles_) {
    if (cur.id != st.id) continue;
    cur = st;
    found = true;
    break;
  }
  if (!found) styles_.push_back(st);
  bridge::writeModuleStyles(bridge::getModuleStylesPath(), styles_);
}

void OverlayEngine::reloadProfiles() {
  const std::string path = bridge::getProfilesPath();
  profiles_ = bridge::readProfiles(path);
  if (profiles_.empty()) {
    bridge::Profile def;
    def.id = "default";
    def.name = "DEFAULT";
    for (const auto& m : modules_) def.modules.push_back({m.id, m.enabled});
    profiles_.push_back(def);
    auto preset = [&](const char* id, const char* name,
                      const std::vector<std::string>& on) {
      bridge::Profile p;
      p.id = id;
      p.name = name;
      for (const auto& m : modules_)
        p.modules.push_back({m.id, std::find(on.begin(), on.end(), m.id) != on.end()});
      profiles_.push_back(p);
    };
    preset("uhc", "UHC", {"hud-fps", "hud-cps", "visual-crosshair", "hud-armor"});
    preset("skyblock", "SKYBLOCK", {"hud-cps", "hud-memory", "visual-crosshair"});
    preset("pvp", "ARENA PVP", {"hud-cps", "hud-keystrokes", "visual-crosshair", "hud-armor"});
    bridge::writeProfiles(path, profiles_);
  }
  activeProfile_ = std::max(0, std::min(activeProfile_, (int)profiles_.size() - 1));
}

void OverlayEngine::applyProfile(int index) {
  if (index < 0 || index >= (int)profiles_.size()) return;
  activeProfile_ = index;
  for (const auto& saved : profiles_[index].modules) {
    for (auto& m : modules_) {
      if (m.id == saved.first) { m.enabled = saved.second; break; }
    }
  }
  bridge::writeModuleStates(bridge::getModulesStatePath(), modules_);
  reloadBridgeState();
}

void OverlayEngine::saveNewProfile() {
  bridge::Profile p;
  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  p.id = "profile-" + std::to_string(now % 100000);
  p.name = "PROFILE " + std::to_string(profiles_.size());
  for (const auto& m : modules_) p.modules.push_back({m.id, m.enabled});
  profiles_.push_back(p);
  activeProfile_ = (int)profiles_.size() - 1;
  bridge::writeProfiles(bridge::getProfilesPath(), profiles_);
}

void OverlayEngine::reloadBridgeState() {
  modules_ = bridge::readModuleStates(bridge::getModulesStatePath());
  cosmetics_ = bridge::readCosmetics(bridge::getCosmeticsPath());
  // Recarga el atlas sólo si cambió el JSON o el cosmético activo (anti-parpadeo).
  const std::string sig = cosmeticSignature(cosmetics_, activeCosmeticIds(modules_));
  if (sig != cosmeticsSignature_) {
    releaseCosmeticTextures();
    cosmeticsSignature_ = sig;
  }
  // Eliminar módulos retirados del catálogo (TPS, ping, dirección, etc.).
  std::vector<bridge::ModuleState> kept;
  kept.reserve(modules_.size());
  for (const auto& m : modules_) {
    const bool inCatalog = std::any_of(catalog_.begin(), catalog_.end(),
        [&](const bridge::ModuleMeta& c) { return c.id == m.id; });
    const bool validCosmetic = m.id.rfind("cosmetic-", 0) == 0 &&
        std::any_of(cosmetics_.begin(), cosmetics_.end(),
            [&](const bridge::Cosmetic& c) { return "cosmetic-" + c.id == m.id; });
    if (inCatalog || validCosmetic) kept.push_back(m);
  }
  modules_ = kept;

  // El catálogo completo debe estar representado: lo que falte entra apagado.
  for (const auto& c : catalog_) {
    const bool found = std::any_of(modules_.begin(), modules_.end(),
                                   [&](const bridge::ModuleState& m) { return m.id == c.id; });
    if (!found) {
      bridge::ModuleState s;
      s.id = c.id;
      s.enabled = false;
      modules_.push_back(s);
    }
  }
  reloadStyles();
  reloadProfiles();
  lastBridgeReload_ = std::chrono::steady_clock::now();
}

void OverlayEngine::reloadCatalog() {
  catalog_ = bridge::readCatalog(bridge::getCatalogPath());
}

std::vector<size_t> OverlayEngine::visibleModules() const {
  std::vector<size_t> out;
  out.reserve(modules_.size());
  for (size_t i = 0; i < modules_.size(); ++i) {
    const std::string& id = modules_[i].id;
    const bridge::ModuleMeta* meta = findMeta(catalog_, id);
    if (menuPage_ == 1) continue;                              // cosméticos tienen su galería
    if (menuPage_ == 2 && !isHudModule(id)) continue;           // HUD
    if (menuPage_ == 0 && id.rfind("cosmetic-", 0) == 0) continue;
    if (!matchesSearch(lowerWide(searchText_), meta, id)) continue;
    out.push_back(i);
  }
  return out;
}

// La ventana vive SOBRE el juego: se dimensiona y coloca según el estado.
// Con menú abierto → panel centrado en el juego y con clics activos.
// Con menú cerrado → HUD del tamaño del juego, click-through,
// para no comerse los clics del jugador; en HUD edit vuelve a aceptar clics.
void OverlayEngine::updateGeometry() {
  if (!hWnd_) return;

  RECT gr{};
  bool haveGame = gameHwnd_ && IsWindow(gameHwnd_) && GetWindowRect(gameHwnd_, &gr);
  int baseX = 0, baseY = 0, gw = 0, gh = 0;
  if (haveGame) {
    baseX = gr.left;
    baseY = gr.top;
    gw = gr.right - gr.left;
    gh = gr.bottom - gr.top;
  } else {
    gw = GetSystemMetrics(SM_CXSCREEN);
    gh = GetSystemMetrics(SM_CYSCREEN);
  }

  int w, h, x, y;
  if (menuVisible_) {
    w = kMenuW;
    h = kMenuH;
    x = baseX + (gw - w) / 2;
    y = baseY + (gh - h) / 2;
    if (x < baseX) x = baseX;
    if (y < baseY) y = baseY;
  } else {
    // El HUD vive DENTRO de la ventana del juego: ocupa su rect para poder
    // colocar módulos en cualquier punto (y arrastrarlos en HUD edit).
    w = gw > 0 ? gw : kHudW;
    h = gh > 0 ? gh : kHudH;
    x = baseX;
    y = baseY;
  }

  const RECT want{x, y, x + w, y + h};
  const bool wantClickThrough = !(menuVisible_ || hudEdit_);
  if (lastGeomValid_ && EqualRect(&want, &lastGeomRect_) &&
      lastGeomClickThrough_ == wantClickThrough) {
    return;   // nada que mover: no hay reposicionamiento ni parpadeo
  }

  SetWindowPos(hWnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
  enableClickThrough(hWnd_, wantClickThrough);
  lastGeomRect_ = want;
  lastGeomClickThrough_ = wantClickThrough;
  lastGeomValid_ = true;
}

// El overlay es un cliente DEL JUEGO, no del sistema:
//  - se cierra cuando se cierra el juego,
//  - se oculta cuando el juego se minimiza o deja de estar visible,
//  - sólo se muestra encima del juego (no flota sobre otras aplicaciones).
void OverlayEngine::syncWithGame() {
  if (!hWnd_) return;

  if (!gameHwnd_ || !IsWindow(gameHwnd_)) {
    // Puede recrearse la ventana (cambio de pantalla completa): rebuscamos
    // unos segundos antes de dar el juego por cerrado.
    HWND again = findMinecraftWindow();
    if (again) {
      gameHwnd_ = again;
      gameGoneTicks_ = 0;
      SetWindowLongPtr(hWnd_, GWLP_HWNDPARENT, (LONG_PTR)gameHwnd_);
    } else if (++gameGoneTicks_ > 20) {   // ~4 s de margen
      PostQuitMessage(0);
      return;
    }
  } else {
    gameGoneTicks_ = 0;
  }

  if (!gameHwnd_ || !IsWindow(gameHwnd_)) return;

  const bool gameShown = IsWindowVisible(gameHwnd_) && !IsIconic(gameHwnd_);

  DWORD gamePid = 0;
  GetWindowThreadProcessId(gameHwnd_, &gamePid);
  HWND fg = GetForegroundWindow();
  DWORD fgPid = 0;
  if (fg) GetWindowThreadProcessId(fg, &fgPid);
  const bool gameFocused = (fg == nullptr) || (fgPid == gamePid);

  // Con el menú o el editor abiertos el overlay debe verse aunque el foco
  // esté en el launcher: si no, F8 parece "no hacer nada".
  const bool modal = menuVisible_ || hudEdit_;
  if (!modal && !(gameShown && gameFocused)) {
    if (IsWindowVisible(hWnd_)) ShowWindow(hWnd_, SW_HIDE);
    lastGameShown_ = gameShown;
    lastGameFocused_ = gameFocused;
    return;
  }
  if (modal) {
    if (!IsWindowVisible(hWnd_)) ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
    updateGeometry();
    return;
  }

  if (gameShown != lastGameShown_ || gameFocused != lastGameFocused_) {
    lastGameShown_ = gameShown;
    lastGameFocused_ = gameFocused;
    if (!IsWindowVisible(hWnd_)) ShowWindow(hWnd_, SW_SHOWNOACTIVATE);
  }
  updateGeometry();
}

int OverlayEngine::run() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);
  if (!registerWindowClass(hInstance)) { fprintf(stderr, "[LEX] registerWindowClass falló\n"); return 1; }
  hWnd_ = createWindow();
  if (!hWnd_) { fprintf(stderr, "[LEX] createWindow falló\n"); return 1; }
  if (gameHwnd_) {
    SetWindowLongPtr(hWnd_, GWLP_HWNDPARENT, (LONG_PTR)gameHwnd_);
    SetWindowPos(hWnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    // Puente de FPS reales: una sola inyección por PID del juego.
    ensureGameBridge();
  }

  // Fuentes GDI: Direct2D no escribe en ventanas WS_EX_LAYERED, así que
  // el overlay se pinta con GDI (vía WM_PAINT) — eso sí llega a la pantalla.
  // ANTIALIASED (no ClearType): en una ventana LAYERED con colorkey, ClearType
  // deja bordes de color. Con antialias grayscale el texto se lee limpio.
  fontTitle_ = CreateFontW(-26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  fontBody_ = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  fontValue_ = CreateFontW(-19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
  fontSmall_ = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  if (!fontTitle_ || !fontBody_ || !fontSmall_) {
    fprintf(stderr, "[LEX] CreateFont falló\n");
    return 1;
  }

  // Catálogo del launcher (nombres, categorías y descripciones reales).
  reloadCatalog();
  reloadBridgeState();
  reloadHudLayout();
  reloadState();
  SetTimer(hWnd_, 1, config_.tickMs, nullptr);
  SetTimer(hWnd_, 2, 500, nullptr);
  InvalidateRect(hWnd_, nullptr, FALSE);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return (int)msg.wParam;
}

}  // namespace engine
}  // namespace lex
