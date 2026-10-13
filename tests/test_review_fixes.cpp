#include "doctest.h"
#include "pet/PetRules.h"
#include "pet/Mood.h"
#include "input/Gesture.h"
using namespace pet;
static int64_t at(int day, int h, int m = 0) { return localMinutes(2026, 10, day, h, m); }
static PetState calmPet(int64_t now) { PetState s = newPet(now); s.discipline = 100; s.food = s.joy = s.hygiene = s.energy = 90; return s; }

TEST_CASE("clock jumping back 7 h still lets strokes count") {
  PetState s = calmPet(at(13, 1));
  applyCare(s, Care::Stroke, at(13, 1));
  catchUp(s, at(12, 18));                 // RTC corrected from UTC to local
  s.joy = 50;
  applyCare(s, Care::Stroke, at(12, 18, 5));
  CHECK(s.joy > 52);                      // ~48.3 after 5 min of decay without the stroke, ~53.3 with it
}

TEST_CASE("clock jumping back does not turn one snack into a bellyache") {
  PetState s = calmPet(at(13, 0));
  applyCare(s, Care::Snack, at(13, 0, 0));
  applyCare(s, Care::Snack, at(13, 0, 10));   // these land at 17:00 and 17:10 after the 7 h correction
  catchUp(s, at(13, 1));
  catchUp(s, at(12, 18));
  applyCare(s, Care::Snack, at(12, 18));      // an hour later: not a bellyache
  CHECK(s.sick == 0);
}

TEST_CASE("clock jumping back does not block win-back sessions") {
  PetState s = calmPet(at(13, 1)); s.life = Life::Gone;
  REQUIRE(callBack(s, at(13, 1)));
  applyCare(s, Care::Stroke, at(13, 1));
  catchUp(s, at(12, 18));
  applyCare(s, Care::Meal, at(12, 19));
  applyCare(s, Care::Clean, at(12, 20));
  CHECK(s.life == Life::Normal);
}

TEST_CASE("resyncClock shifts every saved timestamp by the same amount") {
  PetState s = calmPet(at(13, 1));
  s.lastStrokeMin = at(13, 0); s.lastWinbackMin = at(12, 23); s.snackMin[2] = at(13, 0, 30);
  resyncClock(s, at(12, 18));
  CHECK(s.lastMin == at(12, 18));
  CHECK(s.lastStrokeMin == at(12, 17));
  CHECK(s.lastWinbackMin == at(12, 16));
  CHECK(s.snackMin[2] == at(12, 17, 30));
}

TEST_CASE("low stats alone do not need attention; sickness and tantrums do") {
  PetState s = calmPet(at(12, 12));
  s.joy = 10; s.food = 10;
  CHECK_FALSE(needsAttention(s));
  s.sick = 1; CHECK(needsAttention(s));
  s.sick = 0; s.tantrum = true; CHECK(needsAttention(s));
}

TEST_CASE("at night only an imminent runaway needs attention") {
  PetState s = calmPet(at(13, 2));
  s.sick = 1; s.sinceCareMin = 60;
  CHECK_FALSE(needsAttention(s));
  s.sinceCareMin = 8 * 60 - 90;           // under two hours from leaving
  CHECK(needsAttention(s));
}

TEST_CASE("a long still press with touch jitter is not a stroke") {
  input::GestureTracker g; g.begin(200, 200, 0);
  for (int i = 0; i < 180; i++) g.move(200 + (i % 2), 200 + ((i / 2) % 2));
  CHECK(g.end(3000) == input::Gesture::None);
}
