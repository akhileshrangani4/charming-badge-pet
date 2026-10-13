#include "doctest.h"
#include "pet/PetRules.h"
using namespace pet;
static int64_t at(int h, int m = 0) { return localMinutes(2026, 10, 12, h, m); }
static PetState calmPet(int64_t now) { PetState s = newPet(now); s.discipline = 100; s.food = s.joy = s.hygiene = s.energy = 100; return s; }

TEST_CASE("a meal leaves a mess an hour later") {
  PetState s = calmPet(at(9));
  uint32_t ev = applyCare(s, Care::Meal, at(9));
  CHECK(s.food == doctest::Approx(100));
  ev = catchUp(s, at(9, 59));
  CHECK(s.messes == 0);
  ev = catchUp(s, at(10, 0));
  CHECK(s.messes == 1);
  CHECK((ev & EV_MESS) != 0);
}

TEST_CASE("any stat at zero for an hour makes it sick") {
  PetState s = calmPet(at(9));
  s.food = 0;
  uint32_t ev = catchUp(s, at(9, 59));
  CHECK(s.sick == 0);
  ev = catchUp(s, at(10, 0));
  CHECK(s.sick == 1);
  CHECK((ev & EV_SICK) != 0);
}

TEST_CASE("two uncleaned messes for an hour make it sick") {
  PetState s = calmPet(at(9));
  s.messes = 2;
  catchUp(s, at(10, 0));
  CHECK(s.sick == 1);
}

TEST_CASE("sickness worsens every 3 hours up to stage 3") {
  PetState s = calmPet(at(9));
  s.sick = 1;
  catchUp(s, at(12, 0));
  CHECK(s.sick == 2);
  s.sinceCareMin = 0;
  applyCare(s, Care::Stroke, at(12, 0));
  catchUp(s, at(15, 0));
  CHECK(s.sick == 3);
  applyCare(s, Care::Stroke, at(15, 0));
  catchUp(s, at(17, 59));
  CHECK(s.sick == 3);
}

TEST_CASE("three snacks within 30 minutes cause a bellyache") {
  PetState s = calmPet(at(9));
  applyCare(s, Care::Snack, at(9, 0));
  applyCare(s, Care::Snack, at(9, 10));
  CHECK(s.sick == 0);
  uint32_t ev = applyCare(s, Care::Snack, at(9, 20));
  CHECK(s.sick == 1);
  CHECK((ev & EV_SICK) != 0);
}

TEST_CASE("snacks spread out are fine") {
  PetState s = calmPet(at(9));
  applyCare(s, Care::Snack, at(9, 0));
  applyCare(s, Care::Snack, at(9, 20));
  applyCare(s, Care::Snack, at(9, 40));
  CHECK(s.sick == 0);
}

TEST_CASE("medicine lowers sickness one stage; needless medicine costs joy") {
  PetState s = calmPet(at(9));
  s.sick = 2;
  applyCare(s, Care::Medicine, at(9));
  CHECK(s.sick == 1);
  applyCare(s, Care::Medicine, at(9));
  CHECK(s.sick == 0);
  const float joy = s.joy;
  applyCare(s, Care::Medicine, at(9));
  CHECK(s.joy == doctest::Approx(joy - 5));
}

TEST_CASE("clean removes messes and restores hygiene") {
  PetState s = calmPet(at(9));
  s.messes = 3; s.hygiene = 10;
  applyCare(s, Care::Clean, at(9));
  CHECK(s.messes == 0);
  CHECK(s.hygiene == doctest::Approx(100));
}

TEST_CASE("sick pets get hungry twice as fast") {
  PetState s = calmPet(at(9));
  s.sick = 1;
  catchUp(s, at(10));
  CHECK(s.food == doctest::Approx(100 - 2 * 100.0 / 4).epsilon(0.02));
}

TEST_CASE("stroke has a cooldown") {
  PetState s = calmPet(at(9));
  s.joy = 50;
  applyCare(s, Care::Stroke, at(9, 0));
  applyCare(s, Care::Stroke, at(9, 1));
  CHECK(s.joy == doctest::Approx(55).epsilon(0.01));
  applyCare(s, Care::Stroke, at(9, 2));
  CHECK(s.joy > 58);
}
