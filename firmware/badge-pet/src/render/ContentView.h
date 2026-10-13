#pragma once
#include "TextLayer.h"
#include "../display/Content.h"
namespace render {
// Lays out content in the upper area (y < 300). The mascot must be at POSE_ASIDE.
Rect drawContent(M5Canvas& c, TextLayer& t, const display::Content& content);
}
