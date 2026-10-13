#include "ButtonArbiter.h"
namespace input {
ButtonEvent ButtonArbiter::update(bool a, bool b, uint32_t ms) {
  ButtonEvent ev = ButtonEvent::None;
  if (a && !aDown_) aT_ = ms;
  if (b && !bDown_) bT_ = ms;
  if (a && b && !both_) { both_ = true; bothT_ = ms; }
  if (both_) {
    if (a && b && !consumed_ && ms - bothT_ >= BOTH_HOLD_MS) { consumed_ = true; ev = ButtonEvent::BothHold; }
  } else if (!consumed_) {
    // single holds wait out a grace window so a second button can still join
    if (a && !b && ms - aT_ >= HOLD_MS + BOTH_GRACE_MS) { consumed_ = true; ev = ButtonEvent::AHold; }
    if (b && !a && ms - bT_ >= HOLD_MS + BOTH_GRACE_MS) { consumed_ = true; ev = ButtonEvent::BHold; }
  }
  if (!a && aDown_ && !b && !consumed_ && !both_) ev = ButtonEvent::AClick;
  if (!b && bDown_ && !a && !consumed_ && !both_) ev = ButtonEvent::BClick;
  aDown_ = a; bDown_ = b;
  if (!a && !b) { consumed_ = false; both_ = false; }
  return ev;
}
}
