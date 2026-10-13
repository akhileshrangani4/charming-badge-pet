#pragma once
#include <stdint.h>
namespace display {
// Anything shown above the mascot. Phase 2's brain sends the same shape.
struct Content {
  const char* label = nullptr;   // small mono line, e.g. "HELLO, I'M"
  const char* title = nullptr;   // pixel headline
  const char* body = nullptr;    // one Inter line
  const char* qr = nullptr;      // optional URL rendered as a QR code
  uint32_t ttlMs = 0;            // 0 = until dismissed
};
}
