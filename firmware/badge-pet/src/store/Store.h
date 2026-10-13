#pragma once
#include "../pet/PetState.h"
namespace store {
class Store {
 public:
  void begin();
  bool load(pet::PetState& out);
  bool save(const pet::PetState& s);
  int64_t nowMin();
  void ensureClock();
  void setClock(int y, int mo, int d, int h, int mi);
};
}
