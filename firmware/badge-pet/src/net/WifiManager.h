#pragma once
#include "Protocol.h"
namespace net {
class WifiManager {
 public:
  void begin();                              // load saved networks from NVS
  void add(const char* ssid, const char* psk);
  void setAll(const Network* list, uint8_t n);
  uint8_t count() const { return n_; }
  const Network& at(uint8_t i) const { return list_[i]; }
  void connect();                            // async scan, then join the strongest known network
  void poll();                               // drive the async scan/join state machine
  bool connected() const;
  void off();
 private:
  void save();
  Network list_[5]{}; uint8_t n_ = 0;
  enum class S : uint8_t { Idle, Scanning, Joining, Up } s_ = S::Idle;
  uint32_t t0_ = 0;
};
}
