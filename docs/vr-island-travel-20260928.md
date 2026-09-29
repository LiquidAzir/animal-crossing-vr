# Island travel crash investigation — September 28, 2026

The Quest headset recorded a native crash at 18:29:09 during Kapp'n's boat trip.
Its build ID (`d67ae8da67f0abf1e8ed0bc971d36c1601604c08`) matches the installed
version 11 library. Symbolication resolves `aHUS_actor_draw+330` to
`src/actor/ac_house_draw.c_inc:279`: the house calls `draw_shadow_proc` through a
null background-item clip. The fault address is `0x4`.

Island travel is already present in both versions, including the native island
data, boat sequence, summer climate, islander, cottage, and return route. This
failure comes from its interaction with VR's persistent outdoor actors.

`aBTD_change_season` calls `mBI_change_bg_item` to replace seasonal scenery.
Replacement is deferred: the old actor's destructor clears its shared clip during
the actor update, and the new actor is created on a later update. Distant houses
remain drawable in VR during that gap. The original visibility rules normally
hide those houses while the boat travels.

The shared shadow dispatch now checks that the provider and callback are ready.
Actor geometry still draws; only the optional shadow is skipped during the gap,
and regular shadows resume with the replacement provider. All callers use the
same check so changing the town layout cannot merely move the crash to another
building. The normal deferred actor lifecycle is retained.

Related NPC pitfall requests wait for their provider before starting their
one-shot action. This preserves the action instead of entering it with no
background service.

## Return-trip lifecycle

Whole-town residency also retained a completed boat trip indefinitely, leaving
Kapp'n in his out-of-service state. An anchored, completed trip now uses the
original departure-acre lifetime: leaving the destination acre lets the boat,
passenger, and demo retire and be recreated for the opposite direction. Active
trips and waiting docks retain the existing residency behavior.

New boat creation checks whether its authored acre and the player's current acre
are in the same town/island region. This prevents the whole-town sweep from
recreating the old town dock first and occupying the single return-boat slot.
The check applies only when town residency is enabled; stock behavior is retained.

## Local evidence

Original Quest crash, application logs, and the exact version 11 native library
are retained outside the repository in
`../research/island-crash-20260928`. A fresh export of the Quest save is under
`../backups/quest-save-before-v12-20260928`.

## Verification

- Both production builds passed.
- Provider lifecycle: 152 checks passed in each Windows source tree and on the
  Quest ARM32 processor. Tests cover all 16 seasonal profile pairs, absent and
  restored callbacks, unchanged shadow arguments, and retryable NPC actions.
- All 36 exterior/event shadow calls use the guarded dispatch.
- Existing fountain submissions: 312 checks passed per tree.
- Boat return lifecycle: 68 new checks and 26 existing dock checks passed per
  tree. The new test fails against the previous implementation, and covers both
  directions, active trips, rejected-boat cleanup, and collision ownership.
- An isolated PC fixture using real game assets, actors, and rendering reproduced
  the previous null-shadow crash (a reserve sign was first in its actor order).
  The updated executable passed 29 checks through town-to-island climate and
  return-climate transitions. Fourteen houses continued drawing during the two
  provider-gap frames in each direction; shadows recovered and GL reported no
  errors. Evidence: `pc/build32/island-transition-native-20260928-184714` in PC.
- The Quest graphics harness compiled, but its run was deferred while another
  app was active. The user chose to finish the update and test later.

The native render test exercises the crashing scenery/controller lifecycle; it
does not navigate a complete boat voyage or establish island gameplay coverage.
A physical round trip, island walking/interiors, and save/reload remain on the
playtest checklist.

## Installation

The PC executable was installed with SHA256
`05BA79E5CC8901C20A372CF7A36EFC57E2CBC15BE53CB9796A476E014472CBE3`.
The 23 other installed files retained their hashes. Receipt:
`pc/build32/island-install-20260928.json`; the prior executable is in the PC
workspace's `Backups/island-travel-20260928` directory.

Quest development APK version 12 was installed with SHA256
`f4048284cf68699cdc77a8035619bf55a849c631025600dd3c031fb4613f6ce5`.
The ROM, settings, and save retained their pre-install hashes; a fresh save
export and previous APK are preserved. Receipt: `../research/install-v12.json`.
The installed Quest app was not launched, and no public release was published.
