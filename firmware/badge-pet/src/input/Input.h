#pragma once
#include <stdint.h>
#include "Gesture.h"
#include "Shake.h"
#include "ButtonArbiter.h"
namespace input {
// Measured on the badge: held upright the accelerometer reads ax = -1, flat face-up az = +1.
// So in screen coordinates (x right, y down) gravity is (ay, -ax).
constexpr float TILT_FULL_G = 0.5f;   // sideways gravity that counts as full tilt (game steering)
struct InputFrame {
  ButtonEvent button = ButtonEvent::None;
  bool tap = false; int tapX = 0, tapY = 0;
  bool stroke = false, shake = false, any = false;
  float tiltX = 0, tiltY = 0;     // -1..1, screen-space gravity (right/down positive)
  float upAngle = 0, upWeight = 0; // gravity direction from screen-down (rad), and how much the screen is tilted toward vertical (0 flat .. 1 upright)
};
class Input {
 public:
  void begin();
  InputFrame poll(uint32_t ms);

 private:
  GestureTracker gesture_; ShakeDetector shake_; ButtonArbiter buttons_;
  float gx_ = 0, gy_ = 0;
};
}
