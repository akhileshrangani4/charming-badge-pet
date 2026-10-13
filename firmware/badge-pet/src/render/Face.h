#pragma once
#include <M5Unified.h>
namespace render {
enum class Expression : uint8_t { Idle, Happy, Hungry, Sick, Sleep, Sleepy, Grumpy, Tantrum, Dizzy, Sad };
// (x, y) is the face anchor between the eyes; s is the mascot scale.
void drawFace(M5Canvas& c, Expression e, float x, float y, float s, uint32_t ms, bool blink, float rot = 0);
}
