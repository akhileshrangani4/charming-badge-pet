#pragma once
#include <M5Unified.h>
namespace render {
enum class Expression : uint8_t { Idle, Happy, Hungry, Sick, Sleep, Sleepy, Grumpy, Tantrum, Dizzy, Sad, Listening, Thinking, Speaking };
// (x, y) is the face anchor between the eyes; s is the mascot scale.
// mouth: 0..1 loudness of the reply audio, used by Speaking.
void drawFace(M5Canvas& c, Expression e, float x, float y, float s, uint32_t ms, bool blink, float rot = 0, float mouth = 0);
}
