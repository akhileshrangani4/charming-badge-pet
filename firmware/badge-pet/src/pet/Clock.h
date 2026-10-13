#pragma once
#include <stdint.h>
namespace pet {
inline int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + doe - 719468;
}
inline int64_t localMinutes(int y, int mo, int d, int h, int mi) {
  return (daysFromCivil(y, mo, d) - daysFromCivil(2000, 1, 1)) * 1440 + h * 60 + mi;
}
inline bool plausibleYear(int y) { return y >= 2025 && y <= 2099; }
inline int hourOf(int64_t m) { return (int)(((m / 60) % 24 + 24) % 24); }
}
