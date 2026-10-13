#include "doctest.h"
#include "pet/PetRules.h"
#include "pet/Mood.h"
#include "pet/SaveCodec.h"
#include <string.h>
using namespace pet;
static int64_t at(int h) { return localMinutes(2026, 10, 12, h, 0); }

TEST_CASE("mood priority: gone > paused > sleeping > sick > tantrum > hungry") {
  PetState s = newPet(at(9));
  s.food = 10; s.tantrum = true; s.sick = 1;
  CHECK(moodOf(s) == Mood::Sick);
  s.asleep = true;   CHECK(moodOf(s) == Mood::Sleeping);
  s.paused = true;   CHECK(moodOf(s) == Mood::Paused);
  s.life = Life::Gone; CHECK(moodOf(s) == Mood::Gone);
}

TEST_CASE("content, happy, grumpy") {
  PetState s = newPet(at(9));
  s.food = s.energy = s.hygiene = 90; s.joy = 50; s.trust = 50;
  CHECK(moodOf(s) == Mood::Content);
  s.joy = 90; CHECK(moodOf(s) == Mood::Happy);
  s.life = Life::Returning; CHECK(moodOf(s) == Mood::Grumpy);
}

TEST_CASE("phrases exist for needs and not for content") {
  CHECK(phraseFor(Mood::Hungry) != nullptr);
  CHECK(phraseFor(Mood::Content) == nullptr);
}

TEST_CASE("needsAttention flags sickness and tantrums") {
  PetState s = newPet(at(9));
  CHECK_FALSE(needsAttention(s));
  s.tantrum = true; CHECK(needsAttention(s));
}

TEST_CASE("save round-trips") {
  PetState s = newPet(at(9)); s.food = 12.5f; s.sick = 2; s.life = Life::Returning; s.winbackDone = 1;
  uint8_t buf[SAVE_SIZE];
  REQUIRE(encodeSave(s, buf, sizeof buf) == SAVE_SIZE);
  PetState out;
  REQUIRE(decodeSave(buf, sizeof buf, out));
  CHECK(out.food == doctest::Approx(12.5f));
  CHECK(out.sick == 2);
  CHECK(out.life == Life::Returning);
  CHECK(out.lastMin == s.lastMin);
}

TEST_CASE("corrupt, truncated or foreign saves are rejected") {
  PetState s = newPet(at(9));
  uint8_t buf[SAVE_SIZE];
  encodeSave(s, buf, sizeof buf);
  PetState out;
  CHECK_FALSE(decodeSave(buf, sizeof buf - 1, out));
  buf[20] ^= 0xFF;
  CHECK_FALSE(decodeSave(buf, sizeof buf, out));
  memset(buf, 0, sizeof buf);
  CHECK_FALSE(decodeSave(buf, sizeof buf, out));
  CHECK(encodeSave(s, buf, 4) == 0);
}

TEST_CASE("clock going backwards does not simulate or freeze") {
  PetState s = newPet(at(12));
  s.food = 50;
  catchUp(s, at(9));          // RTC jumped back three hours
  CHECK(s.lastMin == at(9));
  CHECK(s.food == doctest::Approx(50));
  catchUp(s, at(10));
  CHECK(s.food < 50);         // time flows again from the new clock
}
