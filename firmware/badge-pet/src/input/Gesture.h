#pragma once
#include <stdint.h>
namespace input {
enum class Gesture : uint8_t { None, Tap, Stroke };
constexpr float STROKE_MIN_PATH = 120;  constexpr uint32_t STROKE_MIN_MS = 150;
constexpr float TAP_MAX_MOVE = 18;      constexpr uint32_t TAP_MAX_MS = 450;
class GestureTracker {
 public:
  void begin(int x, int y, uint32_t ms) { active_ = true; sx_ = lx_ = x; sy_ = ly_ = y; t0_ = ms; path_ = 0; }
  void move(int x, int y);
  Gesture end(uint32_t ms);
  bool active() const { return active_; }
 private:
  bool active_ = false; int sx_ = 0, sy_ = 0, lx_ = 0, ly_ = 0; uint32_t t0_ = 0; float path_ = 0;
};
}
