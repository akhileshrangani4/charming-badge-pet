#pragma once
#include <stdint.h>
#include <algorithm>
namespace render {
constexpr int SCREEN = 468, CENTER = 234, RADIUS = 233;   // panel is 468x468; the visible circle is ~466
struct Rect {
  int16_t x = 0, y = 0, w = 0, h = 0;
  bool empty() const { return w <= 0 || h <= 0; }
  Rect unite(const Rect& o) const {
    if (empty()) return o; if (o.empty()) return *this;
    int x0 = std::min(x, o.x), y0 = std::min(y, o.y);
    int x1 = std::max(x + w, o.x + o.w), y1 = std::max(y + h, o.y + o.h);
    return {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
  }
  Rect padded(int p) const { return {(int16_t)(x - p), (int16_t)(y - p), (int16_t)(w + 2 * p), (int16_t)(h + 2 * p)}; }
  Rect clippedEven() const {   // clamp to the screen and align x/w to even pixels (AMOLED column pairs)
    int x0 = std::max(0, (int)x) & ~1, y0 = std::max(0, (int)y);
    int x1 = std::min(SCREEN, x + w), y1 = std::min(SCREEN, y + h);
    x1 = std::min(SCREEN, (x1 + 1) & ~1);
    if (x1 <= x0 || y1 <= y0) return {};
    return {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
  }
};
}
