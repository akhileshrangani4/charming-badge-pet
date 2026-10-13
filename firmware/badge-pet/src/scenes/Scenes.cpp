#include "Scenes.h"
#include "../render/Effects.h"
#include "../render/ContentView.h"
#include "../render/Colors.h"
#include "../power/Haptics.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
namespace scenes {
using namespace render;
using input::ButtonEvent;
static const char* MENU_LABELS[] = {"FEED", "SNACK", "PLAY", "CLEAN", "MEDS", "LIGHTS", "COFFEE"};

static Expression exprFor(pet::Mood m) {
  switch (m) {
    case pet::Mood::Paused: case pet::Mood::Sleeping: return Expression::Sleep;
    case pet::Mood::Sick: return Expression::Sick;
    case pet::Mood::Tantrum: return Expression::Tantrum;
    case pet::Mood::Hungry: return Expression::Hungry;
    case pet::Mood::Dirty: case pet::Mood::Sad: return Expression::Sad;
    case pet::Mood::Sleepy: return Expression::Sleepy;
    case pet::Mood::Grumpy: return Expression::Grumpy;
    case pet::Mood::Happy: return Expression::Happy;
    default: return Expression::Idle;
  }
}

void Scenes::showMessage(const display::Content& k, uint32_t ms) {
  msg_ = k;
  if (k.title) { strncpy(msgTitle_, k.title, sizeof msgTitle_ - 1); msg_.title = msgTitle_; }
  go(SceneId::Message, ms);
  full_ = true;   // a new message while one is showing must still be pushed
}

void Scenes::onEvents(uint32_t ev, const pet::PetState& s, uint32_t ms) {
  display::Content k; k.ttlMs = 2500;
  if (ev & pet::EV_RAN_AWAY) { go(SceneId::Gone, ms); return; }
  if (scene_ == SceneId::Play) return;   // let the round finish; the face still shows the mood
  if (ev & pet::EV_WON_BACK) { k.title = "YOU'RE BACK"; showMessage(k, ms); return; }
  if (ev & pet::EV_AUTO_UNPAUSE) { k.title = "PAUSE OVER"; showMessage(k, ms); return; }
  if (ev & pet::EV_GREW) { k.title = "I GREW!"; showMessage(k, ms); return; }
  if (ev & (pet::EV_SICK | pet::EV_SICKER | pet::EV_TANTRUM)) {
    if (const char* p = pet::phraseFor(pet::moodOf(s))) { k.title = p; showMessage(k, ms); }
  }
}

uint32_t Scenes::handle(const input::InputFrame& in, pet::PetState& s, int64_t now, uint32_t ms) {
  uint32_t ev = 0;
  auto care = [&](pet::Care c) { cared = true; return pet::applyCare(s, c, now); };
  if (in.any) lastInputMs_ = ms;
  if (s.life == pet::Life::Gone && scene_ != SceneId::Gone && scene_ != SceneId::Message) go(SceneId::Gone, ms);
  const bool stands = scene_ == SceneId::Pet || scene_ == SceneId::Menu;
  m_.setGravity(in.upAngle, stands ? in.upWeight : 0);
  m_.setLook(0, 0);
  if (in.button == ButtonEvent::BothHold && s.life != pet::Life::Gone) {
    const bool paused = pet::togglePause(s, now);
    cared = true;
    display::Content k; k.title = paused ? "PAUSED" : "UNPAUSED"; k.ttlMs = 1500; showMessage(k, ms);
    return ev;
  }
  switch (scene_) {
    case SceneId::Pet:
      if (in.button == ButtonEvent::AClick) go(SceneId::Menu, ms);
      else if (in.button == ButtonEvent::BClick) go(SceneId::NameTag, ms);
      else if (in.button == ButtonEvent::BHold) go(SceneId::Status, ms);
      if (in.tap && m_.hitTest(in.tapX, in.tapY)) {
        m_.poke(in.tapX, in.tapY);
        if (s.life == pet::Life::Returning || s.trust < 30) m_.setLook(in.tapX < CENTER ? 1.0f : -1.0f, 0);   // turns away
      }
      if (in.stroke) { ev |= care(pet::Care::Stroke); happyUntil_ = ms + 3000; haptics::pulse(40, 1, 0, 90); }
      if (in.shake) { ev |= care(pet::Care::Shake); dizzyUntil_ = ms + 2000; }
      break;
    case SceneId::Menu:
      if (in.button == ButtonEvent::AClick) { sel_ = (MenuItem)(((int)sel_ + 1) % (int)MenuItem::Count); full_ = true; enteredMs_ = ms; }
      else if (in.button == ButtonEvent::BClick) {
        switch (sel_) {
          case MenuItem::Feed:   ev |= care(pet::Care::Meal); break;
          case MenuItem::Snack:  ev |= care(pet::Care::Snack); break;
          case MenuItem::Clean:  ev |= care(pet::Care::Clean); break;
          case MenuItem::Meds:   ev |= care(pet::Care::Medicine); break;
          case MenuItem::Lights: ev |= care(pet::Care::Lights); break;
          case MenuItem::Coffee:
            if (!pet::coffeeReady(s, now)) { display::Content k; k.title = "NOT YET"; k.body = "one coffee every 3 hours"; k.ttlMs = 1800; showMessage(k, ms); return ev; }
            ev |= care(pet::Care::Coffee); happyUntil_ = ms + 2000; break;
          case MenuItem::Play:
            if (s.asleep || s.life == pet::Life::Returning) { display::Content k; k.title = "HMPH."; k.ttlMs = 1500; showMessage(k, ms); }
            else { play_.start(ms); go(SceneId::Play, ms); }
            return ev;
          default: break;
        }
        m_.hop(1);
        go(SceneId::Pet, ms);
      } else if (ms - enteredMs_ > 5000) go(SceneId::Pet, ms);
      break;
    case SceneId::Status: case SceneId::NameTag:
      if (in.button == ButtonEvent::BClick || in.button == ButtonEvent::AClick) go(SceneId::Pet, ms);
      else if (scene_ == SceneId::Status && ms - enteredMs_ > 8000) go(SceneId::Pet, ms);
      break;
    case SceneId::Message:
      if (in.button == ButtonEvent::BClick || (msg_.ttlMs && ms - enteredMs_ > msg_.ttlMs))
        go(s.life == pet::Life::Gone ? SceneId::Gone : SceneId::Pet, ms);
      break;
    case SceneId::Gone:
      if (in.button == ButtonEvent::AHold && pet::callBack(s, now)) {
        cared = true;
        display::Content k; k.title = "...HI"; k.body = "win me back"; k.ttlMs = 2500; showMessage(k, ms);
      }
      break;
    case SceneId::Play:
      play_.update(in.tiltX, ms);
      if (play_.done(ms)) {
        ev |= care(play_.won() ? pet::Care::PlayWin : pet::Care::PlayLose);
        display::Content k; k.title = play_.won() ? "YOU WIN" : "NICE TRY"; k.ttlMs = 1500; showMessage(k, ms);
      }
      break;
  }
  return ev;
}

void Scenes::drawMenu() {
  // Six items evenly around the dial, starting at 12 o'clock; the mascot sits inside the ring.
  const int n = (int)MenuItem::Count;
  for (int i = 0; i < n; i++) {
    const float a = -3.14159f / 2 + i * 2 * 3.14159f / n;
    const int x = (int)(CENTER + cosf(a) * 180), y = (int)(CENTER + sinf(a) * 180);
    const bool on = i == (int)sel_;
    if (on) {
      c_->fillSmoothRoundRect(x - 40, y - 15, 80, 30, 15, UI);
      t_.textAt(*c_, Font::Mono15, MENU_LABELS[i], x, y, 0x000000u);
    } else {
      t_.textAt(*c_, Font::Mono15, MENU_LABELS[i], x, y, DIM);
    }
  }
}

Rect Scenes::drawStatus(const pet::PetState& s) {
  const char* names[] = {"FOOD", "ENERGY", "JOY", "CLEAN"};
  const float vals[] = {s.food, s.energy, s.joy, s.hygiene};
  for (int i = 0; i < 4; i++) {
    const int y = 90 + i * 44;
    t_.text(*c_, Font::Mono15, names[i], y, DIM);
    c_->fillSmoothRoundRect(CENTER - 90, y + 12, 180, 8, 4, 0x262626u);
    const int w = (int)(180 * vals[i] / 100);
    if (w > 0) c_->fillSmoothRoundRect(CENTER - 90, y + 12, w, 8, 4, vals[i] < 25 ? ALERT : UI);
  }
  char line[32];
  snprintf(line, sizeof line, "DAY %u  %s", (unsigned)(s.ageMin / 1440 + 1), s.sick ? "SICK" : (s.trust < 30 ? "WARY" : "OK"));
  return t_.text(*c_, Font::Mono15, line, 280, DIM).unite(Rect{(int16_t)(CENTER - 92), 80, 184, 190});
}

Rect Scenes::draw(const pet::PetState& s, uint32_t ms) {
  const pet::Mood mood = pet::moodOf(s);
  Expression e = exprFor(mood);
  if (ms < happyUntil_ && !s.asleep) e = Expression::Happy;
  if (ms < dizzyUntil_) e = Expression::Dizzy;
  if (forcedExpr_ >= 0 && ms < forcedUntil_) e = (Expression)forcedExpr_;
  if (talkFace_ == net::Face::Listening) e = Expression::Listening;
  else if (talkFace_ == net::Face::Thinking) e = Expression::Thinking;
  else if (talkFace_ == net::Face::Speaking) e = Expression::Speaking;
  m_.setExpression(e);
  m_.setGrowth(s.growth);
  const bool content = scene_ == SceneId::NameTag || scene_ == SceneId::Message || scene_ == SceneId::Status;
  if (scene_ == SceneId::Play) m_.setPose(Pose{play_.mascotX(), 379, 0.3f});
  else m_.setPose(content ? POSE_ASIDE : (scene_ == SceneId::Menu ? POSE_MENU : POSE_HOME));
  m_.update(ms);
  // Clearing all 438 KB of PSRAM costs ~12 ms; clear only what this canvas drew last time unless the scene changed.
  if (full_) { needFull_[0] = needFull_[1] = true; full_ = false; }
  const bool fullFrame = needFull_[slot_];
  if (fullFrame) c_->fillScreen(TFT_BLACK);
  else { const Rect clr = drawn_[slot_].padded(4).clippedEven(); if (!clr.empty()) c_->fillRect(clr.x, clr.y, clr.w, clr.h, TFT_BLACK); }
  Rect dirty;
  // Static text and the gone screen are drawn only on full frames: each canvas keeps them, the
  // mascot's clear area never reaches them, and not repainting them keeps the pushed area small.
  if (scene_ == SceneId::Gone) {
    if (fullFrame) {
      drawFootprints(*c_);
      display::Content k; k.label = "HOLD A TO CALL"; k.title = "GONE"; k.body = "gone exploring. come find me?";
      drawContent(*c_, t_, k);
    }
  } else {
    dirty = m_.draw(*c_, ms);
    const Pose p = m_.pose();
    if (mood == pet::Mood::Sleeping || mood == pet::Mood::Paused) dirty = dirty.unite(drawZs(*c_, p, ms));
    if (mood == pet::Mood::Paused) dirty = dirty.unite(drawSleepCap(*c_, p));
    if (mood == pet::Mood::Sick) dirty = dirty.unite(drawSweat(*c_, p, ms));
    if (e == Expression::Happy) dirty = dirty.unite(drawSparkles(*c_, p, ms));
    if (s.messes) dirty = dirty.unite(drawMesses(*c_, p, s.messes));
    if (mood == pet::Mood::Hungry && scene_ == SceneId::Pet) dirty = dirty.unite(drawFoodBubble(*c_, p));
    if (scene_ == SceneId::Menu && fullFrame) drawMenu();
    // Text areas count as dirty so they are cleared and pushed with the frame (live status bars, no
    // anti-aliased text drawn over itself where it meets the mascot's clear area).
    if (scene_ == SceneId::Status) dirty = dirty.unite(drawStatus(s));
    if (scene_ == SceneId::NameTag && fullFrame) {
      display::Content k; k.label = "HELLO, I'M"; k.title = "AKHILESH"; k.body = "Founding Engineer @Charming";
      drawContent(*c_, t_, k);
    }
    if (scene_ == SceneId::Message && fullFrame) drawContent(*c_, t_, msg_);
    if (scene_ == SceneId::Play) dirty = dirty.unite(play_.drawSparks(*c_));
  }
  // The screen currently shows the other canvas's frame, so the push must cover both.
  Rect push = dirty.unite(drawn_[slot_ ^ 1]);
  drawn_[slot_] = dirty;
  if (fullFrame) { push = Rect{0, 0, SCREEN, SCREEN}; needFull_[slot_] = false; }
  return push.clippedEven();
}
}
