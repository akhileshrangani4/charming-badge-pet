#pragma once
#include <stdint.h>
namespace input {
enum class ButtonEvent : uint8_t { None, AClick, BClick, AHold, BHold, BothHold, TalkStart, TalkEnd, TalkCancel };
constexpr uint32_t HOLD_MS = 1000, BOTH_HOLD_MS = 3000, BOTH_GRACE_MS = 600;
constexpr uint32_t TALK_HOLD_MS = 400, TALK_MAX_MS = 30000;   // push-to-talk: hold A this long; a turn is sent at the cap
class ButtonArbiter {
 public:
  explicit ButtonArbiter(bool waitForRelease = false) : waitRelease_(waitForRelease) {}
  ButtonEvent update(bool a, bool b, uint32_t ms);
  void setTalkEnabled(bool on) { talkEnabled_ = on; }
 private:
  bool aDown_ = false, bDown_ = false, consumed_ = false, both_ = false;
  uint32_t aT_ = 0, bT_ = 0, bothT_ = 0;
  bool waitRelease_ = false;
  bool talkEnabled_ = false, talking_ = false;
  uint32_t talkT_ = 0;
};
}
