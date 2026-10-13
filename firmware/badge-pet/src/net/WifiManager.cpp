#include "WifiManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <string.h>
namespace net {
static Preferences prefs;
void WifiManager::begin() {
  prefs.begin("net", false);
  n_ = (uint8_t)(prefs.getBytes("list", list_, sizeof list_) / sizeof(Network));
  if (n_ > 5) n_ = 0;
  while (n_ > 0 && !list_[n_ - 1].ssid[0]) n_--;
}
void WifiManager::save() { prefs.putBytes("list", list_, sizeof(Network) * n_); }
void WifiManager::add(const char* ssid, const char* psk) {
  for (uint8_t i = 0; i < n_; i++) if (!strcmp(list_[i].ssid, ssid)) { strncpy(list_[i].psk, psk, 64); list_[i].psk[64] = 0; save(); return; }
  if (n_ == 5) { memmove(&list_[0], &list_[1], sizeof(Network) * 4); n_ = 4; }   // drop the oldest
  strncpy(list_[n_].ssid, ssid, 32); list_[n_].ssid[32] = 0;
  strncpy(list_[n_].psk, psk, 64); list_[n_].psk[64] = 0;
  n_++; save();
}
void WifiManager::setAll(const Network* l, uint8_t n) { n_ = n > 5 ? 5 : n; memcpy(list_, l, sizeof(Network) * n_); save(); }
void WifiManager::connect() {
  if (!n_) return;
  WiFi.mode(WIFI_STA); WiFi.setSleep(true);
  WiFi.scanNetworks(true);
  s_ = S::Scanning; t0_ = millis();
}
void WifiManager::poll() {
  if (s_ == S::Scanning) {
    const int16_t found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) return;
    int best = -1, bestRssi = -1000;
    for (int i = 0; i < found; i++)
      for (uint8_t k = 0; k < n_; k++)
        if (WiFi.SSID(i) == list_[k].ssid && WiFi.RSSI(i) > bestRssi) { best = k; bestRssi = WiFi.RSSI(i); }
    WiFi.scanDelete();
    if (best < 0) { s_ = S::Idle; t0_ = millis(); return; }   // nothing known in range; retried by the task
    WiFi.begin(list_[best].ssid, list_[best].psk);
    s_ = S::Joining; t0_ = millis();
  } else if (s_ == S::Joining) {
    if (WiFi.status() == WL_CONNECTED) s_ = S::Up;
    else if (millis() - t0_ > 15000) { WiFi.disconnect(); s_ = S::Idle; }
  } else if (s_ == S::Up && WiFi.status() != WL_CONNECTED) {
    s_ = S::Idle;
  } else if (s_ == S::Idle && n_ && millis() - t0_ > 10000) {
    connect();
  }
}
bool WifiManager::connected() const { return s_ == S::Up && WiFi.status() == WL_CONNECTED; }
void WifiManager::off() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); s_ = S::Idle; }
}
