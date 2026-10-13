#include "Effects.h"
#include "Colors.h"
#include <math.h>
namespace render {
// Mascot-space point (SVG units) to screen, including the pose's rotation about the screen center.
static void at(const Pose& p, float x, float y, int& sx, int& sy) {
  const float dx = p.cx + (x - 256) * p.scale - CENTER, dy = p.cy + (y - 256) * p.scale - CENTER;
  const float cs = cosf(p.rot), sn = sinf(p.rot);
  sx = (int)(CENTER + dx * cs - dy * sn); sy = (int)(CENTER + dx * sn + dy * cs);
}
static Rect around(int x, int y, int r) { return Rect{(int16_t)(x - r), (int16_t)(y - r), (int16_t)(2 * r), (int16_t)(2 * r)}; }
static Rect box(int x0, int y0, int x1, int y1) { return Rect{(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)}; }

Rect drawSparkles(M5Canvas& c, const Pose& p, uint32_t ms) {
  const float pts[3][2] = {{60, 120}, {440, 150}, {420, 60}};
  Rect r;
  for (int i = 0; i < 3; i++) {
    int x, y; at(p, pts[i][0], pts[i][1], x, y);
    const float s = (6 + sinf(ms / 200.0f + i) * 3) * p.scale / 0.6f;
    c.fillTriangle(x, (int)(y - 2 * s), (int)(x + 0.5f * s), y, (int)(x - 0.5f * s), y, UI);
    c.fillTriangle(x, (int)(y + 2 * s), (int)(x + 0.5f * s), y, (int)(x - 0.5f * s), y, UI);
    c.fillTriangle((int)(x - 2 * s), y, x, (int)(y - 0.5f * s), x, (int)(y + 0.5f * s), UI);
    c.fillTriangle((int)(x + 2 * s), y, x, (int)(y - 0.5f * s), x, (int)(y + 0.5f * s), UI);
    r = r.unite(around(x, y, (int)(2 * s) + 3));
  }
  return r;
}

Rect drawZs(M5Canvas& c, const Pose& p, uint32_t ms) {
  Rect r;
  for (int i = 0; i < 3; i++) {
    const float ph = fmodf(ms / 2400.0f + i / 3.0f, 1.0f);
    const float sz = (10 + ph * 14) * p.scale / 0.6f;
    int x, y; at(p, 400 + ph * 80, 120 - ph * 140, x, y);
    const uint8_t a = (uint8_t)(sinf(ph * 3.14159f) * 138);
    const uint32_t col = (uint32_t)a << 16 | (uint32_t)a << 8 | a;
    c.drawWideLine(x, y, (int)(x + sz), y, 1.5f, col);
    c.drawWideLine((int)(x + sz), y, x, (int)(y + sz), 1.5f, col);
    c.drawWideLine(x, (int)(y + sz), (int)(x + sz), (int)(y + sz), 1.5f, col);
    r = r.unite(Rect{(int16_t)(x - 4), (int16_t)(y - 4), (int16_t)(sz + 8), (int16_t)(sz + 8)});
  }
  // include the full drift path so last frame's z is always cleared
  int x0, y0, x1, y1; at(p, 400, 120, x0, y0); at(p, 480, -20, x1, y1);
  return r.unite(Rect{(int16_t)(std::min(x0, x1) - 30), (int16_t)(std::min(y0, y1) - 30), (int16_t)(abs(x1 - x0) + 60), (int16_t)(abs(y1 - y0) + 60)});
}

Rect drawSweat(M5Canvas& c, const Pose& p, uint32_t ms) {
  const float dy = fmodf(ms / 25.0f, 40.0f);
  int x, y; at(p, 360, 200 + dy, x, y);
  c.fillEllipse(x, y, (int)(7 * p.scale / 0.6f), (int)(10 * p.scale / 0.6f), SWEAT);
  int x0, y0; at(p, 360, 220, x0, y0);
  return around(x0, y0, (int)(60 * p.scale) + 10);
}

Rect drawMesses(M5Canvas& c, const Pose& p, uint8_t count) {
  const float spots[3][2] = {{70, 450}, {450, 455}, {20, 470}};
  Rect box;
  for (int i = 0; i < count && i < 3; i++) {
    int x, y; at(p, spots[i][0], spots[i][1], x, y);
    const int r = (int)(14 * p.scale / 0.6f);
    c.fillCircle(x, y, r + 2, INK); c.fillCircle(x, y, r, MESS);
    c.fillCircle(x, y - r, (int)(r * 0.7f) + 2, INK); c.fillCircle(x, y - r, (int)(r * 0.7f), MESS);
    box = box.unite(around(x, y - r / 2, 2 * r + 4));
  }
  return box;
}

Rect drawFoodBubble(M5Canvas& c, const Pose& p) {
  int x, y; at(p, 440, 70, x, y);
  const int r = (int)(42 * p.scale / 0.6f);
  c.fillCircle(x, y, r, TFT_BLACK); c.drawCircle(x, y, r, DIM); c.drawCircle(x, y, r - 1, DIM);
  c.drawCircle(x - r, y + r, 6, DIM);
  c.fillArc(x, y + 2, r / 2, r / 2 - 3, 0, 180, UI);
  c.drawFastHLine(x - r / 2, y + 2, r, UI);
  return box(x - r - 10, y - r - 2, x + r + 2, y + r + 12);
}

Rect drawFootprints(M5Canvas& c) {
  for (int i = 0; i < 7; i++) {
    const int x = 91 + i * 46, y = (int)(341 - sinf(i * 0.9f) * 26 + (i % 2 ? 12 : -12));
    c.fillEllipse(x, y, 9, 6, 0x4D4D4Du);
  }
  return box(71, 291, 401, 391);
}

Rect drawSleepCap(M5Canvas& c, const Pose& p) {
  int tx, ty, lx, ly, rx, ry; at(p, 280, 30, tx, ty); at(p, 240, 95, lx, ly); at(p, 330, 95, rx, ry);
  c.fillTriangle(tx, ty, lx, ly, rx, ry, 0x3B5BA9u);
  c.fillCircle(tx, ty, (int)(10 * p.scale / 0.6f), UI);
  int x0, y0; at(p, 285, 62, x0, y0);
  return around(x0, y0, (int)(70 * p.scale) + 14);
}
}
