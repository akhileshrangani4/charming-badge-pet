#pragma once
#include <stdint.h>
namespace power {
constexpr uint32_t DIM_MS = 30000, OFF_MS = 120000, WAKE_EVERY_S = 15 * 60;
constexpr uint8_t BRIGHT = 160, DIMMED = 30;
// Display calls here must not overlap a frame push: the app calls these only while the push task is idle.
class PowerMgr {
 public:
  void begin();
  bool wokeByTimer() const { return timerWake_; }
  void activity(uint32_t ms);   // also restores brightness after a dim
  void update(uint32_t ms);
  void dimNow();
  bool shouldSleep(uint32_t ms) const { return ms - lastActivity_ >= OFF_MS; }
  bool dimmed() const { return dimmed_; }
  [[noreturn]] void sleepNow(uint32_t seconds);
  void buzz(int pulses);
 private:
  uint32_t lastActivity_ = 0; bool dimmed_ = false, timerWake_ = false;
};
}
