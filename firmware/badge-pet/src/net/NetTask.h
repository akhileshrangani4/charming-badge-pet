#pragma once
#include "Protocol.h"
namespace net {
enum class Status : uint8_t { NoWifi, Connecting, NoBrain, BadToken, Online };
struct Cmd {
  enum Kind : uint8_t { Hello, Snapshot, TalkStart, TalkEnd, TalkCancel, SetToken, AddNetwork, ClearNetworks, Stop } kind;
  SnapshotWire snap; char text[129]; char text2[65];
};
struct Event { enum Kind : uint8_t { StatusChanged, In } kind; Status status; Incoming in; };
void startNetTask();
bool post(const Cmd& c);
bool poll(Event& e);
float speakingLevel();
Status status();
uint8_t networkCount();
const char* networkName(uint8_t i);
}
