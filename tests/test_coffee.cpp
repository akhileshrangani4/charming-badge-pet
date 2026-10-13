#include "doctest.h"
#include "pet/PetRules.h"
#include "pet/SaveCodec.h"
#include <string.h>
#include <stddef.h>
using namespace pet;
static int64_t at(int h, int m = 0) { return localMinutes(2026, 10, 12, h, m); }

TEST_CASE("coffee refills energy and wakes it up") {
  PetState s = newPet(at(14)); s.energy = 5; s.asleep = true;
  REQUIRE(coffeeReady(s, at(14)));
  uint32_t ev = applyCare(s, Care::Coffee, at(14));
  CHECK(s.energy == doctest::Approx(100));
  CHECK_FALSE(s.asleep);
  CHECK((ev & EV_AWAKE) != 0);
}

TEST_CASE("coffee works once every 3 hours") {
  PetState s = newPet(at(9)); s.energy = 10;
  applyCare(s, Care::Coffee, at(9));
  s.energy = 10;
  CHECK_FALSE(coffeeReady(s, at(11, 59)));
  applyCare(s, Care::Coffee, at(11, 59));
  CHECK(s.energy < 20);
  CHECK(coffeeReady(s, at(12, 0)));
}

TEST_CASE("a version 1 save (before coffee) still loads") {
  PetState s = newPet(at(9)); s.food = 33; s.trust = 77;
  const size_t oldSize = offsetof(PetState, lastCoffeeMin);
  uint8_t buf[SAVE_SIZE];
  const uint32_t magic = SAVE_MAGIC; const uint16_t ver = 1, size = (uint16_t)oldSize;
  memcpy(buf, &magic, 4); memcpy(buf + 4, &ver, 2); memcpy(buf + 6, &size, 2);
  memcpy(buf + 8, &s, oldSize);
  uint32_t c = 0xFFFFFFFFu;   // same CRC-32 as the codec
  for (size_t i = 0; i < 8 + oldSize; i++) { c ^= buf[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  c = ~c; memcpy(buf + 8 + oldSize, &c, 4);
  PetState out;
  REQUIRE(decodeSave(buf, 8 + oldSize + 4, out));
  CHECK(out.food == doctest::Approx(33));
  CHECK(out.trust == doctest::Approx(77));
  CHECK(coffeeReady(out, at(9)));
}
