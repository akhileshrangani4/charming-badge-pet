#include "App.h"
#include <M5Unified.h>
#include "../pet/PetRules.h"
#include "../pet/Mood.h"
#include "../input/Input.h"
#include "../render/Mascot.h"
#include "../render/TextLayer.h"
#include "../scenes/Scenes.h"
#include "../store/Store.h"
#include "../power/PowerMgr.h"
#include "../power/Haptics.h"
#include "../net/NetTask.h"
#include "../net/Protocol.h"
namespace app {
static M5Canvas canvases[2] = {M5Canvas(&M5.Display), M5Canvas(&M5.Display)};
static int slot = 0;
static render::Mascot mascot;
static render::TextLayer text;
static scenes::Scenes ui(canvases[0], text, mascot);
static input::Input in;
static store::Store st;
static pet::PetState pet_;
static power::PowerMgr pwr;

static int64_t debugOffsetMin_ = 0;
static pet::PetState snapshot_;   // K/U serial commands: test on a copy without hurting the real pet
static int64_t snapshotOffset_ = 0;
static bool hasSnapshot_ = false;   // added by the F serial command so every time-gated rule can be tested
static int64_t cached = -1;
static uint32_t readMs = 0;
static int64_t goodMin = -1;      // last good RTC reading, and millis() when it was taken
static uint32_t goodMs = 0;
static int64_t now() {
  if (cached < 0 || millis() - readMs >= 1000) {   // RTC is I2C: read at most once a second
    readMs = millis();
    cached = st.nowMin();
    if (cached >= 0) { goodMin = cached; goodMs = readMs; }
    else if (goodMin >= 0) cached = goodMin + (millis() - goodMs) / 60000;   // RTC read failed: keep time flowing
  }
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
  for (;;) {
    if (!xQueueReceive(pushQueue, &job, portMAX_DELAY)) continue;
    pushNow(*job.canvas, job.rect);
    xSemaphoreGive(pushIdle);
    vTaskDelay(1);   // the SPI push busy-waits; yield so core 0's idle task can feed the task watchdog
  }
}
static void waitDisplayIdle() { xSemaphoreTake(pushIdle, portMAX_DELAY); xSemaphoreGive(pushIdle); }
static void submit(M5Canvas& c, const render::Rect& r) {
  xSemaphoreTake(pushIdle, portMAX_DELAY);   // the previous frame must be on screen before this canvas is handed off
  if (r.empty()) { xSemaphoreGive(pushIdle); return; }
  PushJob job{&c, r};
  xQueueSend(pushQueue, &job, portMAX_DELAY);
}

static const char* moodName(pet::Mood m) {
  static const char* names[] = {"gone", "paused", "sleeping", "sick", "tantrum", "hungry", "dirty", "sleepy", "grumpy", "sad", "happy", "content"};
  return names[(int)m];
}
static net::SnapshotWire wireOf(const pet::PetState& s) {
  return {moodName(pet::moodOf(s)), s.food, s.energy, s.joy, s.hygiene, s.trust, s.sick,
          s.life == pet::Life::Gone ? "gone" : (s.life == pet::Life::Returning ? "returning" : "normal"),
          s.asleep, (uint16_t)(s.ageMin / 1440), s.growth, (uint8_t)pet::hourOf(s.lastMin)};
}
static void postSnapshot(bool hello) { net::Cmd c{}; c.kind = hello ? net::Cmd::Hello : net::Cmd::Snapshot; c.snap = wireOf(pet_); net::post(c); }
static void stopNet() { net::Cmd stop{}; stop.kind = net::Cmd::Stop; net::post(stop); delay(300); }   // Wi-Fi off before deep sleep

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
  if (ui.current() == scenes::SceneId::Pet)   // don't overwrite an event message from the catch-up
    if (const char* p = pet::phraseFor(pet::moodOf(pet_))) { display::Content k; k.title = p; k.ttlMs = 2500; ui.showMessage(k, millis()); }
  st.save(pet_);
  lastSaveMs_ = millis();
  pwr.setBeforePanel(waitDisplayIdle);
  pwr.begin();
  if (pwr.wokeByTimer()) {
    if (pet::needsAttention(pet_)) {
      pwr.buzz(2);
      const uint32_t until = millis() + 5000;   // brief glance at the pet, then back to sleep
      while (millis() < until) {
        M5.update();
        haptics::update(millis());
        ui.setTarget(canvases[slot], slot);
        submit(canvases[slot], ui.draw(pet_, millis()));
        slot ^= 1;
        if (M5.BtnA.isPressed() || M5.BtnB.isPressed() || M5.Touch.getDetail().isPressed()) { pwr.activity(millis()); return; }
      }
    }
    st.save(pet_);
    waitDisplayIdle();
    pwr.sleepNow(power::WAKE_EVERY_S);
  }
}

