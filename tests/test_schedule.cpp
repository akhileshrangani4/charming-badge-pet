#include "doctest.h"
#include "pet/PetRules.h"
using namespace pet;
static int64_t at(int day, int h, int m = 0) { return localMinutes(2026, 10, day, h, m); }
static PetState calmPet(int64_t now) { PetState s = newPet(now); s.discipline = 100; s.food = s.joy = s.hygiene = s.energy = 100; return s; }

TEST_CASE("falls asleep at 23:00 and wakes at 07:00") {
  PetState s = calmPet(at(12, 22));
  uint32_t ev = catchUp(s, at(12, 23, 0));
  CHECK(s.asleep);
  CHECK((ev & EV_ASLEEP) != 0);
  ev = catchUp(s, at(13, 7, 0));
  CHECK_FALSE(s.asleep);
  CHECK((ev & EV_AWAKE) != 0);
}

TEST_CASE("waking it at night with lights costs joy and makes it grumpy") {
  PetState s = calmPet(at(12, 22));
  catchUp(s, at(13, 1));
  s.joy = 80;
  applyCare(s, Care::Lights, at(13, 1));
  CHECK_FALSE(s.asleep);
  CHECK(s.joy == doctest::Approx(80 - 10 + 0).epsilon(0.05));
  CHECK(s.wokeThisNight);
}

TEST_CASE("lights in the day puts it to sleep; it wakes when energy is full") {
  PetState s = calmPet(at(12, 13)); s.energy = 90;
  applyCare(s, Care::Lights, at(12, 13));
  CHECK(s.asleep);
  catchUp(s, at(12, 14));
  CHECK_FALSE(s.asleep);
}

TEST_CASE("care while asleep does nothing except lights and shake") {
  PetState s = calmPet(at(12, 13)); s.asleep = true; s.food = 50;
  applyCare(s, Care::Meal, at(12, 13));
  CHECK(s.food == doctest::Approx(50).epsilon(0.02));
  applyCare(s, Care::Shake, at(12, 13));
  CHECK_FALSE(s.asleep);
}

TEST_CASE("play: win gives more joy than a loss, costs energy; refused while returning") {
  PetState s = calmPet(at(12, 9)); s.joy = 40; s.energy = 80;
  applyCare(s, Care::PlayWin, at(12, 9));
  CHECK(s.joy == doctest::Approx(65));
  CHECK(s.energy == doctest::Approx(70));
  applyCare(s, Care::PlayLose, at(12, 9));
  CHECK(s.joy == doctest::Approx(75));
  s.life = Life::Returning; s.joy = 40;
  applyCare(s, Care::PlayWin, at(12, 9));
  CHECK(s.joy == doctest::Approx(40));
}

TEST_CASE("tantrums happen roughly every 2 hours for an average pet") {
  PetState s = calmPet(at(12, 8)); s.discipline = 0; s.spoil = 0; s.rng = 12345;
  int tantrums = 0;
  for (int i = 0; i < 14 * 60; i++) {
    s.food = s.joy = s.hygiene = s.energy = 100;
    if (tickMinute(s) & EV_TANTRUM) tantrums++;
    if (s.lastMin % 1440 >= 22 * 60) s.lastMin += 9 * 60;   // skip nights
    s.asleep = false;
  }
  CHECK(tantrums >= 3);
  CHECK(tantrums <= 14);
}

TEST_CASE("answering a tantrum spoils it; ignoring then caring disciplines it") {
  PetState s = calmPet(at(12, 9)); s.discipline = 50; s.spoil = 0;
  s.tantrum = true;
  applyCare(s, Care::Stroke, at(12, 9));
  CHECK_FALSE(s.tantrum);
  CHECK(s.spoil == doctest::Approx(10));
  CHECK(s.discipline == doctest::Approx(45));
  s.tantrum = true; s.tantrumMin = 0;
  s.food = 29;                // below the tantrum threshold, so no new tantrum can start mid-test
  catchUp(s, at(12, 9, 2));
  CHECK_FALSE(s.tantrum);
  CHECK(s.calmWindowMin > 0);
  applyCare(s, Care::Stroke, at(12, 9, 3));
  CHECK(s.discipline == doctest::Approx(55));
}

TEST_CASE("pause freezes needs and is capped at 10 hours") {
  PetState s = calmPet(at(12, 9));
  REQUIRE(togglePause(s, at(12, 9)));
  catchUp(s, at(12, 18, 59));
  CHECK(s.paused);
  CHECK(s.food == doctest::Approx(100));
  uint32_t ev = catchUp(s, at(12, 19, 1));
  CHECK_FALSE(s.paused);
  CHECK((ev & EV_AUTO_UNPAUSE) != 0);
  CHECK_FALSE(togglePause(s, at(12, 19, 1)));   // budget empty
}

TEST_CASE("growth unlocks at day 3 with good trust") {
  PetState s = calmPet(at(1, 9)); s.trust = 80;
  uint32_t ev = 0;
  for (int d = 0; d < 3; d++) {
    for (int i = 0; i < 1440; i++) {
      s.food = s.joy = s.hygiene = s.energy = 100; s.trust = 80; s.tantrum = false;
      ev |= tickMinute(s);
    }
  }
  CHECK(s.growth == 1);
  CHECK((ev & EV_GREW) != 0);
}
