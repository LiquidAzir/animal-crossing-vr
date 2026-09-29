# VR Playtest Checklist

## September 28 follow-up

- [ ] Cast into a river and the sea in first-person VR. The view stays first
      person when the lure lands, while waiting for a bite, during reeling,
      after a cancelled cast, and through the catch announcement.
      Toggle first person off/on while waiting: each view still works.
- [ ] With no item equipped, look at both hands against the ground, walls,
      and outdoor scenery. Both follow the controllers in each eye. Move a
      hand behind a nearby surface: that surface should cover it. Check
      entering/exiting a shop, equipping a tool, menus, and lost tracking.
- [ ] Inspect Nook's displayed rugs from the front, back, and oblique angles
      while moving the head. Their two sides remain visible without the
      competing checker/stripe pattern. Other shop items and repaired
      building backs should look as before.

Implementation and local checks: [Fishing, hands, and rug fixes](docs/vr-fishing-hands-20260928.md).

## September 27 follow-up

- [ ] With both controllers awake, stow every held item: no hand markers should
      appear by default. Equip a tool and confirm its tracking still works.
- [ ] Catch a bug, catch a fish, and dig up a fossil while facing different
      directions. During each announcement, the item appears ahead and slightly
      below gaze. Turn/tilt the head: it remains visible. Check tiny-catch arrows,
      putting items away, and the full-pockets inventory/exchange flow.
- [ ] Circle Nook's Cranny in summer and winter: one original door/sign, fitted
      timber back walls and closed roof edges. Enter and exit normally.

Implementation and local checks: [VR follow-up notes](docs/vr-followup-20260927.md).

## September 20 follow-up

- [ ] Hold run and move forward at every compass heading, including head turns
      and snap turns. Check for repeated skids; deliberate reversals should skid.
- [ ] Hold each net straight and verify its visible direction and catch position.
      Check that the axe, shovel, and rod grips still feel the same.
- [ ] Circle houses, shops, museum, police station, and wishing well; inspect
      rear fill, roof alignment, doors, night windows, and winter variants.
- [ ] Inspect all five rock backs and check that hitting rocks still works.
- [ ] Approach/leave the dock across acres: Kapp'n should stay with his boat.
      Talk, make an island round trip, and enter/exit a building. Check that
      ordinary villagers still appear after crossing several acres.

Implementation and automated results: [VR polish notes](docs/vr-polish-20260920.md).

A guided ~25-minute route that touches every system with VR-specific risk.
Run it once per release when you can. Mark each item pass/fail; for fails,
one line on what you saw is enough (plus `vr_log.txt` — it now records
frame-timing summaries every 30 seconds automatically).

Setup: headset on, SteamVR running, `--verbose` not needed (vr_log always
writes). Start in the **diorama** (third-person) view.

## VR settings shortcut

- [ ] While playing, click both sticks together (either click may lead slightly):
      VR Settings opens once without opening the game Start/Z screen.
- [ ] Move the left stick to navigate, A/right trigger to select, and B/left
      trigger to resume. Held controls must not repeat or leak into gameplay.
- [ ] Change hands, turning, motion swings, and volume; Apply, Resume, and reopen.
      Confirm the values persist after restarting. Other settings stay unchanged.
- [ ] Make an unapplied edit and press B: Keep editing/Discard behaves correctly.
- [ ] Turn/lean while paused: the world stays head-tracked; gameplay is frozen.
      Individually click each stick afterward to confirm Start/Z still works.

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

### Optional floating hands

- [ ] In Settings → Gameplay, verify VR empty hands is Off by default, then enable it. With no item
      equipped, both small hands follow controller position and rotation.
- [ ] Compare On/Off while standing, walking, running, turning, and recentering.
      Check comfortable size, thumb direction, and alignment at the grip.
- [ ] Equip/stow the net, rod, shovel, axe, umbrella, and other held items.
      Both hands hide while equipped; tool angles and controls stay the same.
