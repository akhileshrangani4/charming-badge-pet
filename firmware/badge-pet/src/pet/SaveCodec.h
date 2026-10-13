#pragma once
#include "PetState.h"
#include <stddef.h>
namespace pet {
constexpr uint32_t SAVE_MAGIC = 0x31544550u;  // "PET1"
constexpr uint16_t SAVE_VERSION = 1;
constexpr size_t SAVE_SIZE = 4 + 2 + 2 + sizeof(PetState) + 4;
size_t encodeSave(const PetState& s, uint8_t* out, size_t cap);
bool decodeSave(const uint8_t* in, size_t n, PetState& out);
}
