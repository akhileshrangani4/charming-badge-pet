#include "doctest.h"
#include "net/Protocol.h"
#include <string.h>
#include <string>
using namespace net;

static bool P(const std::string& s, Incoming& in) { return parse(s.c_str(), s.size(), in); }

TEST_CASE("parses face messages") {
  Incoming in;
  REQUIRE(P("{\"t\":\"face\",\"f\":\"thinking\"}", in));
  CHECK(in.kind == Incoming::FaceMsg);
  CHECK(in.face == Face::Thinking);
}
TEST_CASE("parses a card and truncates long fields safely") {
  Incoming in;
  std::string longTitle(100, 'X');
  REQUIRE(P("{\"t\":\"card\",\"title\":\"" + longTitle + "\",\"qr\":\"https://a.b\"}", in));
  CHECK(in.kind == Incoming::CardMsg);
  CHECK(strlen(in.card.title) == 23);
  CHECK(std::string(in.card.qr) == "https://a.b");
  CHECK(in.card.body[0] == 0);
}
TEST_CASE("parses errors, say_end and networks") {
  Incoming in;
  REQUIRE(P("{\"t\":\"error\",\"code\":\"over_budget\"}", in)); CHECK(in.err == Err::OverBudget);
  REQUIRE(P("{\"t\":\"say_end\"}", in)); CHECK(in.kind == Incoming::SayEnd);
  REQUIRE(P("{\"t\":\"networks\",\"list\":[{\"ssid\":\"home\",\"psk\":\"pw\"},{\"ssid\":\"hotspot\",\"psk\":\"x\"}]}", in));
  CHECK(in.netCount == 2); CHECK(std::string(in.nets[1].ssid) == "hotspot");
}
TEST_CASE("rejects malformed or unknown input") {
  Incoming in;
  CHECK_FALSE(P("{not json", in));
  CHECK_FALSE(P("{\"t\":\"wat\"}", in));
  CHECK_FALSE(P("[]", in));
  std::string many = "{\"t\":\"networks\",\"list\":[";
  for (int i = 0; i < 9; i++) many += std::string(i ? "," : "") + "{\"ssid\":\"n\",\"psk\":\"p\"}";
  many += "]}";
  REQUIRE(P(many, in)); CHECK(in.netCount == 5);
}
TEST_CASE("encodes hello, snapshot and plain types") {
  SnapshotWire s{"happy", 90, 80, 95, 100, 70, 0, "normal", false, 2, 1, 14};
  char buf[512];
  Network n[1] = {{"home", "pw"}};
  REQUIRE(encodeHello(buf, sizeof buf, "2.0.0", s, n, 1) > 0);
  CHECK(std::string(buf).find("\"t\":\"hello\"") != std::string::npos);
  CHECK(std::string(buf).find("\"networks\":[{\"ssid\":\"home\"") != std::string::npos);
  CHECK(std::string(buf).find("\"hour\":14") != std::string::npos);
  CHECK(std::string(buf).find("\"mood\":\"happy\"") != std::string::npos);
  REQUIRE(encodeSnapshot(buf, sizeof buf, s) > 0);
  CHECK(std::string(buf).find("\"t\":\"snapshot\"") != std::string::npos);
  REQUIRE(encodeType(buf, sizeof buf, "talk_start") > 0);
  CHECK(std::string(buf) == "{\"t\":\"talk_start\"}");
  CHECK(encodeType(buf, 4, "talk_start") == 0);   // too small: refuse instead of truncating JSON
}
