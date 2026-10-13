#include "Store.h"
#include "../pet/SaveCodec.h"
#include "../pet/Clock.h"
#include <M5Unified.h>
#include <Preferences.h>
#include <string.h>
namespace store {
static Preferences prefs;
void Store::begin() { prefs.begin("pet", false); }   // NVS namespace "pet"; never touches the ffat partition
bool Store::load(pet::PetState& out) {
  uint8_t buf[pet::SAVE_SIZE];
  const size_t n = prefs.getBytes("state", buf, sizeof buf);
  return pet::decodeSave(buf, n, out);
}
bool Store::save(const pet::PetState& s) {
  uint8_t buf[pet::SAVE_SIZE];
  const size_t n = pet::encodeSave(s, buf, sizeof buf);
  return n && prefs.putBytes("state", buf, n) == n;
}
int64_t Store::nowMin() {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt) || !pet::plausibleYear(dt.date.year)) return -1;
  return pet::localMinutes(dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes);
}
void Store::setClock(int y, int mo, int d, int h, int mi) {
  m5::rtc_datetime_t dt;
  dt.date.year = (int16_t)y; dt.date.month = (int8_t)mo; dt.date.date = (int8_t)d;
  dt.time.hours = (int8_t)h; dt.time.minutes = (int8_t)mi; dt.time.seconds = 0;
  M5.Rtc.setDateTime(dt);
}
void Store::ensureClock() {
  if (nowMin() >= 0) return;
  static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0}; int d = 1, y = 2026, h = 0, mi = 0, sec = 0;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &h, &mi, &sec);
  const char* p = strstr(months, mon);
  setClock(y, p ? (int)(p - months) / 3 + 1 : 1, d, h, mi);
}
}
