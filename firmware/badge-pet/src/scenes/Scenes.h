#pragma once
#include <M5Unified.h>
#include "../pet/PetRules.h"
#include "../pet/Mood.h"
#include "../input/Input.h"
#include "../render/Mascot.h"
#include "../render/TextLayer.h"
#include "../display/Content.h"
#include "PlayGame.h"
namespace scenes {
enum class SceneId : uint8_t { Pet, Menu, Status, NameTag, Message, Gone, Play };
enum class MenuItem : uint8_t { Feed, Snack, Play, Clean, Meds, Lights, Count };
class Scenes {
 public:
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