void App::serialCommands() {
  if (!Serial.available()) return;
  char line[160] = {0};
  const size_t n = Serial.readBytesUntil('\n', line, sizeof line - 1); line[n] = 0;
  pwr.activity(millis());   // a serial session counts as someone using the badge
  int y, mo, d, h, mi, mins;
  if (sscanf(line, "T %d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5 &&
      !(pet::plausibleYear(y) && mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && h >= 0 && h <= 23 && mi >= 0 && mi <= 59)) {
    Serial.println("bad time: use T YYYY-MM-DD HH:MM (24 h, year 2025-2099)");
  } else if (sscanf(line, "T %d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5) {
    st.setClock(y, mo, d, h, mi);
    cached = -1;
    pet::resyncClock(pet_, pet::localMinutes(y, mo, d, h, mi) + debugOffsetMin_);   // correcting the clock is not time passing
    Serial.println("clock set");
  }
  else if (sscanf(line, "F %d", &mins) == 1) { debugOffsetMin_ += mins; ui.onEvents(pet::catchUp(pet_, now()), pet_, millis()); Serial.println("forwarded"); }
  else if (sscanf(line, "G %d", &mins) == 1) { ui.debugGo((scenes::SceneId)mins, millis()); Serial.println("scene"); }
  else if (sscanf(line, "X %d", &mins) == 1) { ui.debugExpression(mins, millis() + 20000); Serial.println("expr"); }
  else if (line[0] == 'P') {   // dump the last drawn frame: "FRAME 468 468\n" + raw big-endian RGB565
    waitDisplayIdle();
    M5Canvas& c = canvases[slot ^ 1];
    Serial.printf("FRAME %d %d\n", render::SCREEN, render::SCREEN);
    Serial.write((const uint8_t*)c.getBuffer(), (size_t)render::SCREEN * render::SCREEN * 2);
    Serial.flush();
  }
  else if (line[0] == 'K') { snapshot_ = pet_; snapshotOffset_ = debugOffsetMin_; hasSnapshot_ = true; Serial.println("kept"); }
  else if (line[0] == 'U' && hasSnapshot_) {   // restore the kept pet and undo any fast-forward since
    debugOffsetMin_ = snapshotOffset_; cached = -1;
    pet_ = snapshot_; pet::resyncClock(pet_, now()); st.save(pet_);
    ui.debugGo(pet_.life == pet::Life::Gone ? scenes::SceneId::Gone : scenes::SceneId::Pet, millis());
    Serial.println("restored");
  }
  else if (line[0] == 'W' && line[1] == ' ') {   // W <ssid> <password>
    net::Cmd c{}; c.kind = net::Cmd::AddNetwork;
    if (sscanf(line + 2, "%32s %64s", c.text, c.text2) >= 1) { net::post(c); Serial.println("network added"); }
  }
  else if (line[0] == 'N' && (line[1] == 0 || line[1] == '\r')) { for (uint8_t i = 0; i < net::networkCount(); i++) Serial.printf("%u %s\n", i, net::networkName(i)); }
  else if (line[0] == 'D' && line[1] == ' ') { net::Cmd c{}; c.kind = net::Cmd::SetToken; strncpy(c.text, line + 2, 64); for (char* p = c.text; *p; p++) if (*p == '\r') *p = 0; net::post(c); Serial.println("token set"); }
  else if (strncmp(line, "R YES", 5) == 0) { pet_ = pet::newPet(now()); st.save(pet_); Serial.println("new pet"); }
  else if (line[0] == 'S') {
    Serial.printf("min=%lld food=%.0f energy=%.0f joy=%.0f hyg=%.0f trust=%.0f disc=%.0f spoil=%.0f sick=%u messes=%u life=%u asleep=%d paused=%d budget=%.0f age=%u growth=%u mood=%u\n",
      (long long)pet_.lastMin, pet_.food, pet_.energy, pet_.joy, pet_.hygiene, pet_.trust, pet_.discipline, pet_.spoil,
      pet_.sick, pet_.messes, (unsigned)pet_.life, pet_.asleep, pet_.paused, pet_.pauseBudget, (unsigned)pet_.ageMin, pet_.growth,
      (unsigned)pet::moodOf(pet_));
  }
}

void App::loop() {
  // Started here, not in setup(): the timer-wake glance can return early from setup().
  static bool netStarted = false;
  if (!netStarted) { net::startNetTask(); postSnapshot(true); netStarted = true; }
  M5.update();
  const uint32_t ms = millis();
  serialCommands();
  uint32_t ev = pet::catchUp(pet_, now());
  in.setTalkEnabled(pet_.life != pet::Life::Gone && ui.current() == scenes::SceneId::Pet);
  const input::InputFrame f = in.poll(ms);
  using BE = input::ButtonEvent;
  if (f.button == BE::TalkStart) {
    const net::Status st = net::status();
    if (st == net::Status::Online) { net::Cmd c{}; c.kind = net::Cmd::TalkStart; net::post(c); ui.setTalkFace(net::Face::Listening); }
    else ui.showStatus(st == net::Status::NoWifi || st == net::Status::Connecting ? "NO WIFI" : (st == net::Status::BadToken ? "RE-PAIR ME" : "CAN'T REACH ME"), ms);
  }
  if (f.button == BE::TalkEnd) { net::Cmd c{}; c.kind = net::Cmd::TalkEnd; net::post(c); ui.setTalkFace(net::Face::Thinking); }
  if (f.button == BE::TalkCancel) { net::Cmd c{}; c.kind = net::Cmd::TalkCancel; net::post(c); ui.setTalkFace(net::Face::Idle); }
  net::Event ne;
  while (net::poll(ne)) {
    if (ne.kind == net::Event::StatusChanged) { if (ne.status != net::Status::Online) ui.setTalkFace(net::Face::Idle); continue; }
    switch (ne.in.kind) {
      case net::Incoming::FaceMsg: ui.setTalkFace(ne.in.face); break;
      case net::Incoming::CardMsg: ui.showCard(ne.in.card, ms); break;
      case net::Incoming::SayEnd: ui.setTalkFace(net::Face::Idle); break;
      case net::Incoming::Error: ui.setTalkFace(net::Face::Idle); ui.showStatus(ne.in.err == net::Err::OverBudget ? "TOO SLEEPY" : "BRAIN FOG", ms); break;
      default: break;
    }
  }
  mascot.setMouth(net::speakingLevel());
  ev |= ui.handle(f, pet_, now(), ms);
  if (ev) ui.onEvents(ev, pet_, ms);
  static pet::Mood lastMood = pet::Mood::Content;
  static uint32_t lastSnapMs = 0;
  // On mood change, plus every 2 min, so the app's online dot and the brain's stats stay current.
  if (pet::moodOf(pet_) != lastMood || ms - lastSnapMs > 120000) { lastMood = pet::moodOf(pet_); lastSnapMs = ms; postSnapshot(false); }
  // A flash write stalls both cores for a moment, so batch saves: at most every 30 s after a
  // change, every 5 min regardless, and always right before sleeping.
  static bool unsaved = false;
  if (ev || ui.cared) { unsaved = true; ui.cared = false; }
  if ((unsaved && ms - lastSaveMs_ > 30000) || ms - lastSaveMs_ > 5 * 60 * 1000) { st.save(pet_); lastSaveMs_ = ms; unsaved = false; }
  haptics::update(ms);
  if (f.any) pwr.activity(ms);
  pwr.update(ms);
  if (ev & (pet::EV_AUTO_UNPAUSE | pet::EV_SICK | pet::EV_SICKER | pet::EV_TANTRUM | pet::EV_RAN_AWAY)) pwr.buzz(1);
  if (pet::moodOf(pet_) == pet::Mood::Sleeping && !pwr.dimmed()) pwr.dimNow();
  if (pwr.shouldSleep(ms) && ui.current() != scenes::SceneId::Play) { st.save(pet_); stopNet(); pwr.sleepNow(power::WAKE_EVERY_S); }
  ui.setTarget(canvases[slot], slot);
  submit(canvases[slot], ui.draw(pet_, ms));
  slot ^= 1;
  if (pwr.dimmed()) delay(40);   // ~15 fps while dimmed

  static uint32_t frames = 0, t0 = 0;
  if (++frames == 120) { Serial.printf("fps=%.1f\n", 120000.f / (ms - t0)); frames = 0; t0 = ms; }
}
}
