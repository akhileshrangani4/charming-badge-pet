#include "TextLayer.h"
#include "assets/fonts.h"
#include <math.h>
namespace render {
static const uint8_t* fontData(Font f) {
  switch (f) {
    case Font::Head48: return FONT_HEAD48;
    case Font::Head32: return FONT_HEAD32;
    case Font::Body20: return FONT_BODY20;
    default:           return FONT_MONO15;
  }
}
void TextLayer::use(M5Canvas& c, Font f) {
  int i = canvas_[0] == &c ? 0 : (canvas_[1] == &c ? 1 : -1);
  if (i < 0) { i = canvas_[0] ? 1 : 0; canvas_[i] = &c; loaded_[i] = -1; }
  if (loaded_[i] == (int)f) return;
  c.unloadFont();
  c.loadFont(fontData(f));
  loaded_[i] = (int)f;
}
int TextLayer::maxWidthAt(int y) {
  const float dy = fabsf((float)(y - CENTER));
  if (dy >= RADIUS - 20) return 0;
  return (int)(2 * sqrtf((float)(RADIUS - 20) * (RADIUS - 20) - dy * dy)) - 24;
}
Rect TextLayer::textAt(M5Canvas& c, Font f, const char* s, int x, int y, uint32_t color) {
  if (!s || !*s) return {};
  use(c, f);
  c.setTextDatum(middle_center);
  c.setTextColor(color);
  const int w = c.textWidth(s), h = c.fontHeight();
  c.drawString(s, x, y);
  return Rect{(int16_t)(x - w / 2 - 2), (int16_t)(y - h / 2 - 2), (int16_t)(w + 4), (int16_t)(h + 4)};
}
Rect TextLayer::headline(M5Canvas& c, const char* s, int y, uint32_t color) {
  use(c, Font::Head48);
  if (c.textWidth(s) <= maxWidthAt(y)) return text(c, Font::Head48, s, y, color);
  return text(c, Font::Head32, s, y, color);
}
}
