#pragma once
#include <M5Unified.h>
#include "Geometry.h"
#include "Mascot.h"
namespace render {
Rect drawSparkles(M5Canvas& c, const Pose& p, uint32_t ms);
Rect drawZs(M5Canvas& c, const Pose& p, uint32_t ms);
Rect drawSweat(M5Canvas& c, const Pose& p, uint32_t ms);
Rect drawMesses(M5Canvas& c, const Pose& p, uint8_t count);
Rect drawFoodBubble(M5Canvas& c, const Pose& p);
Rect drawFootprints(M5Canvas& c);
Rect drawSleepCap(M5Canvas& c, const Pose& p);
}
