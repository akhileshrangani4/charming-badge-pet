#include "Gesture.h"
#include <math.h>
namespace input {
void GestureTracker::move(int x, int y) {
  if (!active_) return;
  const int dx = x - lx_, dy = y - ly_;
  if (dx * dx + dy * dy <= 4) return;   // ignore touch jitter of 2 px or less, or a resting thumb adds up to a "stroke"
  path_ += sqrtf((float)(dx * dx + dy * dy));
  lx_ = x; ly_ = y;
}
Gesture GestureTracker::end(uint32_t ms) {
  if (!active_) return Gesture::None;
  active_ = false;
  const uint32_t dur = ms - t0_;
  const float disp = sqrtf((float)((lx_ - sx_) * (lx_ - sx_) + (ly_ - sy_) * (ly_ - sy_)));
  if (path_ >= STROKE_MIN_PATH && dur >= STROKE_MIN_MS) return Gesture::Stroke;
  if (disp <= TAP_MAX_MOVE && path_ <= TAP_MAX_MOVE * 2 && dur <= TAP_MAX_MS) return Gesture::Tap;
  return Gesture::None;
}
}
