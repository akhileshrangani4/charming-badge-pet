#include "SaveCodec.h"
#include <string.h>
namespace pet {
static uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
  return ~c;
}
size_t encodeSave(const PetState& s, uint8_t* out, size_t cap) {
  if (cap < SAVE_SIZE) return 0;
  const uint16_t ver = SAVE_VERSION, size = (uint16_t)sizeof(PetState);
  memcpy(out, &SAVE_MAGIC, 4); memcpy(out + 4, &ver, 2); memcpy(out + 6, &size, 2);
  memcpy(out + 8, &s, sizeof(PetState));
  const uint32_t crc = crc32(out, 8 + sizeof(PetState));
  memcpy(out + 8 + sizeof(PetState), &crc, 4);
  return SAVE_SIZE;
}
bool decodeSave(const uint8_t* in, size_t n, PetState& out) {
  if (n != SAVE_SIZE) return false;
  uint32_t magic, crc; uint16_t ver, size;
  memcpy(&magic, in, 4); memcpy(&ver, in + 4, 2); memcpy(&size, in + 6, 2);
  if (magic != SAVE_MAGIC || ver != SAVE_VERSION || size != sizeof(PetState)) return false;
  memcpy(&crc, in + 8 + sizeof(PetState), 4);
  if (crc != crc32(in, 8 + sizeof(PetState))) return false;
  memcpy(&out, in + 8, sizeof(PetState));
  return true;
}
}