- [ ] Dialogue, catching/digging/pickups, inventory, map, pause, entering a
      building, and title screens show no extra hands over their UI.
- [ ] Hide/disconnect one controller: only its hand disappears and returns
      when tracking recovers. No frozen hand remains after headset tracking loss.
- [ ] Move a hand behind a wall or prop: nearby world geometry occludes it.
      Toggle first person off: floating hands disappear from the diorama.

### Movement

- [ ] Stick-forward walks exactly where you're looking
- [ ] **Stick-right snap-turns RIGHT** (this validates the angle math)
- [ ] Run (hold B/left trigger) — speed feels right, no camera judder
- [ ] Walk all four compass directions across an acre boundary
      (borderless): terrain streams, no pop-in directly ahead
- [ ] Cross a bridge; walk the beach; walk along a cliff edge

## 5. Doors & interiors

- [ ] Enter your house: first person continues through the door transition
      and into the room
- [ ] Interior FP: room at life scale, no clipping through walls when
      leaning
- [ ] Exit door works the same in reverse
- [ ] Train station + platform, museum, shop, post office all enter/exit
      cleanly

## 6. Dialogue (FP)

- [ ] Talk to a villager: view snaps to face them, they're at eye level,
      dialogue sits higher and their face is easier to see
- [ ] Nameplate, message text, continue arrow, and choices stay aligned
      throughout dialogue opening/closing. Repeat several conversations:
      no upward drift. Check shop prompts with many choices too.
- [ ] Conversation ends: view stays where you left it, pitch eases level
- [ ] Talk while deliberately facing away first — snap still finds them
- [ ] Nook's shop: buy something; **sell something** (the camera pans to a
      point, not a person — view should face the counter area, not spin)
- [ ] Toggle FP OFF then ON mid-conversation — re-faces the villager

## 7. Tools (FP — the fun part)

- [ ] Fishing rod: equip (grip-hold + stick for D-pad), cast with A — rod
      visible and bending in front of you, bobber flies and floats
- [ ] Hook a fish: bite → A → catch animation and message stay first person,
      with the same gaze and world scale before, during, and after
- [ ] Net: visible in hand; swing catches a bug on a tree/ground
- [ ] Hold the net handle upright and point naturally: hoop opening faces
      forward. Rotate the wrist: it follows without sideways shaft drift.
      Check both normal and golden nets, readying, swinging and putting away.
- [ ] Shovel: dig a hole, bury something, dig it back up
- [ ] Dig up a fossil and catch a bug: remain first person through the
      announcement and putting the item away. Look around during the message.
      Try with full pockets as well: choices and inventory still work.
- [ ] Move the controller quickly during a catch/discovery announcement:
      no skipped text; deliberate A still advances it. After closing the
      message, slow the hand then swing again: normal tool use resumes.
- [ ] Axe: chop a tree three times (collision aligned with view)
- [ ] Watering can / umbrella in rain if available: visible and animated
- [ ] Motion swing (if enabled): a firm swing of the right controller
      triggers the tool exactly once per swing
- [ ] Stand still facing one way, turn your head 90 degrees, then 180
      degrees: axe/shovel use the target you are looking toward. Repeat
      with A and with a gesture, including the golden tools if available.
- [ ] Face away from water, look toward it, and cast without taking a step.
      The bobber lands ahead of your gaze. Look away during the wind-up:
      the committed cast keeps its destination; hooking/reeling still work.
- [ ] Hold A to ready the net, turn your head, then release. Repeat after
      creeping sideways. The net remains on the controller and catches
      where you swing; regular bug catches and blocked swings still work.
- [ ] With an axe, look away from a nearby solid actor toward a tree before
      swinging: the old facing must not cause a reflection behind your view.
- [ ] Sustain one fast arm movement: one use only. Slow the hand and swing
      again: another use. Normal reaching and slow movements do nothing.
