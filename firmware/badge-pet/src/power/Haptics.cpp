#include "Haptics.h"
#include <M5Unified.h>
namespace haptics {
static uint16_t onMs_ = 0, gapMs_ = 0; static uint8_t left_ = 0, strength_ = 0;
static bool on_ = false; static uint32_t nextMs_ = 0;
void pulse(uint16_t onMs, uint8_t count, uint16_t gapMs, uint8_t strength) {
  onMs_ = onMs; gapMs_ = gapMs; left_ = count; strength_ = strength; on_ = false; nextMs_ = millis();
}
void update(uint32_t ms) {
  if ((!left_ && !on_) || (int32_t)(ms - nextMs_) < 0) return;
  if (on_) { M5.Power.setVibration(0); on_ = false; nextMs_ = ms + gapMs_; }
  else if (left_) { M5.Power.setVibration(strength_); on_ = true; left_--; nextMs_ = ms + onMs_; }
}
bool busy() { return left_ || on_; }
}
