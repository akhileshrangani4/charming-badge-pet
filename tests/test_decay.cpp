#include "doctest.h"
#include "pet/PetRules.h"
#include "pet/Clock.h"
using namespace pet;

static int64_t at(int h, int m = 0, int day = 12) { return localMinutes(2026, 10, day, h, m); }

TEST_CASE("localMinutes is monotonic and day-aligned") {
  CHECK(localMinutes(2000, 1, 1, 0, 0) == 0);
  CHECK(localMinutes(2000, 1, 2, 0, 0) == 1440);
  CHECK(localMinutes(2026, 3, 1, 0, 0) - localMinutes(2026, 2, 28, 0, 0) == 1440);
  CHECK(hourOf(at(23, 59)) == 23);
}

TEST_CASE("night is 23:00 to 07:00") {
  CHECK(isNight(at(23, 0)));
  CHECK(isNight(at(3, 0)));
  CHECK_FALSE(isNight(at(7, 0)));
  CHECK_FALSE(isNight(at(22, 59)));
}

TEST_CASE("food empties in about 4 hours while awake") {
  PetState s = newPet(at(9));
  s.food = 100; s.joy = 100; s.hygiene = 100; s.energy = 100; s.discipline = 100;
  catchUp(s, at(11));
  CHECK(s.food == doctest::Approx(50).epsilon(0.02));
  catchUp(s, at(13));
  CHECK(s.food < 0.01f);
}

TEST_CASE("energy drains awake and refills asleep") {
  PetState s = newPet(at(9));
  s.energy = 100;
  catchUp(s, at(12));
  CHECK(s.energy == doctest::Approx(50).epsilon(0.02));
  s.asleep = true;
  catchUp(s, at(13));
  CHECK(s.energy == doctest::Approx(75).epsilon(0.02));
}

TEST_CASE("sleep slows hunger to a quarter") {
  PetState s = newPet(at(9));
  s.food = 100; s.asleep = true; s.energy = 40;
  catchUp(s, at(11));
  CHECK(s.food == doctest::Approx(100 - 50 * 0.25).epsilon(0.02));
}

TEST_CASE("stats stay within 0..100") {
  PetState s = newPet(at(9));
  catchUp(s, at(9) + 3000);
  CHECK(s.food >= 0); CHECK(s.joy >= 0); CHECK(s.hygiene >= 0); CHECK(s.energy >= 0);
  CHECK(s.energy <= 100);
}
