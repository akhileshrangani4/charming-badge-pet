#include "PlayGame.h"
#include "../render/Colors.h"
namespace scenes {
void PlayGame::start(uint32_t ms) { t0_ = last_ = ms; x_ = render::CENTER; caught_ = 0; seed_ = ms | 1; sx_ = 141 + (ms % 190); sy_ = 70; }
void PlayGame::update(float tiltX, uint32_t ms) {
  const float dt = (ms - last_) / 1000.0f; last_ = ms;
  x_ += tiltX * 260 * dt;
  if (x_ < 131) x_ = 131;
  if (x_ > 337) x_ = 337;
  sy_ += 150 * dt;
  if (sy_ > 330) {
    if (sx_ > x_ - 45 && sx_ < x_ + 45) caught_++;
    seed_ = seed_ * 1664525u + 1013904223u;
    sx_ = 141 + (seed_ >> 8) % 190; sy_ = 70;
  }
}
render::Rect PlayGame::drawSparks(M5Canvas& c) const {
  c.fillCircle((int)sx_, (int)sy_, 9, render::UI);
  for (int i = 0; i < 3; i++) c.fillCircle(204 + i * 30, 40, 7, i < caught_ ? render::UI : 0x262626u);
  return render::Rect{121, 25, 226, 320};
}
}
