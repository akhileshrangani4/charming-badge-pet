#pragma once
#include "PetState.h"
#include "Clock.h"
namespace pet {
enum : uint32_t {
  EV_SICK = 1u << 0, EV_SICKER = 1u << 1, EV_RAN_AWAY = 1u << 2, EV_TANTRUM = 1u << 3,
  EV_ASLEEP = 1u << 4, EV_AWAKE = 1u << 5, EV_MESS = 1u << 6, EV_AUTO_UNPAUSE = 1u << 7,
  EV_GREW = 1u << 8, EV_WON_BACK = 1u << 9,
};
enum class Care : uint8_t { Meal, Snack, PlayWin, PlayLose, Clean, Medicine, Lights, Stroke, Shake };
bool isNight(int64_t m);
PetState newPet(int64_t nowMin);
uint32_t tickMinute(PetState& s);
uint32_t catchUp(PetState& s, int64_t nowMin);
// The clock was corrected (not time passing): move the pet to nowMin and shift every saved timestamp with it.
void resyncClock(PetState& s, int64_t nowMin);
uint32_t applyCare(PetState& s, Care c, int64_t nowMin);
bool callBack(PetState& s, int64_t nowMin);
bool togglePause(PetState& s, int64_t nowMin);
}
