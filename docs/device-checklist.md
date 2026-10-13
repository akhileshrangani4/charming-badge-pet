# Phase 1 device checklist

Run with the badge on USB and `scripts/monitor.sh` open. Mark each item pass/fail with a note.

- [x] Boots to the mascot only, on black. No text after the first 2.5 s, unless it has a need.
- [x] Smooth motion (about 40-50 fps after the double-buffer change).
- [x] Gravity: the mascot stands upright on real "down" when the badge is rotated.
- [x] Tap on the mascot pokes it. A stroke purrs and makes it happy.
- [x] Shake makes it dizzy.
- [x] A opens the dial menu, A cycles six items, B picks.
- [x] B shows the name tag: HELLO, I'M / AKHILESH / Founding Engineer @Charming.
- [x] Holding B shows status. Holding both for 3 s toggles pause.
- [x] Play: tilt to catch sparks; plays repeatedly without crashing.
- [x] Dims at 30 s, sleeps at 2 min, A wakes it with state intact.
- [ ] Each expression looks right (Idle, Happy, Hungry, Sick, Sleep, Sleepy, Grumpy, Tantrum, Dizzy, Sad). Force states with `F <minutes>`.
- [ ] Night: `T` to 22:59, wait a minute, it falls asleep with z's and dims.
- [ ] Runs away after neglect (`F 2000`). Hold A to call it back. Three care sessions an hour apart (`F 60` between) win it back.
- [ ] 15-minute timer wake buzzes twice when it needs attention.
- [ ] Battery: note the percentage drop per hour over a normal day.
- [ ] Installer compatibility (last, optional): https://workos.com/init/badge/install in Chrome recognizes the badge. Only restore if you want the conference firmware back; reflash the pet with `scripts/flash.sh`.

Serial commands (115200 baud): `T YYYY-MM-DD HH:MM` sets the clock (local time; the conference firmware left it on UTC), `S` prints state, `F <min>` fast-forwards, `R YES` starts a new pet.
