#include "Shake.h"
namespace input {
bool ShakeDetector::feed(float g, uint32_t ms) {
  if (g < 1.3f) { armed_ = true; return false; }
  if (!armed_ || g < 2.0f) return false;
  armed_ = false;
  peaks_[0] = peaks_[1]; peaks_[1] = peaks_[2]; peaks_[2] = ms;
  if (n_ < 3) n_++;
  if (n_ == 3 && peaks_[2] - peaks_[0] <= 800) { n_ = 0; return true; }
  return false;
}
}
