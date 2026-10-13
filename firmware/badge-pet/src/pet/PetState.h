#pragma once
#include <stdint.h>
namespace pet {
enum class Life : uint8_t { Normal, Gone, Returning };
struct PetState {
  int64_t lastMin = 0;            // last simulated local minute
  float food = 80, energy = 80, joy = 80, hygiene = 100;
  float trust = 50, discipline = 50, spoil = 0;
  float pauseBudget = 600;
  int64_t lastWinbackMin = -100000, lastStrokeMin = -100000;
  int64_t snackMin[3] = {-100000, -100000, -100000};
  int32_t zeroMin = 0, allZeroMin = 0, messMin = 0, sickMin = 0, sinceCareMin = 0;
  int32_t tantrumMin = 0, calmWindowMin = 0, goneMin = 0;
  int32_t messDue[4] = {-1, -1, -1, -1};
  uint32_t ageMin = 0;
  uint32_t rng = 0x9E3779B9u;
  Life life = Life::Normal;
  uint8_t sick = 0, messes = 0, growth = 0, winbackNeeded = 0, winbackDone = 0;
  bool asleep = false, paused = false, tantrum = false, wokeThisNight = false;
};
}
