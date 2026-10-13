#include <M5Unified.h>
static M5Canvas canvas(&M5.Display);
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  bool ok = canvas.createSprite(M5.Display.width(), M5.Display.height());
  Serial.printf("board=%d w=%d h=%d psram=%u sprite=%d imu=%d rtc=%d touch=%d\n",
    (int)M5.getBoard(), M5.Display.width(), M5.Display.height(), (unsigned)ESP.getPsramSize(),
    ok, M5.Imu.isEnabled(), M5.Rtc.isEnabled(), M5.Touch.isEnabled());
}
void loop() {
  static uint32_t frames = 0, t0 = millis();
  M5.update();
  canvas.fillScreen(TFT_BLACK);
  canvas.fillSmoothCircle(233 + (int)(sinf(millis() / 500.f) * 100), 233, 60, 0xF77448u);
  M5.Display.startWrite();
  canvas.pushSprite(0, 0);
  M5.Display.endWrite();
  if (M5.BtnA.wasClicked()) { M5.Power.setVibration(160); delay(60); M5.Power.setVibration(0); Serial.println("A"); }
  if (M5.BtnB.wasClicked()) Serial.println("B");
  if (++frames == 60) { Serial.printf("fps=%.1f\n", 60000.f / (millis() - t0)); frames = 0; t0 = millis(); }
}
