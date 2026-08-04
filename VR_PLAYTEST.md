# VR Playtest Checklist

A guided ~25-minute route that touches every system with VR-specific risk.
Run it once per release when you can. Mark each item pass/fail; for fails,
one line on what you saw is enough (plus `vr_log.txt` — it now records
frame-timing summaries every 30 seconds automatically).

Setup: headset on, SteamVR running, `--verbose` not needed (vr_log always
writes). Start in the **diorama** (third-person) view.

## 1. Boot & menus (panel-only frames)

- [ ] Title screen and save select appear on the floating panel, readable
- [ ] Start prompt responds to A (right trigger)
- [ ] No SteamVR grid flash between screens

## 2. Diorama basics

- [ ] Head tracking: lean in/out, look around — village tracks solidly,
      horizon stays level
- [ ] X+Y recenters cleanly while looking straight ahead
- [ ] Camera cuts (walking between acres if borderless off, entering areas)
      are instant snaps, no swooping

## 3. First-person entry

- [ ] Toggle FP with left grip + Y standing in an open field
- [ ] World becomes life-size; you're at villager eye height (if wrong:
      `fp_eye_height`)
- [ ] Your body is gone but your round shadow is on the ground
- [ ] Toggle back and forth a few times — no residue either way

## 4. Movement & turning (FP)

- [ ] Stick-forward walks exactly where you're looking
- [ ] **Stick-right snap-turns RIGHT** (this validates the angle math)
- [ ] Run (hold B/left trigger) — speed feels right, no camera judder
- [ ] Walk all four compass directions across an acre boundary
      (borderless): terrain streams, no pop-in directly ahead
- [ ] Cross a bridge; walk the beach; walk along a cliff edge

## 5. Doors & interiors (scripted-camera fallbacks)

- [ ] Enter your house: camera falls back for the door transition, FP
      resumes inside
- [ ] Interior FP: room at life scale, no clipping through walls when
      leaning
- [ ] Exit door works the same in reverse
- [ ] Train station + platform, museum, shop, post office all enter/exit
      cleanly

## 6. Dialogue (FP)

- [ ] Talk to a villager: view snaps to face them, they're at eye level,
      text panel readable below
- [ ] Conversation ends: view stays where you left it, pitch eases level
- [ ] Talk while deliberately facing away first — snap still finds them
- [ ] Nook's shop: buy something; **sell something** (the camera pans to a
      point, not a person — view should face the counter area, not spin)
- [ ] Toggle FP OFF then ON mid-conversation — re-faces the villager

## 7. Tools (FP — the fun part)

- [ ] Fishing rod: equip (grip-hold + stick for D-pad), cast with A — rod
      visible and bending in front of you, bobber flies and floats
- [ ] Hook a fish: bite → A → catch animation (falls back to game camera
      for the trophy pose — expected)
- [ ] Net: visible in hand; swing catches a bug on a tree/ground
- [ ] Shovel: dig a hole, bury something, dig it back up
- [ ] Axe: chop a tree three times (collision aligned with view)
- [ ] Watering can / umbrella in rain if available: visible and animated
- [ ] Motion swing (if enabled): a firm swing of the right controller
      triggers the tool exactly once per swing

## 8. Events & edge scenes (fallback correctness)

- [ ] Any demo/cutscene moment (train arrival, event NPC): camera takes
      over, player model VISIBLE during it, FP resumes after
- [ ] Pelly/Pete mail moments, Resetti if you can bait him — no stuck
      camera, no invisible player in third-person shots
- [ ] K.K. Slider (Saturday night) if applicable

## 9. Panel & 2D (regression)

- [ ] Inventory, map, and pattern screens all render on the panel
- [ ] NES game (if you have one) plays on the panel at 60 Hz
- [ ] Pause menu (Start) shows and navigates; **F5 and grip+Y do nothing
      while paused**

## 10. Endurance (20-30 min free play)

- [ ] No drift: after 20 min, recenter still puts you dead-center
- [ ] No progressive stutter (check `vr_log.txt` timing lines: `gpu=` should
      stay well under your headset's frame budget — 11.1 ms at 90 Hz;
      `drops=` should stay at or near 0 per interval)
- [ ] Quit from the pause menu: clean exit, SteamVR releases, save intact
      on next boot

## Notes / comfort observations

- World scale feel (diorama / FP):
- Eye height feel:
- Any nausea moments (what were you doing):
- Anything visually broken (screenshot key: SteamVR System button menu):
