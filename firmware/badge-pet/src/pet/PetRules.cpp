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

static float rand01(PetState& s) {
  uint32_t x = s.rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s.rng = x;
  return (x >> 8) * (1.0f / 16777216.0f);
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
  if (s.life == Life::Gone) { s.goneMin++; return ev; }
  const int64_t m = s.lastMin;
  if (s.paused) {
    s.pauseBudget -= 1;
    if (s.pauseBudget > 0) return ev;
    s.pauseBudget = 0; s.paused = false; ev |= EV_AUTO_UNPAUSE;
  } else {
    s.pauseBudget = std::min(PAUSE_BUDGET_MIN, s.pauseBudget + PAUSE_REFILL_PER_MIN);
  }
  const bool night = isNight(m), wasNight = isNight(m - 1);
  if (night && !wasNight && !s.asleep) { s.asleep = true; ev |= EV_ASLEEP; }
  if (!night && wasNight) { if (s.asleep) ev |= EV_AWAKE; s.asleep = false; s.wokeThisNight = false; }
  if (s.asleep && !night && s.energy >= 100) { s.asleep = false; ev |= EV_AWAKE; }
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
  if ((s.sick && s.sinceCareMin >= RUNAWAY_SICK_NOCARE_MIN) || s.allZeroMin >= RUNAWAY_ALLZERO_MIN) {
    s.life = Life::Gone; s.goneMin = 0; s.tantrum = false; s.asleep = false; s.paused = false;
    s.trust = std::min(s.trust, RETURN_TRUST_CAP);
    return ev | EV_RAN_AWAY;
  }
  if (s.tantrum) {
    if (++s.tantrumMin >= TANTRUM_IGNORE_MIN) { s.tantrum = false; s.calmWindowMin = CALM_WINDOW_MIN; }
  } else {
    if (s.calmWindowMin > 0) s.calmWindowMin--;
    const bool fine = s.food > TANTRUM_OK_STAT && s.joy > TANTRUM_OK_STAT &&
                      s.hygiene > TANTRUM_OK_STAT && s.energy > TANTRUM_OK_STAT;
    if (!s.asleep && !s.sick && s.life == Life::Normal && fine) {
      const float p = TANTRUM_PER_MIN * (1 + s.spoil / 50.0f) * std::max(0.1f, 1 - s.discipline / 150.0f);
      if (rand01(s) < p) { s.tantrum = true; s.tantrumMin = 0; ev |= EV_TANTRUM; }
    }
  }
  s.ageMin++;
  if (s.growth < 3 && s.ageMin >= (uint32_t)GROWTH_DAYS[s.growth] * 1440u && s.trust >= GROWTH_TRUST) {
    s.growth++; ev |= EV_GREW;
  }
  return ev;
}

void resyncClock(PetState& s, int64_t nowMin) {
  const int64_t d = nowMin - s.lastMin;
  s.lastMin = nowMin;
  s.lastStrokeMin += d; s.lastWinbackMin += d;
  for (auto& t : s.snackMin) t += d;
}

uint32_t catchUp(PetState& s, int64_t nowMin) {
  if (nowMin < s.lastMin) { resyncClock(s, nowMin); return 0; }   // clock went backwards: resync, simulate nothing
  if (nowMin - s.lastMin > MAX_CATCHUP_MIN) s.lastMin = nowMin - MAX_CATCHUP_MIN;
  uint32_t ev = 0;
  while (s.lastMin < nowMin) ev |= tickMinute(s);
  return ev;
}

uint32_t applyCare(PetState& s, Care c, int64_t nowMin) {
  uint32_t ev = catchUp(s, nowMin);
  if (s.life == Life::Gone || s.paused) return ev;
  if (s.asleep && c != Care::Lights && c != Care::Shake) return ev;
  const bool isCare = c != Care::Shake;
  if (isCare) {
    if (s.tantrum) { s.tantrum = false; s.spoil += SPOIL_STEP; s.discipline -= DISCIPLINE_LOSS; }
    else if (s.calmWindowMin > 0) { s.discipline += DISCIPLINE_GAIN; s.spoil -= SPOIL_STEP / 2; s.calmWindowMin = 0; }
  }
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
    case Care::PlayWin:
    case Care::PlayLose:
      if (s.life != Life::Returning) {
        s.joy += c == Care::PlayWin ? PLAY_WIN_JOY : PLAY_LOSE_JOY;
        s.energy -= PLAY_ENERGY;
      }
      break;
    case Care::Lights:
      if (s.asleep) {
        s.asleep = false; ev |= EV_AWAKE;
        if (isNight(nowMin)) { s.joy -= NIGHT_WAKE_JOY; s.wokeThisNight = true; }
      } else { s.asleep = true; ev |= EV_ASLEEP; }
      break;
    case Care::Shake:
      s.joy -= SHAKE_JOY; s.trust -= SHAKE_TRUST;
      if (s.asleep) {
        s.asleep = false; ev |= EV_AWAKE;
        if (isNight(nowMin)) { s.joy -= NIGHT_WAKE_JOY; s.wokeThisNight = true; }
      }
      break;
    default:
      break;
  }
  if (isCare) {
    s.sinceCareMin = 0;
    s.trust += need ? TRUST_NEED_GAIN : TRUST_CARE_GAIN;
    if (s.life == Life::Returning && nowMin - s.lastWinbackMin >= WINBACK_GAP_MIN) {
      s.lastWinbackMin = nowMin;
      if (++s.winbackDone >= s.winbackNeeded) {
        s.life = Life::Normal;
        s.trust = std::max(s.trust, WON_BACK_TRUST);
        ev |= EV_WON_BACK;
      }
    }
  }
  clampAll(s);
  return ev;
}
bool callBack(PetState& s, int64_t nowMin) {
  catchUp(s, nowMin);
  if (s.life != Life::Gone) return false;
  s.life = Life::Returning;
  s.winbackNeeded = (uint8_t)(s.goneMin >= WINBACK_LONG_GONE_MIN ? WINBACK_SESSIONS_LONG : WINBACK_SESSIONS);
  s.winbackDone = 0;
  s.lastWinbackMin = nowMin - WINBACK_GAP_MIN;
  s.food = s.energy = s.joy = s.hygiene = RETURN_STAT;
  s.sick = 0; s.sickMin = 0; s.messes = 0;
  for (auto& d : s.messDue) d = -1;
  s.zeroMin = s.allZeroMin = s.messMin = 0;
  s.sinceCareMin = 0; s.tantrum = false; s.calmWindowMin = 0; s.goneMin = 0;
  return true;
}
bool togglePause(PetState& s, int64_t nowMin) {
  catchUp(s, nowMin);
  if (s.paused) { s.paused = false; return false; }
  if (s.life == Life::Gone || s.pauseBudget < 1) return false;
  s.paused = true;
  return true;
}
}
