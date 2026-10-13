#pragma once
#include <stddef.h>
#include <stdint.h>
namespace net {
enum class Face : uint8_t { Idle, Listening, Thinking, Speaking };
enum class Err : uint8_t { OverBudget, BrainFog, Busy, Other };
struct Card { char title[24]; char body[64]; char qr[121]; };   // 120 chars fits a version-6 QR at 140 px
struct Network { char ssid[33]; char psk[65]; };
struct Incoming {
  enum Kind : uint8_t { Unknown, FaceMsg, CardMsg, SayEnd, Error, Networks } kind = Unknown;
  Face face = Face::Idle; Card card{}; Err err = Err::Other; Network nets[5]{}; uint8_t netCount = 0;
};
bool parse(const char* json, size_t n, Incoming& out);
struct SnapshotWire { const char* mood; float food, energy, joy, hygiene, trust; uint8_t sick; const char* life; bool asleep; uint16_t ageDays; uint8_t growth; uint8_t hour; };
size_t encodeHello(char* buf, size_t cap, const char* fw, const SnapshotWire& s, const Network* nets = nullptr, uint8_t netCount = 0);
size_t encodeAddNetwork(char* buf, size_t cap, const Network& n);
size_t encodeSnapshot(char* buf, size_t cap, const SnapshotWire& s);
size_t encodeType(char* buf, size_t cap, const char* t);
}
