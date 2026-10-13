#include "PowerMgr.h"
#include <M5Unified.h>
#include <esp_sleep.h>
#include <esp_idf_version.h>
#include <driver/rtc_io.h>
namespace power {
void PowerMgr::begin() {
  timerWake_ = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  Serial.printf("wake cause=%d\n", (int)esp_sleep_get_wakeup_cause());
  M5.Display.setBrightness(BRIGHT);
  lastActivity_ = millis();
}
void PowerMgr::activity(uint32_t ms) {
  lastActivity_ = ms;
  if (dimmed_) { panel(); M5.Display.setBrightness(BRIGHT); dimmed_ = false; }
}
void PowerMgr::update(uint32_t ms) {
  if (!dimmed_ && ms - lastActivity_ >= DIM_MS) dimNow();
}
void PowerMgr::dimNow() { panel(); M5.Display.setBrightness(DIMMED); dimmed_ = true; }
void PowerMgr::buzz(int pulses) {
  for (int i = 0; i < pulses; i++) { M5.Power.setVibration(200); delay(120); M5.Power.setVibration(0); delay(120); }
}
void PowerMgr::sleepNow(uint32_t seconds) {
  const uint64_t mask = (1ULL << GPIO_NUM_1) | (1ULL << GPIO_NUM_2);   // buttons B and A, active low
  rtc_gpio_pullup_en(GPIO_NUM_1); rtc_gpio_pulldown_dis(GPIO_NUM_1);
  rtc_gpio_pullup_en(GPIO_NUM_2); rtc_gpio_pulldown_dis(GPIO_NUM_2);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
#else
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
  panel();
  M5.Power.deepSleep((uint64_t)seconds * 1000000ULL, false);
  esp_restart();   // deepSleep refused (e.g. a wake pin stuck low): restart rather than hang with the panel asleep
}
}
