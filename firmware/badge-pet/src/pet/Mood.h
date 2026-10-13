#pragma once
#include "PetState.h"
namespace pet {
enum class Mood : uint8_t { Gone, Paused, Sleeping, Sick, Tantrum, Hungry, Dirty, Sleepy, Grumpy, Sad, Happy, Content };
Mood moodOf(const PetState& s);
bool needsAttention(const PetState& s);
const char* phraseFor(Mood m);
}
