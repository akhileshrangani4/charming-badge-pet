#pragma once
#include <stdint.h>
namespace input {
class ShakeDetector {
 public:
  bool feed(float g, uint32_t ms);   // g = |accel| in units of g
 private:
  uint32_t peaks_[3] = {0, 0, 0}; int n_ = 0; bool armed_ = true;
};
}
