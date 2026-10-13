#pragma once
#include <M5Unified.h>
#include "Geometry.h"
namespace render {
enum class Font : uint8_t { Head48, Head32, Body20, Mono15 };
class TextLayer {
 public:
  Rect text(M5Canvas& c, Font f, const char* s, int y, uint32_t color) { return textAt(c, f, s, CENTER, y, color); }
  Rect textAt(M5Canvas& c, Font f, const char* s, int x, int y, uint32_t color);   // middle-center datum at (x, y)
  Rect headline(M5Canvas& c, const char* s, int y, uint32_t color);       // Head48, or Head32 if too wide
  static int maxWidthAt(int y);
 private:
  void use(M5Canvas& c, Font f);
  // Fonts are loaded per canvas, and the app double-buffers two canvases, so remember both.
  const M5Canvas* canvas_[2] = {nullptr, nullptr};
  int loaded_[2] = {-1, -1};
};
}
