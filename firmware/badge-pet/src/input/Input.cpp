#include "Input.h"
#include <M5Unified.h>
#include <math.h>
namespace input {
void Input::begin() {}
InputFrame Input::poll(uint32_t ms) {
  InputFrame f;
  f.button = buttons_.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(), ms);
  if (M5.Touch.isEnabled()) {
    const auto d = M5.Touch.getDetail();
    if (d.wasPressed()) gesture_.begin(d.x, d.y, ms);
    else if (d.isPressed()) gesture_.move(d.x, d.y);
    if (d.wasReleased() && gesture_.active()) {
      const Gesture g = gesture_.end(ms);
      if (g == Gesture::Tap) { f.tap = true; f.tapX = d.x; f.tapY = d.y; }
      if (g == Gesture::Stroke) f.stroke = true;
    }
    if (d.isPressed()) f.any = true;
  }
  if (M5.Imu.update()) {
    const auto& imu = M5.Imu.getImuData();
    const float ax = imu.accel.x, ay = imu.accel.y, az = imu.accel.z;
    gx_ += (ay - gx_) * 0.3f; gy_ += (-ax - gy_) * 0.3f;   // screen-space gravity, lightly smoothed
    f.shake = shake_.feed(sqrtf(ax * ax + ay * ay + az * az), ms);
  }
  auto clamp1 = [](float v) { return v < -1 ? -1.0f : (v > 1 ? 1.0f : v); };
  f.tiltX = clamp1(gx_ / TILT_FULL_G); f.tiltY = clamp1(gy_ / TILT_FULL_G);
  const float m = sqrtf(gx_ * gx_ + gy_ * gy_);
  f.upAngle = atan2f(gx_, gy_);
  f.upWeight = m < 0.3f ? 0 : (m > 0.7f ? 1 : (m - 0.3f) / 0.4f);
  if (f.button != ButtonEvent::None || f.tap || f.stroke || f.shake || M5.BtnA.isPressed() || M5.BtnB.isPressed()) f.any = true;
  return f;
}
}
