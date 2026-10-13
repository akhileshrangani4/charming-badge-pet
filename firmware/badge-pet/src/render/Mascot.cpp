#include "Mascot.h"
#include "Colors.h"
#include "assets/flame_points.h"
#include <math.h>
namespace render {
static_assert(FLAME_N == MASCOT_N, "regenerate flame_points.h");

static float hash01(uint32_t v) { v ^= v >> 16; v *= 0x7feb352du; v ^= v >> 15; v *= 0x846ca68bu; v ^= v >> 16; return (v & 0xFFFFFF) / 16777216.0f; }

void Mascot::poke(int x, int y) {
  for (int i = 0; i < MASCOT_N; i++) {
    const float px = pose_.cx + (FLAME_PTS[i][0] - 256) * pose_.scale, py = pose_.cy + (FLAME_PTS[i][1] - 256) * pose_.scale;
    const float dx = px - x, dy = py - y, d = sqrtf(dx * dx + dy * dy) + 0.001f, k = fmaxf(0, 1 - d / 160);
    velX_[i] += dx / d * 22 * k; velY_[i] += dy / d * 22 * k;
  }
  hop_ = 1;
}

bool Mascot::hitTest(int x, int y) const {
  return x >= bounds_.x && x < bounds_.x + bounds_.w && y >= bounds_.y && y < bounds_.y + bounds_.h;
}

void Mascot::update(uint32_t ms) {
  float dt = lastMs_ ? (ms - lastMs_) / 1000.0f : 0.016f;
  if (dt > 0.05f) dt = 0.05f;
  lastMs_ = ms;
  const float steps = dt * 60, damp = powf(0.86f, steps);
  for (int i = 0; i < MASCOT_N; i++) {
    velX_[i] += -offX_[i] * 0.18f * steps; velY_[i] += -offY_[i] * 0.18f * steps;
    velX_[i] *= damp; velY_[i] *= damp;
    offX_[i] += velX_[i] * steps; offY_[i] += velY_[i] * steps;
  }
  hop_ *= powf(0.92f, steps);
  const float e = fminf(1, dt * 6);
  pose_.cx += (target_.cx - pose_.cx) * e; pose_.cy += (target_.cy - pose_.cy) * e; pose_.scale += (target_.scale - pose_.scale) * e;
  // Rotate so the feet point at gravity: down (0,1) maps to (sin a, cos a) when rot = -a. Ease along the shortest arc.
  float want = -gravAngle_ * gravWeight_ + target_.rot, d = want - pose_.rot;
  while (d > 3.14159f) d -= 6.28318f;
  while (d < -3.14159f) d += 6.28318f;
  pose_.rot += d * fminf(1, dt * 8);
  if (ms > nextWander_) {
    wanderX_ = ((rnd() % 1000) / 500.0f - 1) * 0.5f; wanderY_ = ((rnd() % 1000) / 500.0f - 1) * 0.3f;
    nextWander_ = ms + 900 + rnd() % 1800;
  }
  const float tx = lookTX_ + wanderX_ * (1 - fminf(1, fabsf(lookTX_))), ty = lookTY_ + wanderY_ * (1 - fminf(1, fabsf(lookTY_)));
  const float le = fminf(1, dt * 8);
  lookX_ += (tx - lookX_) * le; lookY_ += (ty - lookY_) * le;
  if (!blinking_ && ms > nextBlink_) { blinking_ = true; blinkStart_ = ms; }
  if (blinking_ && ms - blinkStart_ > 160) { blinking_ = false; nextBlink_ = ms + 2200 + rnd() % 3000; }
  boil_ = ms / 110 + 7;
}

void Mascot::deform(float x, float y, int i, uint32_t ms, float& ox, float& oy) const {
  const float t = (float)ms;
  const bool slow = expr_ == Expression::Sleep;
  const float br = sinf(t / (slow ? 1400.0f : 650.0f)) * (slow ? 0.035f : 0.022f);
  float sx = 1 - br * 0.6f, sy = 1 + br, hopY = 0;
  switch (expr_) {
    case Expression::Happy: { const float b = fabsf(sinf(t / 230)); hopY = -b * 22; sy += b < 0.25f ? -0.08f : 0.03f; sx += b < 0.25f ? 0.06f : -0.02f; break; }
    case Expression::Hungry: { const float b = fmaxf(0, sinf(t / 180)) * (sinf(t / 1300) > 0.3f ? 1 : 0); hopY = -b * 12; break; }
    case Expression::Sick: sy *= 0.9f; sx *= 1.07f; break;
    case Expression::Tantrum: sx += sinf(t / 45) * 0.03f; hopY = -fabsf(sinf(t / 120)) * 10; break;
    case Expression::Sad: case Expression::Sleepy: sy *= 0.95f; sx *= 1.03f; break;
    default: break;
  }
  const float g = 1 + growth_ * 0.04f;
  sx *= g; sy *= g;
  hopY -= hop_ * 10;
  float nx = 256 + (x - 256) * sx, ny = 459 + (y - 459) * sy + hopY;
  if (y < 170) {
    const float w = (170 - y) / 128, amp = slow ? 0.3f : (expr_ == Expression::Sick ? 0.15f : 1.0f);
    nx += sinf(t / 160 + y / 30) * 7 * w * amp;
    if (expr_ == Expression::Sick) nx += w * w * 26;
  }
  nx += lookX_ * 12 * (459 - y) / 400;   // lean toward the low side and slide along the floor
  nx += (hash01(boil_ * 131 + i * 17) - 0.5f) * 2.2f;
  ny += (hash01(boil_ * 131 + i * 17 + 1) - 0.5f) * 2.2f;
  const float sx0 = pose_.cx + (nx - 256) * pose_.scale - CENTER, sy0 = pose_.cy + (ny - 256) * pose_.scale - CENTER;
  const float cs = cosf(pose_.rot), sn = sinf(pose_.rot);
  ox = CENTER + sx0 * cs - sy0 * sn;
  oy = CENTER + sx0 * sn + sy0 * cs;
}

// Active-edge scanline fill: each row only visits the few edges that cross it, not all n.
// rim != 0 also draws 2 px of `rim` just outside each span, a cheap light edge so the ink reads on black.
static void fillPoly(M5Canvas& c, const float* xs, const float* ys, int n, uint32_t col, int ymin, int ymax, uint32_t rim = 0) {
  constexpr int MAX_E = 256, ROWS = SCREEN;
  static int16_t head[ROWS], next[MAX_E], yEnd[MAX_E];
  static float xAt[MAX_E], slope[MAX_E];
  static int16_t active[MAX_E];
  if (ymin < 0) ymin = 0;
  if (ymax > ROWS - 1) ymax = ROWS - 1;
  if (ymin > ymax || n > MAX_E) return;
  for (int y = ymin; y <= ymax; y++) head[y] = -1;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    float x0 = xs[j], y0 = ys[j], x1 = xs[i], y1 = ys[i];
    if (y0 == y1) continue;
    if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    int ys0 = (int)ceilf(y0 - 0.5f), ys1 = (int)ceilf(y1 - 0.5f) - 1;   // rows whose centers lie in [y0, y1)
    if (ys0 < ymin) ys0 = ymin;
    if (ys1 > ymax) ys1 = ymax;
    if (ys0 > ys1) continue;
    slope[i] = (x1 - x0) / (y1 - y0);
    xAt[i] = x0 + (ys0 + 0.5f - y0) * slope[i];
    yEnd[i] = (int16_t)ys1;
    next[i] = head[ys0]; head[ys0] = (int16_t)i;
  }
  int na = 0;
  float nodes[32];
  for (int y = ymin; y <= ymax; y++) {
    for (int e = head[y]; e >= 0; e = next[e]) active[na++] = (int16_t)e;
    int cnt = 0;
    for (int k = 0; k < na; k++) { if (cnt < 32) nodes[cnt++] = xAt[active[k]]; }
    for (int a = 1; a < cnt; a++) { float v = nodes[a]; int b = a - 1; while (b >= 0 && nodes[b] > v) { nodes[b + 1] = nodes[b]; b--; } nodes[b + 1] = v; }
    for (int k = 0; k + 1 < cnt; k += 2) {
      const int x0 = (int)lroundf(nodes[k]), x1 = (int)lroundf(nodes[k + 1]);
      if (x1 > x0) {
        c.drawFastHLine(x0, y, x1 - x0, col);
        if (rim) { c.drawFastHLine(x0 - 2, y, 2, rim); c.drawFastHLine(x1, y, 2, rim); }
      }
    }
    int w = 0;
    for (int k = 0; k < na; k++) { const int e = active[k]; if (yEnd[e] > y) { xAt[e] += slope[e]; active[w++] = (int16_t)e; } }
    na = w;
  }
}

