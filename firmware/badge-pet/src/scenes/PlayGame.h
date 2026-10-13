#pragma once
#include <M5Unified.h>
#include "../render/Geometry.h"
namespace scenes {
// 10 s: sparks fall; tilt to move the mascot and catch 3 to win.
class PlayGame {
 public:
  void start(uint32_t ms);
  void update(float tiltX, uint32_t ms);
  render::Rect drawSparks(M5Canvas& c) const;
  float mascotX() const { return x_; }
  bool done(uint32_t ms) const { return ms - t0_ >= 10000 || caught_ >= 3; }
  bool won() const { return caught_ >= 3; }
  int caught() const { return caught_; }
 private:
  uint32_t t0_ = 0, last_ = 0; float x_ = render::CENTER, sx_ = render::CENTER, sy_ = 60; int caught_ = 0; uint32_t seed_ = 7;
};
}
