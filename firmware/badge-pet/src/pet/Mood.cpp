#include "Mood.h"
#include "PetRules.h"
namespace pet {
Mood moodOf(const PetState& s) {
  if (s.life == Life::Gone) return Mood::Gone;
  if (s.paused) return Mood::Paused;
  if (s.asleep) return Mood::Sleeping;
  if (s.sick) return Mood::Sick;
  if (s.tantrum) return Mood::Tantrum;
  if (s.food < 25) return Mood::Hungry;
  if (s.messes > 0 || s.hygiene < 25) return Mood::Dirty;
  if (s.energy < 20) return Mood::Sleepy;
  if (s.life == Life::Returning || s.trust < 30 || (s.wokeThisNight && isNight(s.lastMin))) return Mood::Grumpy;
  if (s.joy < 25) return Mood::Sad;
  if (s.joy > 70) return Mood::Happy;
  return Mood::Content;
}
// Spec: buzz when it is sick, about to run away, or throwing a tantrum. At night only a runaway
// within two hours is worth waking someone for.
bool needsAttention(const PetState& s) {
  if (s.life == Life::Gone || s.paused) return false;
  const bool leavingSoon = (s.sick && s.sinceCareMin >= 8 * 60 - 120) || s.allZeroMin >= 4 * 60 - 120;
  if (isNight(s.lastMin)) return leavingSoon;
  return s.sick || s.tantrum || leavingSoon;
}
const char* phraseFor(Mood m) {
  switch (m) {
    case Mood::Hungry:  return "FEED ME";
    case Mood::Sick:    return "I FEEL SICK";
    case Mood::Dirty:   return "CLEAN UP?";
    case Mood::Sleepy:  return "SO TIRED";
    case Mood::Grumpy:  return "HMPH.";
    case Mood::Sad:     return "PLAY WITH ME";
    case Mood::Tantrum: return "NOW!!";
    default:            return nullptr;
  }
}
}
