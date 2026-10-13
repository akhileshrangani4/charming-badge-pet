#include "App.h"
#include <M5Unified.h>
#include "../pet/PetRules.h"
#include "../pet/Mood.h"
#include "../input/Input.h"
#include "../render/Mascot.h"
#include "../render/TextLayer.h"
#include "../scenes/Scenes.h"
#include "../store/Store.h"
namespace app {
static M5Canvas canvases[2] = {M5Canvas(&M5.Display), M5Canvas(&M5.Display)};
static int slot = 0;
static render::Mascot mascot;
static render::TextLayer text;
static scenes::Scenes ui(canvases[0], text, mascot);
static input::Input in;
static store::Store st;
static pet::PetState pet_;

static int64_t debugOffsetMin_ = 0;   // added by the F serial command so every time-gated rule can be tested
static int64_t cached = -1;
static uint32_t readMs = 0;
static int64_t now() {
  if (cached < 0 || millis() - readMs >= 1000) { cached = st.nowMin(); readMs = millis(); }   // RTC is I2C: read at most once a second
  return (cached >= 0 ? cached : pet_.lastMin - debugOffsetMin_) + debugOffsetMin_;
}

static void pushNow(M5Canvas& c, const render::Rect& r) {
  if (r.empty()) return;
  M5.Display.startWrite();
  M5.Display.setClipRect(r.x, r.y, r.w, r.h);
  c.pushSprite(0, 0);
  M5.Display.clearClipRect();
  M5.Display.endWrite();
}

// Frames are pushed on core 0 while core 1 draws the next one into the other canvas.
struct PushJob { M5Canvas* canvas; render::Rect rect; };
static QueueHandle_t pushQueue;
static SemaphoreHandle_t pushIdle;
static void pushTask(void*) {
  PushJob job;
  for (;;) if (xQueueReceive(pushQueue, &job, portMAX_DELAY)) { pushNow(*job.canvas, job.rect); xSemaphoreGive(pushIdle); }
}
static void waitDisplayIdle() { xSemaphoreTake(pushIdle, portMAX_DELAY); xSemaphoreGive(pushIdle); }
static void submit(M5Canvas& c, const render::Rect& r) {
  xSemaphoreTake(pushIdle, portMAX_DELAY);   // the previous frame must be on screen before this canvas is handed off
  if (r.empty()) { xSemaphoreGive(pushIdle); return; }
  PushJob job{&c, r};
  xQueueSend(pushQueue, &job, portMAX_DELAY);
}

void App::setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.fillScreen(TFT_BLACK);
  for (auto& c : canvases) { c.setPsram(true); c.setColorDepth(16); c.createSprite(render::SCREEN, render::SCREEN); c.fillScreen(TFT_BLACK); }
  pushQueue = xQueueCreate(1, sizeof(PushJob));
  pushIdle = xSemaphoreCreateBinary();
  xSemaphoreGive(pushIdle);
  xTaskCreatePinnedToCore(pushTask, "push", 4096, nullptr, 2, nullptr, 0);
  st.begin();
  st.ensureClock();
  const int64_t t = now();
  if (!st.load(pet_)) { pet_ = pet::newPet(t); Serial.println("new pet"); }
  const uint32_t ev = pet::catchUp(pet_, t);
  ui.onEvents(ev, pet_, millis());
  if (const char* p = pet::phraseFor(pet::moodOf(pet_))) { display::Content k; k.title = p; k.ttlMs = 2500; ui.showMessage(k, millis()); }
  st.save(pet_);
  lastSaveMs_ = millis();
}

void App::serialCommands() {
  if (!Serial.available()) return;
  char line[48] = {0};
  const size_t n = Serial.readBytesUntil('\n', line, sizeof line - 1); line[n] = 0;
  int y, mo, d, h, mi, mins;
  if (sscanf(line, "T %d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5) {
    st.setClock(y, mo, d, h, mi);
    cached = -1;
    pet_.lastMin = pet::localMinutes(y, mo, d, h, mi) + debugOffsetMin_;   // correcting the clock is not time passing
    Serial.println("clock set");
  }
  else if (sscanf(line, "F %d", &mins) == 1) { debugOffsetMin_ += mins; ui.onEvents(pet::catchUp(pet_, now()), pet_, millis()); Serial.println("forwarded"); }
  else if (strncmp(line, "R YES", 5) == 0) { pet_ = pet::newPet(now()); st.save(pet_); Serial.println("new pet"); }
  else if (line[0] == 'S') {
    Serial.printf("min=%lld food=%.0f energy=%.0f joy=%.0f hyg=%.0f trust=%.0f disc=%.0f spoil=%.0f sick=%u messes=%u life=%u asleep=%d paused=%d budget=%.0f age=%u growth=%u mood=%u\n",
      (long long)pet_.lastMin, pet_.food, pet_.energy, pet_.joy, pet_.hygiene, pet_.trust, pet_.discipline, pet_.spoil,
      pet_.sick, pet_.messes, (unsigned)pet_.life, pet_.asleep, pet_.paused, pet_.pauseBudget, (unsigned)pet_.ageMin, pet_.growth,
      (unsigned)pet::moodOf(pet_));
  }
}

void App::loop() {
  M5.update();
  const uint32_t ms = millis();
  serialCommands();
  uint32_t ev = pet::catchUp(pet_, now());
  const input::InputFrame f = in.poll(ms);
  ev |= ui.handle(f, pet_, now(), ms);
  if (ev) ui.onEvents(ev, pet_, ms);
  if (ev || ui.cared) { st.save(pet_); lastSaveMs_ = ms; ui.cared = false; }
  if (ms - lastSaveMs_ > 5 * 60 * 1000) { st.save(pet_); lastSaveMs_ = ms; }
  ui.setTarget(canvases[slot], slot);
  submit(canvases[slot], ui.draw(pet_, ms));
  slot ^= 1;

  static uint32_t frames = 0, t0 = 0;
  if (++frames == 120) { Serial.printf("fps=%.1f\n", 120000.f / (ms - t0)); frames = 0; t0 = ms; }
}
}