Rect Mascot::draw(M5Canvas& c, uint32_t ms) {
  float px[MASCOT_N], py[MASCOT_N];
  for (int i = 0; i < MASCOT_N; i++) {
    deform(FLAME_PTS[i][0], FLAME_PTS[i][1], i, ms, px[i], py[i]);
    px[i] += offX_[i]; py[i] += offY_[i];
  }
  constexpr int SUB = 3, M = MASCOT_N * SUB;
  static float qx[M], qy[M];
  float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
  for (int i = 0; i < MASCOT_N; i++) {
    const int p = (i + MASCOT_N - 1) % MASCOT_N, n = (i + 1) % MASCOT_N;
    const float ax = (px[p] + px[i]) / 2, ay = (py[p] + py[i]) / 2, ex = (px[i] + px[n]) / 2, ey = (py[i] + py[n]) / 2;
    for (int k = 0; k < SUB; k++) {
      const float t = (float)k / SUB, u = 1 - t;
      const float x = u * u * ax + 2 * u * t * px[i] + t * t * ex, y = u * u * ay + 2 * u * t * py[i] + t * t * ey;
      qx[i * SUB + k] = x; qy[i * SUB + k] = y;
      minX = fminf(minX, x); maxX = fmaxf(maxX, x); minY = fminf(minY, y); maxY = fmaxf(maxY, y);
    }
  }
  const float s = pose_.scale;
  if (fabsf(pose_.rot) < 0.35f) {   // the floor shadow is axis-aligned, so only draw it near upright
    const float sx0 = pose_.cx - CENTER, sy0 = pose_.cy + 206 * s - CENTER, cs = cosf(pose_.rot), sn = sinf(pose_.rot);
    c.fillEllipse((int32_t)(CENTER + sx0 * cs - sy0 * sn), (int32_t)(CENTER + sx0 * sn + sy0 * cs), (int32_t)(120 * s), (int32_t)(14 * s), SHADOW);
  }
  // Ink outline = outer offset polygon filled with ink, then the body inset by the same amount.
  // Two scanline fills are ~8x cheaper than anti-aliased wide-line strokes on this panel.
  const float sr = 12 * s * 1.15f * 0.5f + 0.5f;
  float area = 0;
  for (int i = 0, j = M - 1; i < M; j = i++) area += (qx[j] - qx[i]) * (qy[j] + qy[i]);
  const float outward = area > 0 ? 1.0f : -1.0f;
  static float ox[M], oy[M], ix[M], iy[M];   // static: too big for the 8 KB loop stack
  for (int i = 0; i < M; i++) {
    const int p = (i + M - 1) % M, n = (i + 1) % M;
    float tx = qx[n] - qx[p], ty = qy[n] - qy[p];
    const float len = sqrtf(tx * tx + ty * ty) + 1e-4f;
    const float nx = ty / len * outward, ny = -tx / len * outward;
    ox[i] = qx[i] + nx * sr; oy[i] = qy[i] + ny * sr;
    ix[i] = qx[i] - nx * sr; iy[i] = qy[i] - ny * sr;
  }
  const uint32_t body = expr_ == Expression::Sick ? CORAL_SICK : CORAL;
  fillPoly(c, ox, oy, M, INK, (int)floorf(minY - sr), (int)ceilf(maxY + sr), RIM);
  fillPoly(c, ix, iy, M, body, (int)floorf(minY), (int)ceilf(maxY));
  if (expr_ != Expression::Sick) {
    float hx0 = 0, hy0 = 0;
    for (int i = 0; i < HL_N; i++) {
      float hx, hy; deform(HL_PTS[i][0], HL_PTS[i][1], (i * 7) % MASCOT_N, ms, hx, hy);
      if (i) c.drawWideLine((int32_t)hx0, (int32_t)hy0, (int32_t)hx, (int32_t)hy, 6 * s, CREAM);
      hx0 = hx; hy0 = hy;
    }
  }
  float fx, fy; deform(260, 268, 0, ms, fx, fy);
  const float cs = cosf(pose_.rot), sn = sinf(pose_.rot), lx = lookX_ * 14 * s, ly = lookY_ * 10 * s;
  drawFace(c, expr_, fx + lx * cs - ly * sn, fy + lx * sn + ly * cs, s, ms, blinking_, pose_.rot);
  bounds_ = Rect{(int16_t)minX, (int16_t)minY, (int16_t)(maxX - minX + 1), (int16_t)(maxY - minY + 1)}.padded((int)(sr + 3));
  if (fabsf(pose_.rot) < 0.35f) bounds_ = bounds_.unite(Rect{(int16_t)(pose_.cx - 122 * s), (int16_t)(pose_.cy + 190 * s), (int16_t)(244 * s), (int16_t)(34 * s)});
  return bounds_;
}
}
