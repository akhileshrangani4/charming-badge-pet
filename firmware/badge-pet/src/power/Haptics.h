#pragma once
#include <stdint.h>
namespace haptics {
// Non-blocking vibration: queue pulses, and call update() every loop.
void pulse(uint16_t onMs, uint8_t count = 1, uint16_t gapMs = 120, uint8_t strength = 200);
void update(uint32_t ms);
bool busy();
}
