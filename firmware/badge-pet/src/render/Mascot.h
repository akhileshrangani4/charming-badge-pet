#pragma once
#include <M5Unified.h>
#include "Geometry.h"
#include "Face.h"
namespace render {
struct Pose { float cx, cy, scale; float rot = 0; };   // rot: radians about the screen center
constexpr Pose POSE_HOME{CENTER, 246, 0.6f, 0};
constexpr Pose POSE_ASIDE{CENTER, 379, 0.3f, 0};
constexpr Pose POSE_MENU{CENTER, 236, 0.4f, 0};   // small enough to sit inside the menu ring
constexpr int MASCOT_N = 72;
class Mascot {
 public:
  void setExpression(Expression e) { expr_ = e; }
  Expression expression() const { return expr_; }
  void setPose(const Pose& p, bool instant = false) { target_ = p; if (instant) pose_ = p; }
  Pose pose() const { return pose_; }
  void setLook(float x, float y) { lookTX_ = x; lookTY_ = y; }
  void setGrowth(uint8_t g) { growth_ = g; }
  // Stand on real gravity: angle of "down" from screen-down, weight 0 (upright) .. 1 (fully follow).
  void setGravity(float angle, float weight) { gravAngle_ = angle; gravWeight_ = weight; }
  void poke(int x, int y);
  void hop(float strength) { hop_ = strength; }
  void update(uint32_t ms);
  Rect draw(M5Canvas& c, uint32_t ms);
  bool hitTest(int x, int y) const;
 private:
  void deform(float x, float y, int i, uint32_t ms, float& ox, float& oy) const;
  uint32_t rnd() { seed_ = seed_ * 1664525u + 1013904223u; return seed_ >> 8; }
  float offX_[MASCOT_N] = {}, offY_[MASCOT_N] = {}, velX_[MASCOT_N] = {}, velY_[MASCOT_N] = {};
  Pose pose_ = POSE_HOME, target_ = POSE_HOME;
  Expression expr_ = Expression::Idle;
  float gravAngle_ = 0, gravWeight_ = 0;
  float lookX_ = 0, lookY_ = 0, lookTX_ = 0, lookTY_ = 0, wanderX_ = 0, wanderY_ = 0, hop_ = 0;
  uint32_t nextWander_ = 0, nextBlink_ = 1500, blinkStart_ = 0, lastMs_ = 0, boil_ = 1, seed_ = 12345;
  bool blinking_ = false;
  uint8_t growth_ = 0;
  Rect bounds_;
};
}
