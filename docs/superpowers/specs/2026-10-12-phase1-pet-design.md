# Phase 1: Charming badge pet (design spec)

Date: 2026-10-12
Status: draft, awaiting review

## Goal

Turn the WorkOS init() badge (M5Stack StopWatch C152, ESP32-S3) into a Tamagotchi whose creature is the Charming mascot. Phase 1 is badge-only: no network, no AI. It must be polished enough to show off at a booth and robust enough to carry daily.

Later phases (separate specs) add push-to-talk voice and a text chat UI (Phase 2), Charming and personal-tool powers (Phase 3), and a wake word (Phase 4). Phase 1 must leave clean hooks for them: a display API the brain can drive, and the hold-A gesture reserved for push-to-talk.

## Success criteria

1. The badge boots into a living mascot: it breathes, blinks, flickers, looks around, follows tilt, and reacts to taps and strokes, at a steady frame rate with no visible tearing.
2. The pet's needs change over real time, including while the badge is off, and the rules below play out (hunger, sickness, tantrums, running away, coming back).
3. The name tag shows "Akhilesh" / "Founding Engineer @Charming".
4. Pet state survives reboots and battery swaps.
5. The WorkOS browser installer can still restore the conference firmware afterwards, and the attendee profile in the `ffat` partition is never modified.
6. All pet-rule logic is covered by host-side unit tests.

## Visual design

- **Background:** pure black (AMOLED pixels off). Nothing else is drawn on the pet screen except the mascot and its ground shadow.
- **Mascot:** the Charming flame (`charming-mark.svg`), fully procedural. Its outline is sampled into ~72 points used as a spring rest shape. Each frame:
  - deform for breathing, squash/stretch, hops, droop and tip flicker;
  - add spring offsets from pokes and strokes;
  - add "line boil" jitter, re-seeded about every 110 ms;
  - fill coral `#f77448`, stroke with ink `#151410`, add a thin light rim so the dark outline reads on black, and draw the cream highlight stroke.
- **Face:** drawn live. Eyes are vertical ink strokes that blink, look around and follow tilt. Expressions:

  | State | Eyes | Mouth | Extras |
  |---|---|---|---|
  | happy | ^ ^ arcs | smile | blush, sparkles |
  | hungry | tall eyes, looking up | "o" | impatient hops, food bubble icon |
  | sick | flat dashes | squiggle | desaturated body, sweat drop |
  | sleepy / sleeping | closed arcs | none | slow breathing, drifting z's, dimmed |
  | grumpy (low trust) | half-lidded | flat line | turns away when poked |
  | tantrum | squeezed | open | stomps, rapid shaking |
  | dizzy | swirls | none | after a shake |
- **Only the mascot is Charming.** Charming's fonts, paper colors and cards are not used.
- **Text appears only when needed** (talking, name tag, status, menu, messages), then fades out.
  - Headlines: **Silkscreen** (pixel).
  - Body: **Inter**.
  - Small labels: **Geist Mono**.
  - All three are OFL licensed and converted to bitmap fonts at build time.
- **Content never overlaps the mascot.** When a screen shows text or a QR code, the mascot hops down to the lower part of the circle at about 30% scale, and the content uses the upper area.
- **Round safe area:** all text and controls stay inside the visible 466 px circle. Nothing is placed in the framebuffer's corners.

Reference mockups: `mockups/phase1-screens.html` (screen states, older typography), `mockups/type-directions.html` (direction C chosen).

## Screens

| Screen | Content |
|---|---|
| Pet (home) | Mascot only |
| Care menu | Feed, Play, Clean, Medicine, Lights, as Geist Mono labels along the rim, with the selected one highlighted. Closes after 5 s with no input. |
| Status | Food, Energy, Joy and Hygiene bars, plus age in days and mood |
| Name tag | "HELLO, I'M" / **AKHILESH** / "Founding Engineer @Charming" |
| Message | Short text from the pet (and, in Phase 2, from the brain) |
| Gone | Footprints and the note "gone exploring. come find me?" / "HOLD A TO CALL" |
| Paused | Mascot wearing a tiny sleeping cap |

## Controls

