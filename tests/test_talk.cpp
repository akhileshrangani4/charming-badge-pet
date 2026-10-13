#include "doctest.h"
#include "input/ButtonArbiter.h"
using namespace input;

static ButtonEvent last(ButtonArbiter& b, bool a, bool bb, uint32_t from, uint32_t to) {
  ButtonEvent e = ButtonEvent::None;
  for (uint32_t t = from; t <= to; t += 10) { auto x = b.update(a, bb, t); if (x != ButtonEvent::None) e = x; }
  return e;
}

TEST_CASE("holding A 400 ms starts talking; release ends it; no click or hold") {
  ButtonArbiter b; b.setTalkEnabled(true);
  CHECK(last(b, true, false, 0, 390) == ButtonEvent::None);
  CHECK(last(b, true, false, 400, 2500) == ButtonEvent::TalkStart);   // no AHold while talking
  CHECK(b.update(false, false, 2510) == ButtonEvent::TalkEnd);
}
TEST_CASE("a quick tap of A is still a click when talking is enabled") {
  ButtonArbiter b; b.setTalkEnabled(true);
  CHECK(last(b, true, false, 0, 150) == ButtonEvent::None);
  CHECK(b.update(false, false, 160) == ButtonEvent::AClick);
}
TEST_CASE("pressing B while talking cancels the talk") {
  ButtonArbiter b; b.setTalkEnabled(true);
  CHECK(last(b, true, false, 0, 600) == ButtonEvent::TalkStart);
  CHECK(b.update(true, true, 610) == ButtonEvent::TalkCancel);
}
TEST_CASE("a talk is sent automatically at 30 s") {
  ButtonArbiter b; b.setTalkEnabled(true);
  CHECK(last(b, true, false, 0, 1000) == ButtonEvent::TalkStart);
  CHECK(last(b, true, false, 1010, 30500) == ButtonEvent::TalkEnd);
  CHECK(b.update(false, false, 30510) == ButtonEvent::None);   // the late release does nothing
}
TEST_CASE("with talking disabled, holding A is the old AHold (call back from Gone)") {
  ButtonArbiter b;
  CHECK(last(b, true, false, 0, 1700) == ButtonEvent::AHold);
}
TEST_CASE("after B cancels a talk, keeping both held still pauses (BothHold)") {
  ButtonArbiter b; b.setTalkEnabled(true);
  CHECK(last(b, true, false, 0, 600) == ButtonEvent::TalkStart);
  CHECK(b.update(true, true, 610) == ButtonEvent::TalkCancel);
  CHECK(last(b, true, true, 620, 3700) == ButtonEvent::BothHold);
}
