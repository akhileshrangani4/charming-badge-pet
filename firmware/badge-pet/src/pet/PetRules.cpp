#include "PetRules.h"
#include "PetConfig.h"
#include <algorithm>
namespace pet {
using namespace cfg;

static float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
static void clampAll(PetState& s) {
  s.food = clamp100(s.food); s.energy = clamp100(s.energy); s.joy = clamp100(s.joy);
  s.hygiene = clamp100(s.hygiene); s.trust = clamp100(s.trust);
  s.discipline = clamp100(s.discipline); s.spoil = clamp100(s.spoil);
}

bool isNight(int64_t m) { const int h = hourOf(m); return h >= NIGHT_START_H || h < NIGHT_END_H; }

PetState newPet(int64_t nowMin) {
  PetState s;
  s.lastMin = nowMin;
  s.rng = (uint32_t)(nowMin * 2654435761u) | 1u;
  return s;
}

uint32_t tickMinute(PetState& s) {
  uint32_t ev = 0;
  s.lastMin++;
  const float f = s.asleep ? SLEEP_DECAY : 1.0f;
  const float k = s.sick ? SICK_DECAY : 1.0f;
  s.food    -= 100.0f / FOOD_EMPTY_MIN * f * k;
  s.joy     -= 100.0f / JOY_EMPTY_MIN * f * k;
  s.hygiene -= 100.0f / HYGIENE_EMPTY_MIN * f * k;
  if (s.asleep) s.energy += 100.0f / ENERGY_REFILL_MIN;
  else          s.energy -= 100.0f / ENERGY_EMPTY_MIN;
  clampAll(s);
  return ev;
}

uint32_t catchUp(PetState& s, int64_t nowMin) {
  uint32_t ev = 0;
  while (s.lastMin < nowMin) ev |= tickMinute(s);
  return ev;
}

uint32_t applyCare(PetState& s, Care, int64_t nowMin) { return catchUp(s, nowMin); }
bool callBack(PetState&, int64_t) { return false; }
bool togglePause(PetState&, int64_t) { return false; }
}
