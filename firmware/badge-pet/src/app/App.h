#pragma once
#include <stdint.h>
namespace app {
class App {
 public:
  void setup();
  void loop();
 private:
  void serialCommands();
  uint32_t lastSaveMs_ = 0;
};
}
