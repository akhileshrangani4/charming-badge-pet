#include "Protocol.h"
#include <ArduinoJson.h>
#include <string.h>
namespace net {
static void copy(char* dst, size_t cap, const char* src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1); dst[cap - 1] = 0;
}
bool parse(const char* json, size_t n, Incoming& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, n) || !doc.is<JsonObject>()) return false;
  const char* t = doc["t"] | "";
  out = Incoming{};
  if (!strcmp(t, "face")) {
    const char* f = doc["f"] | "idle";
    out.kind = Incoming::FaceMsg;
    out.face = !strcmp(f, "listening") ? Face::Listening : !strcmp(f, "thinking") ? Face::Thinking : !strcmp(f, "speaking") ? Face::Speaking : Face::Idle;
    return true;
  }
  if (!strcmp(t, "card")) {
    out.kind = Incoming::CardMsg;
    copy(out.card.title, sizeof out.card.title, doc["title"] | "");
    copy(out.card.body, sizeof out.card.body, doc["body"] | "");
    copy(out.card.qr, sizeof out.card.qr, doc["qr"] | "");
    return true;
  }
  if (!strcmp(t, "say_end")) { out.kind = Incoming::SayEnd; return true; }
  if (!strcmp(t, "error")) {
    const char* c = doc["code"] | "";
    out.kind = Incoming::Error;
    out.err = !strcmp(c, "over_budget") ? Err::OverBudget : !strcmp(c, "brain_fog") ? Err::BrainFog : !strcmp(c, "busy") ? Err::Busy : Err::Other;
    return true;
  }
  if (!strcmp(t, "networks")) {
    out.kind = Incoming::Networks;
    for (JsonObject n : doc["list"].as<JsonArray>()) {
      if (out.netCount >= 5) break;
      copy(out.nets[out.netCount].ssid, sizeof out.nets[0].ssid, n["ssid"] | "");
      copy(out.nets[out.netCount].psk, sizeof out.nets[0].psk, n["psk"] | "");
      if (out.nets[out.netCount].ssid[0]) out.netCount++;
    }
    return true;
  }
  return false;
}
static void fillSnapshot(JsonObject o, const SnapshotWire& s) {
  o["mood"] = s.mood; o["food"] = (int)s.food; o["energy"] = (int)s.energy; o["joy"] = (int)s.joy;
  o["hygiene"] = (int)s.hygiene; o["trust"] = (int)s.trust; o["sick"] = s.sick; o["life"] = s.life;
  o["asleep"] = s.asleep; o["ageDays"] = s.ageDays; o["growth"] = s.growth; o["hour"] = s.hour;
  o["battery"] = s.battery; o["charging"] = s.charging;
}
static size_t finish(JsonDocument& doc, char* buf, size_t cap) {
  if (measureJson(doc) + 1 > cap) return 0;
  return serializeJson(doc, buf, cap);
}
size_t encodeHello(char* buf, size_t cap, const char* fw, const SnapshotWire& s, const Network* nets, uint8_t netCount) {
  JsonDocument doc; doc["t"] = "hello"; doc["fw"] = fw; fillSnapshot(doc["snapshot"].to<JsonObject>(), s);
  JsonArray list = doc["networks"].to<JsonArray>();
  for (uint8_t i = 0; i < netCount; i++) { JsonObject o = list.add<JsonObject>(); o["ssid"] = nets[i].ssid; o["psk"] = nets[i].psk; }
  return finish(doc, buf, cap);
}
size_t encodeAddNetwork(char* buf, size_t cap, const Network& n) {
  JsonDocument doc; doc["t"] = "add_network"; doc["ssid"] = n.ssid; doc["psk"] = n.psk;
  return finish(doc, buf, cap);
}
size_t encodeSnapshot(char* buf, size_t cap, const SnapshotWire& s) {
  JsonDocument doc; doc["t"] = "snapshot"; fillSnapshot(doc.as<JsonObject>(), s);
  return finish(doc, buf, cap);
}
size_t encodeType(char* buf, size_t cap, const char* t) {
  JsonDocument doc; doc["t"] = t;
  return finish(doc, buf, cap);
}
}
