#include "doctest.h"
#include "pet/PetRules.h"
using namespace pet;
static int64_t at(int day, int h, int m = 0) { return localMinutes(2026, 10, day, h, m); }

TEST_CASE("sick and uncared for 8 hours: it runs away") {
  PetState s = newPet(at(12, 8)); s.discipline = 100;
  s.food = s.joy = s.hygiene = s.energy = 100; s.sick = 1;
  uint32_t ev = catchUp(s, at(12, 15, 59));
  CHECK(s.life == Life::Normal);
  ev = catchUp(s, at(12, 16, 0));
  CHECK(s.life == Life::Gone);
  CHECK((ev & EV_RAN_AWAY) != 0);
  CHECK(s.trust <= 20);
}

TEST_CASE("every stat at zero for 4 hours: it runs away even if not sick yet") {
  PetState s = newPet(at(12, 8));
  s.food = s.joy = s.hygiene = s.energy = 0;
  s.sinceCareMin = -100000;   // keep the sick+no-care rule from firing first
  catchUp(s, at(12, 12, 0));
  CHECK(s.life == Life::Gone);
}

TEST_CASE("care actions do nothing while it is gone") {
  PetState s = newPet(at(12, 8)); s.life = Life::Gone;
  const float food = s.food;
  applyCare(s, Care::Meal, at(12, 9));
  CHECK(s.food == doctest::Approx(food));
}

TEST_CASE("call back starts a 3-session win-back after a short absence") {
  PetState s = newPet(at(12, 8)); s.life = Life::Gone;
  catchUp(s, at(12, 20));
  REQUIRE(callBack(s, at(12, 20)));
  CHECK(s.life == Life::Returning);
  CHECK(s.winbackNeeded == 3);
  applyCare(s, Care::Stroke, at(12, 20, 0));
  applyCare(s, Care::Meal,   at(12, 20, 30));   // within an hour: does not count
  CHECK(s.winbackDone == 1);
  applyCare(s, Care::Meal,   at(12, 21, 0));
  CHECK(s.winbackDone == 2);
  uint32_t ev = applyCare(s, Care::Clean, at(12, 22, 0));
  CHECK(s.life == Life::Normal);
  CHECK((ev & EV_WON_BACK) != 0);
  CHECK(s.trust >= 35);
}

TEST_CASE("gone 48 hours or more needs 5 sessions") {
  PetState s = newPet(at(12, 8)); s.life = Life::Gone;
  catchUp(s, at(14, 8));
  REQUIRE(callBack(s, at(14, 8)));
  CHECK(s.winbackNeeded == 5);
}

TEST_CASE("call back fails when it is not gone") {
  PetState s = newPet(at(12, 8));
  CHECK_FALSE(callBack(s, at(12, 9)));
}

TEST_CASE("weeks off: bounded catch-up, ends gone, still recoverable") {
  PetState s = newPet(at(1, 9));
  s.lastMin = localMinutes(2026, 8, 1, 9, 0);
  catchUp(s, at(12, 9));             // about 72 days later; capped to 30 days of simulation
  CHECK(s.lastMin == at(12, 9));
  CHECK(s.life == Life::Gone);
  REQUIRE(callBack(s, at(12, 9)));
  CHECK(s.winbackNeeded == 5);
}
