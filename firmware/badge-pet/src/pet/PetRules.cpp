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

static bool anyZero(const PetState& s) { return s.food <= 0 || s.energy <= 0 || s.joy <= 0 || s.hygiene <= 0; }
static bool allZero(const PetState& s) { return s.food <= 0 && s.energy <= 0 && s.joy <= 0 && s.hygiene <= 0; }

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
  for (auto& d : s.messDue) {
    if (d > 0 && --d == 0) {
      d = -1;
      s.messes = (uint8_t)std::min(s.messes + 1, 9);
      s.hygiene -= MESS_HYGIENE_HIT;
      ev |= EV_MESS;
    }
  }
  clampAll(s);
  s.zeroMin    = anyZero(s) ? s.zeroMin + 1 : 0;
  s.allZeroMin = allZero(s) ? s.allZeroMin + 1 : 0;
  s.messMin    = s.messes >= 2 ? s.messMin + 1 : 0;
  s.sinceCareMin++;
  if (s.zeroMin > 0 && s.zeroMin % 60 == 0) s.trust = clamp100(s.trust - 1);
  if (!s.sick) {
    if (s.zeroMin >= SICK_ZERO_MIN || s.messMin >= SICK_MESS_MIN) { s.sick = 1; s.sickMin = 0; ev |= EV_SICK; }
  } else if (++s.sickMin >= SICK_STAGE_UP_MIN && s.sick < MAX_SICK) {
    s.sick++; s.sickMin = 0; ev |= EV_SICKER;
  }
  return ev;
}

uint32_t catchUp(PetState& s, int64_t nowMin) {
  uint32_t ev = 0;
  while (s.lastMin < nowMin) ev |= tickMinute(s);
  return ev;
}

uint32_t applyCare(PetState& s, Care c, int64_t nowMin) {
  uint32_t ev = catchUp(s, nowMin);
  if (s.life == Life::Gone || s.paused) return ev;
  if (s.asleep && c != Care::Lights && c != Care::Shake) return ev;
  const bool isCare = c != Care::Shake;
  const bool need = s.food < NEED_THRESHOLD || s.joy < NEED_THRESHOLD || s.hygiene < NEED_THRESHOLD ||
                    s.energy < NEED_THRESHOLD || s.sick || s.messes > 0;
  switch (c) {
    case Care::Meal:
      s.food += MEAL_FOOD;
      for (auto& d : s.messDue) if (d < 0) { d = MESS_AFTER_MEAL_MIN; break; }
      break;
    case Care::Snack:
      s.food += SNACK_FOOD; s.joy += SNACK_JOY;
      s.snackMin[0] = s.snackMin[1]; s.snackMin[1] = s.snackMin[2]; s.snackMin[2] = nowMin;
      if (!s.sick && nowMin - s.snackMin[0] < BELLYACHE_WINDOW_MIN) { s.sick = 1; s.sickMin = 0; ev |= EV_SICK; }
      break;
    case Care::Clean:
      s.messes = 0; s.hygiene = 100; s.messMin = 0;
      break;
    case Care::Medicine:
      if (s.sick) { s.sick--; s.sickMin = 0; s.zeroMin = 0; s.messMin = 0; }
      else s.joy -= NEEDLESS_MED_JOY;
      break;
    case Care::Stroke:
      if (nowMin - s.lastStrokeMin >= STROKE_COOLDOWN_MIN) { s.joy += STROKE_JOY; s.lastStrokeMin = nowMin; }
      break;
    case Care::Shake:
      s.joy -= SHAKE_JOY; s.trust -= SHAKE_TRUST;
      break;
    default:
      break;
  }
  if (isCare) {
    s.sinceCareMin = 0;
    s.trust += need ? TRUST_NEED_GAIN : TRUST_CARE_GAIN;
  }
  clampAll(s);
  return ev;
}
bool callBack(PetState&, int64_t) { return false; }
bool togglePause(PetState&, int64_t) { return false; }
}