- [ ] Swing while paused/in inventory/in dialogue: no menu selections or
      skipped text. Physical A/B still navigate normally.
- [ ] Briefly obscure controller tracking, then recover: no stale gesture.
      A fresh deliberate swing works. The same applies after pausing.
- [ ] Hold left grip and partially deflect the stick: no walking. A full
      deflection still changes tools; releasing the grip restores movement.
- [ ] While holding the tool-selection grip, move the right hand quickly:
      no accidental tool use. Physical A still works; deliberate gestures
      work again after releasing the grip and slowing the hand.
- [ ] A refused axe/shovel request must not snap the body toward a new gaze.
      Accepted follow-up actions should still aim toward the gaze.
- [ ] Repeat ordinary tool use in diorama VR and flat mode: facing, input,
      target selection, catches, and fishing still follow the original rules.

## 8. Events & edge scenes (fallback correctness)

- [ ] Take Kapp'n's boat to the island: the scenery/season switch must finish
      without a crash. Walk across both island acres and enter/exit the cottage
      and islander's house; confirm terrain, actors, and normal interaction.
- [ ] After leaving the arrival acre, return to the dock and board for town.
      Check the return scenery switch, then leave the town dock acre and revisit:
      Kapp'n should offer another trip. Repeat on PC VR and native Quest.
- [ ] After a successful round trip, save/quit and reload; town and island data
      should persist. Repeat seasonal transitions from winter if available.
- [ ] Scripted event/cutscene moment (train arrival, event NPC): camera takes
      over, player model VISIBLE during it, FP resumes after
- [ ] Pelly/Pete mail moments, Resetti if you can bait him — no stuck
      camera, no invisible player in third-person shots
- [ ] K.K. Slider (Saturday night) if applicable

## 9. Panel & 2D (regression)

- [ ] Inventory, map, and pattern screens all render on the panel
- [ ] Desktop dialogue and diorama VR keep their previous placement.
      F5-off catches still show the original third-person presentation.
- [ ] NES game (if you have one) plays on the panel at 60 Hz
- [ ] Pause menu (Start) shows and navigates; **F5 and grip+Y do nothing
      while paused**

## Outdoor world regression

- [ ] Walk around each house: one roof and porch, no duplicated rear doors
      or intersecting facades. Rear walls are filled; repeat in winter.
- [ ] Inspect plaster, timber, and player-home backs in daylight and dusk:
      rear colors and trim suit the building, with no featureless gray panels
      or giant stretched bands. Roof paint/palette variations still work.
- [ ] Circle Able Sisters and the police station: one original entrance/sign,
      solid rear surfaces, no overlapping roof or reflected signage. Enter/exit
      both normally; check their window lighting after dark and in winter.
- [ ] Circle the fountain: one tree and basin, closed bark/stone backs, normal
      water/splash animation, and no branches crossing the front basin.
      Bubble scrolling should stay stable while the player and other objects
      enter/leave view, in both summer and winter.
- [ ] Circle the museum and post office in summer and winter: siding matches
      the original stonework/plaster, roof undersides have no gaps, and there
      are no duplicate facades. Check dusk lighting for panels poking through
      walls; enter/exit and use the post office normally.
- [ ] Visit the ramp acre from both elevations and sides: the triangular
      gaps below the cliff lip are closed, and walking/collisions are normal.

- [ ] Turn through 360 degrees and look overhead: no seams, stretched poles,
      or clouds attached to the headset. Leaning produces no sky parallax.
- [ ] First-person, diorama, and desktop horizons align with the world.
- [ ] Buildings, foliage, ocean, and UI draw correctly in front of the sky.
- [ ] Interiors and inventory/map previews retain their original backgrounds;
      entering/exiting them restores the outdoor sky correctly.
- [ ] Pausing freezes the sky; day/night and rain/snow use appropriate colors.
- [ ] `skybox = 0` restores the stock background after restarting.

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
