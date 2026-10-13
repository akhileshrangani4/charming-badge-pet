#include "NetTask.h"
#include "Config.h"
#include "WifiManager.h"
#include "../audio/Voice.h"
#include <M5Unified.h>
#include <WebSocketsClient.h>
#include <Preferences.h>
#include <atomic>
#include <string.h>
#include "RootCAs.h"
namespace net {
static QueueHandle_t cmdQ, evtQ;
static WifiManager wifi;
static audio::Voice voice;
static WebSocketsClient ws;
static std::atomic<float> level{0};
static std::atomic<uint8_t> stat{(uint8_t)Status::NoWifi};
static char token[65] = {0};
static bool wsStarted = false, talking = false;
static SnapshotWire lastSnap{"content", 80, 80, 80, 100, 50, 0, "normal", false, 1, 0, 12, 100, false};

static void emitStatus(Status s) {
  if ((uint8_t)s == stat.load()) return;
  stat = (uint8_t)s;
  Event e{Event::StatusChanged, s, {}}; xQueueSend(evtQ, &e, 0);
}
static void onWs(WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.println("net: brain connected");
      emitStatus(Status::Online);
      char buf[1024]; Network nets[5]; const uint8_t k = wifi.count();
      for (uint8_t i = 0; i < k; i++) nets[i] = wifi.at(i);
      const size_t n = encodeHello(buf, sizeof buf, FW_VERSION, lastSnap, nets, k);
      if (n) ws.sendTXT(buf, n);
      break;
    }
    case WStype_DISCONNECTED:
      Serial.println("net: brain disconnected");
      if (talking) { voice.stopRecording(); talking = false; }
      voice.stopPlayback();
      emitStatus(len >= 2 && payload && ((payload[0] << 8) | payload[1]) == 4001 ? Status::BadToken : Status::NoBrain);
      break;
    case WStype_TEXT: {
      Event e{Event::In, Status::Online, {}};
      if (!parse((const char*)payload, len, e.in)) break;
      if (e.in.kind == Incoming::Networks && e.in.netCount > 0) wifi.setAll(e.in.nets, e.in.netCount);   // never wipe the list
      if (e.in.kind == Incoming::SayEnd) voice.flushPlayback();
      xQueueSend(evtQ, &e, 0);
      break;
    }
    case WStype_BIN: voice.enqueuePlayback(payload, len); break;
    case WStype_ERROR:
      // The library reports an HTTP 401 on upgrade here; treat it as a bad token
      if (payload && strstr((const char*)payload, "401")) emitStatus(Status::BadToken);
      break;
    default: break;
  }
}
static void startWs() {
  if (wsStarted || !token[0]) return;
  static char hdr[96];
  snprintf(hdr, sizeof hdr, "Authorization: Bearer %s", token);
  ws.beginSslWithCA(BRAIN_HOST, 443, "/badge", BRAIN_ROOT_CAS);   // verify the Worker's certificate
  ws.setExtraHeaders(hdr);
  ws.onEvent(onWs);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(20000, 5000, 2);
  wsStarted = true;
}
static void task(void*) {
  Preferences p; p.begin("net", false); p.getString("token", token, sizeof token);
  wifi.begin(); voice.begin();
  Serial.printf("net: %u saved networks, token %s\n", wifi.count(), token[0] ? "set" : "missing");
  emitStatus(wifi.count() ? Status::Connecting : Status::NoWifi);
  wifi.connect();
  for (;;) {
    Cmd c;
    while (xQueueReceive(cmdQ, &c, 0)) {
      switch (c.kind) {
        case Cmd::Snapshot: case Cmd::Hello: {
          lastSnap = c.snap;
          if (ws.isConnected()) {
            char buf[1024]; Network nets[5]; const uint8_t k = wifi.count();
            for (uint8_t i = 0; i < k; i++) nets[i] = wifi.at(i);
            const size_t n = c.kind == Cmd::Hello ? encodeHello(buf, sizeof buf, FW_VERSION, c.snap, nets, k) : encodeSnapshot(buf, sizeof buf, c.snap);
            if (n) ws.sendTXT(buf, n);
          }
          break;
        }
        case Cmd::TalkStart: if (ws.isConnected()) { char b[32]; ws.sendTXT(b, encodeType(b, sizeof b, "talk_start")); voice.startRecording(); talking = true; } break;
        case Cmd::TalkEnd: if (talking) { voice.stopRecording(); talking = false; char b[32]; ws.sendTXT(b, encodeType(b, sizeof b, "talk_end")); } break;
        case Cmd::TalkCancel: if (talking) { voice.stopRecording(); talking = false; } { char b[32]; if (ws.isConnected()) ws.sendTXT(b, encodeType(b, sizeof b, "talk_cancel")); } voice.stopPlayback(); break;
        case Cmd::SetToken: strncpy(token, c.text, 64); token[64] = 0; p.putString("token", token); if (wsStarted) { ws.disconnect(); wsStarted = false; } break;
        case Cmd::AddNetwork: {
          wifi.add(c.text, c.text2);
          Serial.printf("net: saved network '%s'\n", c.text);
          if (ws.isConnected()) { Network n{}; strncpy(n.ssid, c.text, 32); strncpy(n.psk, c.text2, 64); char b[160]; const size_t len = encodeAddNetwork(b, sizeof b, n); if (len) ws.sendTXT(b, len); }
          if (!wifi.connected()) wifi.connect();
          break;
        }
        case Cmd::ClearNetworks: wifi.setAll(nullptr, 0); break;
        case Cmd::Stop: if (wsStarted) ws.disconnect(); wifi.off(); vTaskDelete(nullptr); break;
      }
    }
    wifi.poll();
    if (wifi.connected()) { startWs(); ws.loop(); if (!ws.isConnected() && stat.load() == (uint8_t)Status::Online) emitStatus(Status::NoBrain); }
    else emitStatus(wifi.count() ? Status::Connecting : Status::NoWifi);
    const int16_t* chunk; size_t n;
    while (talking && voice.recordChunk(chunk, n)) { ws.sendBIN((const uint8_t*)chunk, n * 2); }
    voice.pumpPlayback();
    level = voice.level();
    vTaskDelay(1);
  }
}
void startNetTask() {
  cmdQ = xQueueCreate(8, sizeof(Cmd)); evtQ = xQueueCreate(8, sizeof(Event));
  xTaskCreatePinnedToCore(task, "net", 12288, nullptr, 1, nullptr, 0);   // core 0, below the push task (2)
}
bool post(const Cmd& c) { return cmdQ && xQueueSend(cmdQ, &c, 0) == pdTRUE; }
bool poll(Event& e) { return evtQ && xQueueReceive(evtQ, &e, 0) == pdTRUE; }
float speakingLevel() { return level.load(); }
Status status() { return (Status)stat.load(); }
uint8_t networkCount() { return wifi.count(); }
const char* networkName(uint8_t i) { return wifi.at(i).ssid; }
}