| Input | Action |
|---|---|
| Tap the mascot | Poke: spring impulse plus a reaction that depends on mood |
| Stroke across the screen | Pet: raises Joy, short purr vibration |
| A (short) | Open the care menu. Inside the menu: next item |
| B (short) | Open the name tag. Inside the menu: choose. On other screens: back |
| Hold B (1 s) | Status screen |
| Hold A (1 s) | On the Gone screen: call it back. Otherwise reserved for Phase 2 push-to-talk |
| Hold A and B together (3 s) | Toggle pause |
| Tilt | Eyes follow, and the body leans slightly |
| Shake | Wakes it up. Repeated shaking makes it dizzy and lowers Joy |

## Pet rules ("harsh, always recoverable")

**Stats.** Food, Energy, Joy and Hygiene, each 0 to 100. Hidden except on the Status screen.

**Hidden values.**
- **Trust**, 0 to 100: long-term care quality.
- **Discipline**, 0 to 100.
- **Sickness stage**: 0 = healthy, then 1 to 3.
- **Age** in days.
- **Messes**: a count of uncleaned messes.

**Decay rates while awake.**

| Stat | Time from full to empty | Notes |
|---|---|---|
| Food | about 4 h | |
| Joy | about 5 h | Faster while sulking |
| Hygiene | about 8 h | Messes appear periodically after meals |
| Energy | about 6 h | Refills while asleep |

While asleep, every stat except Energy decays at 25% of the normal rate. While sick, Food, Joy and Hygiene decay at twice the normal rate.

**Sickness.** Any of these triggers sickness stage 1:
- any stat at 0 for 1 h or more;
- 2 or more messes left uncleaned for 1 h or more;
- 3 or more snacks within 30 min (bellyache).

Without medicine, the stage goes up by 1 every 3 h, to a maximum of 3. One medicine lowers the stage by 1.

**Tantrums.**
- While awake and healthy, it throws a fake tantrum about once every 2 h on average. The rate rises with spoiling and falls with Discipline.
- **Answering** a tantrum with care raises spoiling. That makes later tantrums more frequent and lowers Discipline.
- **Ignoring** it for 2 minutes, then giving it attention after it calms down, raises Discipline.
- Higher Discipline means fewer tantrums and makes it more willing to play.

**Running away.** It leaves when either:
- it is sick (stage 1 or higher) and gets no care for 8 h; or
- every stat is at 0 for 4 h.

**Coming back (always possible).**
- On the Gone screen, hold A to call it.
- It reappears skittish, with low Trust, and has to be won back through care sessions. **Normally** it takes 3 sessions at least 1 h apart. **If it was gone 48 h or more**, it takes 5.
- Until it is won back it acts grumpy and won't play.
- It never dies and is never reset.

**Trust and personality.**
- Trust rises slowly with good care and quickly answered real needs.
- Trust falls with neglect, running away, and shaking.
- Low Trust shows as the grumpy expression and turning away. In Phase 2, the brain will also read Trust when setting the pet's tone.

**Care actions.**

| Action | Effect |
|---|---|
| Meal | Food +40 |
| Snack | Food +10, Joy +10. Counts toward a bellyache. |
| Play | A 10-second tilt mini-game using the motion sensor. Joy +25 if won, +10 if lost. Costs Energy 10. |
| Clean | Removes all messes, Hygiene = 100 |
| Medicine | Sickness stage -1 |
| Lights | Puts it to sleep early, or wakes it |
| Pet (stroke) | Joy +5, with a cooldown |

**Sleep.**
- It sleeps automatically from 23:00 to 07:00 by the real-time clock (configurable).
- Waking it during those hours makes it grumpy and costs Joy 10.

**Growth.**
- Age goes up by 1 for each day it is kept.
- At day 3, 7 and 14, with good average care, it unlocks a cosmetic change: a slightly larger flame or a new idle animation (stretch, yawn, look around). The mascot's silhouette stays on-brand.

**Pause.**
- Freezes all timers.
- Limited to 10 h per rolling 24 h. When the limit is reached, it unpauses automatically and the badge buzzes.

**Elapsed time.**
- On every wake, the time since the last update is read from the real-time clock.
- That gap is simulated in fixed 1-minute steps with the same rules. Sleep hours are taken into account.
- The result is the same as if the badge had been on the whole time.

