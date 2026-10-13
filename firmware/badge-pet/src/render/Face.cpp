#include "Face.h"
#include "Colors.h"
#include <math.h>
namespace render {
// Face shapes are written upright relative to the anchor; these rotate them with the body.
static float gAx = 0, gAy = 0, gCs = 1, gSn = 0;
static void rot(float& x, float& y) { const float dx = x - gAx, dy = y - gAy; x = gAx + dx * gCs - dy * gSn; y = gAy + dx * gSn + dy * gCs; }
static void ellipse(M5Canvas& c, float x, float y, float rx, float ry, uint32_t col) { rot(x, y); c.fillEllipse((int32_t)x, (int32_t)y, (int32_t)rx, (int32_t)ry, col); }
static void line(M5Canvas& c, float x0, float y0, float x1, float y1, float r, uint32_t col) {
  rot(x0, y0); rot(x1, y1);
  c.drawWideLine((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, r, col);
}
static void arc(M5Canvas& c, float cx, float cy, float rad, float a0, float a1, float r, uint32_t col) {
  const int n = 8;
  float px = cx + cosf(a0) * rad, py = cy + sinf(a0) * rad;
  for (int i = 1; i <= n; i++) {
    const float a = a0 + (a1 - a0) * i / n, x = cx + cosf(a) * rad, y = cy + sinf(a) * rad;
    line(c, px, py, x, y, r, col); px = x; py = y;
  }
}
void drawFace(M5Canvas& c, Expression e, float x, float y, float s, uint32_t ms, bool blink, float rotation, float mouth) {
  gAx = x; gAy = y; gCs = cosf(rotation); gSn = sinf(rotation);
  const float ex = 21 * s, r = 9 * s, PI_F = 3.14159265f;
  for (int side = -1; side <= 1; side += 2) {
    const float cx = x + side * ex;
    switch (e) {
      case Expression::Happy:   arc(c, cx, y + 6 * s, 11 * s, PI_F * 1.1f, PI_F * 1.9f, r * 0.7f, INK); break;
      case Expression::Sleep:   arc(c, cx, y - 4 * s, 10 * s, PI_F * 0.15f, PI_F * 0.85f, r * 0.6f, INK); break;
      case Expression::Sick:    line(c, cx - 9 * s, y + 2 * s, cx + 9 * s, y + 6 * s, r * 0.7f, INK); break;
      case Expression::Sleepy:  line(c, cx - 9 * s, y + 2 * s, cx + 9 * s, y + 2 * s, r * 0.6f, INK); break;
      case Expression::Grumpy:
        line(c, cx - 10 * s, y - 2 * s, cx + 10 * s, y - 2 * s, r * 0.55f, INK);
        line(c, cx, y, cx, y + 5 * s, r, INK); break;
      case Expression::Tantrum:
        line(c, cx - side * 8 * s, y - 7 * s, cx + side * 8 * s, y, r * 0.6f, INK);
        line(c, cx + side * 8 * s, y, cx - side * 8 * s, y + 7 * s, r * 0.6f, INK); break;
      case Expression::Dizzy: {
        const float rot = ms / 120.0f;
        arc(c, cx, y, 9 * s, rot, rot + PI_F * 1.5f, r * 0.45f, INK); break;
      }
      case Expression::Sad:     line(c, cx, y - 3 * s, cx, y + 8 * s, r, INK); break;
      case Expression::Hungry: { const float h = 18 * s; line(c, cx, y - h / 2, cx, y + h / 2, r, INK); break; }
      case Expression::Listening: { ellipse(c, cx, y, 9 * s, 11 * s, INK); ellipse(c, cx - 3 * s, y - 4 * s, 3 * s, 3 * s, CREAM); break; }
      case Expression::Thinking:  { const float h = 13 * s; line(c, cx + 4 * s, y - 6 * s - h / 2, cx + 4 * s, y - 6 * s + h / 2, r, INK); break; }
      case Expression::Speaking:  { const float h = blink ? 1 : 13 * s; line(c, cx, y - h / 2, cx, y + h / 2, r, INK); break; }
      case Expression::Idle:
      default: { const float h = blink ? 1 : 13 * s; line(c, cx, y - h / 2, cx, y + h / 2, r, INK); break; }
    }
  }
  const float mr = r * 0.5f;
  switch (e) {
    case Expression::Happy:
      ellipse(c, x - 44 * s, y + 22 * s, 13 * s, 7 * s, BLUSH);
      ellipse(c, x + 44 * s, y + 22 * s, 13 * s, 7 * s, BLUSH);
      arc(c, x, y + 22 * s, 10 * s, PI_F * 0.15f, PI_F * 0.85f, mr, INK); break;
    case Expression::Hungry: {
      const float ry = 9 * s + sinf(ms / 200.0f) * 2 * s;
      ellipse(c, x, y + 32 * s, 7 * s + mr, ry + mr, INK);
      ellipse(c, x, y + 32 * s, 7 * s - mr, ry - mr, CORAL); break;
    }
    case Expression::Sick: {
      float px = x - 18 * s, py = y + 26 * s;
      for (int i = 1; i <= 6; i++) { const float nx = x - 18 * s + i * 6 * s, ny = y + 30 * s + (i % 2 ? 4 : -4) * s; line(c, px, py, nx, ny, mr, INK); px = nx; py = ny; }
      break;
    }
    case Expression::Thinking: {   // three drifting dots above-right
      for (int i = 0; i < 3; i++) { const float ph = fmodf(ms / 600.0f + i / 3.0f, 1.0f); ellipse(c, x + (40 + i * 16) * s, y - (40 + ph * 30) * s, 4 * s, 4 * s, UI); }
      break;
    }
    case Expression::Speaking: {   // mouth opens with the reply's loudness
      const float open = 2 * s + mouth * 14 * s;
      ellipse(c, x, y + 32 * s, 9 * s + mr, open + mr, INK);
      if (open > 3 * s) ellipse(c, x, y + 32 * s, 9 * s - mr, open - mr, CORAL);
      break;
    }
    case Expression::Tantrum: ellipse(c, x, y + 30 * s, 10 * s, 8 * s, INK); break;
    case Expression::Grumpy:  line(c, x - 9 * s, y + 30 * s, x + 9 * s, y + 30 * s, mr, INK); break;
    case Expression::Sad:     arc(c, x, y + 38 * s, 9 * s, PI_F * 1.15f, PI_F * 1.85f, mr, INK); break;
    default: break;
  }
}
}
