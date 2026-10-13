#pragma once
#include <M5Unified.h>
#include "../pet/PetRules.h"
#include "../pet/Mood.h"
#include "../input/Input.h"
#include "../render/Mascot.h"
#include "../render/TextLayer.h"
#include "../display/Content.h"
#include "PlayGame.h"
#include "../net/Protocol.h"
#include <string.h>
namespace scenes {
enum class SceneId : uint8_t { Pet, Menu, Status, NameTag, Message, Gone, Play };
enum class MenuItem : uint8_t { Feed, Snack, Play, Clean, Meds, Lights, Coffee, Count };
class Scenes {
 public:
  // Push-to-talk overlay: the server's face wins over the mood face while a conversation runs.
  void setTalkFace(net::Face f) { talkFace_ = f; }
  void showCard(const net::Card& c, uint32_t ms) {
    strncpy(cardTitle_, c.title, sizeof cardTitle_ - 1); strncpy(cardBody_, c.body, sizeof cardBody_ - 1); strncpy(cardQr_, c.qr, sizeof cardQr_ - 1);
    display::Content k; k.title = cardTitle_; k.body = cardBody_[0] ? cardBody_ : nullptr; k.qr = cardQr_[0] ? cardQr_ : nullptr; k.ttlMs = cardQr_[0] ? 20000 : 6000;
    showMessage(k, ms);
  }
  void setBattery(int level, bool charging) { battery_ = level; charging_ = charging; }
  void showStatus(const char* title, uint32_t ms) { display::Content k; k.title = title; k.ttlMs = 2000; showMessage(k, ms); }
  Scenes(M5Canvas& c, render::TextLayer& t, render::Mascot& m) : c_(&c), t_(t), m_(m) {}
  // Double buffering: draw() renders into canvas `slot` (0/1); each canvas still holds the frame from two draws ago.
  void setTarget(M5Canvas& c, int slot) { c_ = &c; slot_ = slot; }
  uint32_t handle(const input::InputFrame& in, pet::PetState& s, int64_t nowMin, uint32_t ms);
  void onEvents(uint32_t events, const pet::PetState& s, uint32_t ms);
  render::Rect draw(const pet::PetState& s, uint32_t ms);
  void showMessage(const display::Content& k, uint32_t ms);
  SceneId current() const { return scene_; }
  bool cared = false;   // set whenever handle() applied care; App saves and clears it
  // Debug (serial G/X commands): jump to a scene, force an expression for a few seconds.
  void debugGo(SceneId id, uint32_t ms) { go(id, ms); }
  void debugExpression(int e, uint32_t untilMs) { forcedExpr_ = e; forcedUntil_ = untilMs; }
 private:
  net::Face talkFace_ = net::Face::Idle;
  int battery_ = -1; bool charging_ = false;   // -1 until the first reading
  render::Rect drawBattery();
  char cardTitle_[24] = {0}, cardBody_[64] = {0}, cardQr_[121] = {0};
  void go(SceneId id, uint32_t ms) { if (scene_ != id) { scene_ = id; full_ = true; } enteredMs_ = ms; }
  render::Rect drawn_[2];      // what each canvas last drew
  bool needFull_[2] = {true, true};
  int slot_ = 0;
  void drawMenu(); render::Rect drawStatus(const pet::PetState& s);
  M5Canvas* c_; render::TextLayer& t_; render::Mascot& m_;
  SceneId scene_ = SceneId::Pet; MenuItem sel_ = MenuItem::Feed;
  display::Content msg_; char msgTitle_[24] = {0};
  PlayGame play_;
  uint32_t enteredMs_ = 0, happyUntil_ = 0, dizzyUntil_ = 0, lastInputMs_ = 0;
  bool full_ = true;
  int forcedExpr_ = -1; uint32_t forcedUntil_ = 0;
};
}
