#include "doctest.h"
#include "input/Gesture.h"
#include "input/Shake.h"
#include "input/ButtonArbiter.h"
using namespace input;

TEST_CASE("quick touch without movement is a tap") {
  GestureTracker g; g.begin(200, 200, 0); g.move(203, 202);
  CHECK(g.end(180) == Gesture::Tap);
}
TEST_CASE("slow press without movement is neither tap nor stroke") {
  GestureTracker g; g.begin(200, 200, 0);
  CHECK(g.end(900) == Gesture::None);
}
TEST_CASE("long drag across the screen is a stroke") {
  GestureTracker g; g.begin(120, 240, 0);
  for (int x = 130; x <= 300; x += 10) g.move(x, 240);
  CHECK(g.end(500) == Gesture::Stroke);
}
TEST_CASE("short fast flick is not a tap") {
  GestureTracker g; g.begin(200, 200, 0); g.move(240, 200); g.move(260, 200);
  CHECK(g.end(90) == Gesture::None);
}
TEST_CASE("back-and-forth rubbing counts as a stroke by path length") {
  GestureTracker g; g.begin(200, 240, 0);
  for (int i = 0; i < 6; i++) { g.move(240, 240); g.move(200, 240); }
  CHECK(g.end(700) == Gesture::Stroke);
}

TEST_CASE("three hard peaks within 800 ms is a shake") {
  ShakeDetector s;
  CHECK_FALSE(s.feed(2.5f, 0));  CHECK_FALSE(s.feed(1.0f, 50));
  CHECK_FALSE(s.feed(2.5f, 200)); CHECK_FALSE(s.feed(1.0f, 250));
  CHECK(s.feed(2.5f, 400));
}
TEST_CASE("peaks spread over seconds are not a shake") {
  ShakeDetector s;
  s.feed(2.5f, 0); s.feed(1.0f, 50); s.feed(2.5f, 900); s.feed(1.0f, 950);
  CHECK_FALSE(s.feed(2.5f, 1800));
}
TEST_CASE("holding one value high counts once") {
  ShakeDetector s;
  for (uint32_t t = 0; t < 600; t += 20) CHECK_FALSE(s.feed(2.5f, t));
}

static ButtonEvent run(ButtonArbiter& b, bool a, bool bb, uint32_t from, uint32_t to) {
  ButtonEvent last = ButtonEvent::None;
  for (uint32_t t = from; t <= to; t += 10) { auto e = b.update(a, bb, t); if (e != ButtonEvent::None) last = e; }
  return last;
}
TEST_CASE("short press of A is a click, on release") {
  ButtonArbiter b;
  CHECK(run(b, true, false, 0, 120) == ButtonEvent::None);
  CHECK(b.update(false, false, 130) == ButtonEvent::AClick);
}
TEST_CASE("holding B is BHold after the 1 s hold plus grace, and release does not also click") {
  ButtonArbiter b;
  CHECK(run(b, false, true, 0, 1700) == ButtonEvent::BHold);
  CHECK(b.update(false, false, 1710) == ButtonEvent::None);
}
TEST_CASE("holding both for 3 s is BothHold and never a single hold or click") {
  ButtonArbiter b;
  ButtonEvent seen = ButtonEvent::None;
  for (uint32_t t = 0; t <= 3100; t += 10) {
    auto e = b.update(true, true, t);
    if (e != ButtonEvent::None) { CHECK(e == ButtonEvent::BothHold); seen = e; }
  }
  CHECK(seen == ButtonEvent::BothHold);
  CHECK(b.update(false, false, 3110) == ButtonEvent::None);
}
TEST_CASE("pressing B shortly after A started still becomes a both-hold, not an A hold") {
  ButtonArbiter b;
  ButtonEvent seen = ButtonEvent::None;
  for (uint32_t t = 0; t <= 3400; t += 10) {
    auto e = b.update(true, t >= 300, t);
    if (e != ButtonEvent::None) seen = e;
    CHECK(e != ButtonEvent::AHold);
  }
  CHECK(seen == ButtonEvent::BothHold);
}