All numbers are tuning defaults kept together in one config header.

## Architecture

Firmware is Arduino on ESP32-S3 using M5Unified and M5GFX, built and flashed with `arduino-cli`.

```
firmware/badge-pet/
  badge-pet.ino        setup/loop, wiring only
  src/pet/             pure C++ simulation, no Arduino includes (host-testable)
    PetState.h         stats, hidden values, timestamps
    PetRules.h/.cpp    tick(state, dt, clock), care actions, transitions
    PetConfig.h        all tuning numbers
  src/render/
    Mascot.h/.cpp      spring outline, deformations, line boil, fill and stroke
    Face.h/.cpp        eyes and expressions
    Effects.h/.cpp     sparkles, z's, sweat, footprints, bubbles
    TextLayer.h/.cpp   bitmap fonts, centered and fitted to the round area
    assets/            outline points from the SVG, converted fonts (generated)
  src/input/           buttons, touch gestures (tap vs stroke), IMU tilt/shake
  src/scenes/          scene state machine (Pet, Menu, Status, NameTag, Message, Gone, Paused)
  src/store/           NVS save/load with a version number, RTC time
  src/power/           dim, screen off, timer wake, battery
  src/display/         show({title, body, qr}): content API used by scenes, and in Phase 2 by the brain
tools/
  svg_to_points.py     samples the flame and highlight paths into a C header
  fonts/               convert Silkscreen, Inter and Geist Mono to M5GFX fonts
tests/pet/             host unit tests for src/pet (doctest + clang)
```

**Rendering.**
- One 466×466 RGB565 sprite in PSRAM (about 424 KB).
- The mascot's bounding box is redrawn each frame, and only that dirty rectangle is pushed over QSPI.
- Target 30 fps. Drop to 15 fps when idle to save power.

**Storage.**
- Pet state goes to NVS through `Preferences`: a versioned struct, saved after care actions and every 5 min.
- The `ffat` partition is never mounted for writing, and `FFat.begin(true)` is never called.

**Power.**
- The display dims after 30 s with no interaction and turns off after 2 min.
- While the screen is off, the chip deep-sleeps with a 15-minute timer wake. On each wake it runs the elapsed-time update. If the pet needs attention (sick, running away soon, a tantrum), it buzzes and shows a brief glance, then sleeps again.
- A button or touch wakes the screen. Lift-to-wake is a stretch goal, depending on whether the motion sensor's interrupt is reachable through M5PM1/M5IOE1.

**Partition and recovery.**
- Before the first flash, back up the full 16 MB with `esptool read_flash` to `backups/` (gitignored).
- Always build with the `app3M_fat9M_16MB` partition scheme.

## Toolchain

- `arduino-cli` with the M5Stack board package (3.3.7 or newer), board `M5StopWatch`.
- M5Unified 0.2.15 or newer and M5GFX 0.2.21 or newer, pinned in a project config.
- `esptool` for the backup.
- Upload port is `/dev/cu.usbmodem101`, confirmed at flash time.
- Download mode: hold power for about 2 s until the LED turns green.

## Testing

- **Host unit tests** for `src/pet`:
  - decay rates;
  - sickness triggers and stages;
  - bellyache;
  - tantrum and discipline;
  - running away and the comeback requirements (normal and 48 h or more);
  - pause cap;
  - the elapsed-time update matching a continuously running simulation;
  - sleep-hour handling;
  - care action effects.
- **Build check:** `arduino-cli compile` passes with no warnings in our code.
- **On-device checklist**, run together, because a build passing does not prove hardware behavior:
  - frame rate and smoothness;
  - eyes follow tilt;
  - taps vs. strokes are detected correctly;
  - both buttons, including holds and the two-button hold;
  - vibration;
  - dim, screen off and timer wake;
  - state survives a reboot;
  - name tag text fits the circle;
  - the WorkOS installer still recognizes the badge afterwards (optional, last).

## Out of scope for Phase 1

Wi-Fi, voice, the brain service, the text chat UI, AI-pushed content (the display API exists but only local code uses it), the wake word, and reading the conference profile from `ffat`. The name tag text is set in config.
