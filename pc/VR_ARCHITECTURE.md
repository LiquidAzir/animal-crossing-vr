# VR Mod Architecture (SteamVR / Quest 3 PCVR)

Native stereo VR support for the Animal Crossing PC port, targeting Meta Quest 3
over Link / Air Link / Virtual Desktop / Steam Link. All paths present the HMD
as a SteamVR device, so the mod uses **OpenVR** (the only PCVR API with
first-class 32-bit x86 support — SteamVR's OpenXR runtime has no win32 build).

## Rendering model

The port renders in three tiers: game code builds N64 display lists → `emu64`
interprets them into GX calls → `pc_gx` translates GX to OpenGL 3.3. One call —
`emu64_taskstart(Gfx_list05)` in `graph_task_set00()` (src/graph.c) — draws the
entire frame from an already-built display list. VR re-interprets that list
once per eye:

```
JW_BeginFrame                      -> pc_vr_frame_begin(): PollNextEvent, UpdateActionState,
                                      WaitGetPoses (paces at HMD rate), compute per-eye matrices
for eye in {L, R}:
    pc_vr_begin_eye(eye)           -> bind eye FBO, clear, set target dims for viewport scaling
    emu64_init / emu64_taskstart / emu64_cleanup     (same display list, per-eye matrices)
pc_vr_compose_and_submit()         -> draw UI panel into each eye, glFlush,
                                      IVRCompositor::Submit(L), Submit(R)
JW_EndFrame -> VIWaitForRetrace    -> mirror blit (left eye -> window), SwapWindow
                                      (swap interval 0, frame limiter bypassed in VR)
```

The game's own delta-time system (`graph->dt`) means logic ticks correctly at
whatever rate WaitGetPoses paces us to (72/80/90/120 Hz).

## Matrix injection (in pc_gx / emu64 boundary)

emu64 keeps a clean split: `projection_mtx` (pure GX projection, perspective or
ortho), `position_mtx` (the game's lookAt view V, captured from the
`G_MTX_PROJECTION|G_MTX_MUL` command), and per-object `GXLoadPosMtxImm(V * M)`.

- **Projection**: when `projection_type == GX_PERSPECTIVE` and a VR scene pass
  is active, `pc_gx` uploads the HMD eye projection (from
  `GetProjectionMatrix`, near 0.05 m, far ≥ game far × scale) instead of the
  game matrix. The widescreen hor+ hack is disabled in VR.
- **View**: emu64 notifies `pc_vr_notify_game_view(V)` whenever `position_mtx`
  is set. pc_vr computes, per eye:

  ```
  X_eye = Eye⁻¹ · Head⁻¹ · Scale · Anchor · V⁻¹
  ```

  where `Anchor` is a *leveled* (yaw-only) camera frame at the game camera's
  position — the horizon stays level with real gravity, preserving the game's
  downward camera tilt as geometry below your gaze (diorama view). `Scale`
  maps game units to meters (default 0.01: one 40-unit tile = 40 cm).
  At flush, `pc_gx` uploads `X_eye · posmtx` as the position matrix.
- **Camera cuts** (acre scroll ends, doors, events) snap instantly — no
  interpolation, which is the comfortable choice.

## 2D / UI layer

Ortho-projection draws (HUD, dialogue, menus, wipes, NES games) render into a
single 4:3 **UI FBO** with their original matrices, drawn only during the left
eye's interpretation pass (dropped during the right pass to avoid double
blending; state processing still runs). After both eye passes, the UI texture
is composited into each eye as a slightly curved-feel flat panel fixed in the
anchor frame (~2 m ahead), so head movement lets you look around it. Frames
that are entirely ortho (main menu) appear as just the panel.

## Outdoor sky

`pc_sky.cpp` draws a procedural sky once per world pass, before the first
perspective batch. The lookAt notification supplies its game view; VR uses
the rotation of `view_correction * game_view` and each eye's asymmetric
projection to reconstruct world rays. Removing translation and scale places
the sky at infinity. The renderer restores its GL state and never writes
depth. `m_kankyo.c` supplies outdoor/submenu eligibility, game time, and the
weather transition. See [skybox notes](../docs/skybox.md).

## Culling

emu64 performs CPU vertex culling against the game's narrow (FOV ~20°)
frustum. In VR passes the cull bounds are widened (the mechanism already
exists for widescreen at emu64.c's cull test) so head rotation doesn't reveal
missing geometry.

## Input

SteamVR Input action manifest (`vr_actions/`) mapping Touch controllers to the
GameCube pad, merged in `pc_pad.c` alongside keyboard/SDL gamepad:
left stick → stick, right stick → C-stick, right trigger → A, left trigger → B,
A/B → A/B, X/Y → X/Y, left grip → L, right grip → R, right stick click → Z,
left stick click → Start. While the left grip is held the left stick acts as
the D-pad (tool switching) instead of movement. X+Y together recenters the
seated origin (an unbound `recenter` action also exists for rebinding). Users
can rebind everything in SteamVR's controller settings. GC rumble mirrors to
controller haptics.

## First person & motion tools

`pc/src/pc_fp_camera.c` runs the first-person camera as a small state machine
hooked into `Camera2_SetView` (src/game/m_camera2.c): when active it replaces
the game's eye/at/up with a first-person view at the player's head, redirects
stick movement to the view yaw (`getCamera2AngleY`), and hides the player
model (src/game/m_player_draw.c_inc) while keeping its shadow. NORMAL, WADE,
TALK, DOOR, and ITEM stay first person. Item presentations (fish, bugs, dug-up
items) preserve the current gaze instead of cutting to the stock trophy view.
Other scripted cameras — events, demos, previews — fall through to the stock
path, and first person resumes when they end. Starting a conversation snaps
the view to face the speaker. In VR, first person switches the world scale to
`vr_fp_world_scale` (life size) and back.

`mMsg_Draw_Window` temporarily raises message and choice centers by 32 pixels
in the original 320x240 UI space while VR first person is active, excluding
flat menu scenes. Body/nameplate and text use the same shifted center; choices
use their own shifted center. Both centers are restored after drawing, keeping
animation state and subsequent draws stable. The overall VR UI panel is unchanged.

Motion tools are two independent pieces:

- **Swing gesture** (`pc_vr_frame_begin`, pc_vr.cpp): sustained right-controller
  speed above 2.2 m/s for ≥ 22 ms fires one virtual A press into the pad
  merge. `pc_vr_swing.h` retains the two-frame pulse and 350 ms cooldown,
  requires slowing to 1.1 m/s to rearm after a fired arc, and uses wrap-safe
  elapsed times. Qualification resets on tracking loss or disabled input.
  `pc_vr_tool_input_allowed()` excludes pause, submenus, conversations,
  invalid current headset poses, and non-first-person play. The pad merger
  checks again before injection; physical A is independently preserved.
  ITEM cameras also set the dialogue guard so motion cannot skip catch text.
  Tool-selection grip also cancels synthetic input, and invalid velocity
  cancels both partial qualification and any remaining virtual A pulse.
- **Tool on hand** (`pc_vr_hand_tool_mtx`, pc_vr.cpp): exports the controller
  pose as the matrix src/game/m_player_item.c_inc pushes in place of the
  player's `right_hand_mtx` — tool procs derive both their attach point and
  their collision from that stack top, so the net catches where you actually
  swing.

Tool target selection still uses the game's original range and collision rules.
The PC-only `Player_actor_face_vr_tool_target` sets body yaw from
`pc_fp_camera_yaw()` at axe/shovel target queries and accepted rod/net actions.
The axe's existing collision triangle is also built with gaze yaw in active VR
first person, without rotating the body each frame. Committed action targets
are not updated as the headset moves. Flat and diorama paths retain body yaw.
If an axe/shovel request is refused after its query, its temporary gaze
alignment is rolled back to the prior logical and visual facing.

The controller-held net's skeleton places the shaft along tool +Z and the
hoop opening along +X. A -90 degree Z roll at net draw changes the hoop's
orientation without yawing the shaft. The stock +3000 Y rotation belongs to
the catch proxy, not the mesh; it is retained only for animated-hand fallback.
The prior shared -3000 yaw trim has been removed. Regular/golden meshes and
all seven net animations are checked against the original disc vertices.

## Residence rear walls and cliff gaps

`src/pc_house_back.c_inc` and its data include add small rear caps for all 18
seasonal house/home models. Indices follow their actual open rear contours;
they do not reflect any original surface. `cKF_Si3_draw_R_SV_solid` routes known
residences through the normal skeleton draw exactly once and appends the cap
in joint 1's frame after child transforms unwind. Original callbacks, doors,
decorations, and palettes continue normally. The cap gets its own frame matrix
and a cached 64x64 CI4 material assembled from opaque original wall samples.
Source detail repeats in model units; plaster homes use sampled timber trim
around their actual contour, center, and eaves. Rear normals blend toward
adjoining original normals except under overhanging eaves. The live segment-8
palette preserves house colors; clamping prevents bleeding into transparent
door/window regions.

`src/pc_structure_back.c_inc` closes the police rear and the tailor's missing
left/rear walls, gable, and eaves. The tailor's absent rear-left corner extends
its existing diagonal footprint; all other corners reference original vertices.
Tailor uses the same owner-scoped joint-1 route as residences. The police actor
appends its patch under the original matrix. Neither path replays a facade or
changes entrances, callbacks, collisions, or window lighting.
The original tailor light plane touches the new left wall. Only under the
active owner-scoped repair, its submitted list uses a cached vertex copy with
the two hidden edge vertices inset by 32 source units per X/Z component. This
avoids depth fighting without moving the wall perimeter or original assets;
callbacks still receive the original display-list pointer and supply its tint.

`src/pc_structure_back_builder.c_inc` shares the bounded patch builder across
police, tailor, museum, and post-office repairs. Each patch has at most eight
vertices and six triangles; each model has at most six patches. Material crops
come from the user's original disc. A NULL spec palette uses live segment 8;
the museum explicitly binds its original fixed seasonal body palette.
`src/pc_civic_back.c_inc` adds museum rear stonework, foundation, shaped gable,
roof underside and narrow column seams, and post-office missing side/rear walls,
eaves and awning backing. Museum appends under its actor's original matrix;
post office uses the owner-scoped joint-1 route. Its light panel has one corner
outside the missing wall: only the submitted copy clips that Z coordinate to
64 source units inside the fitted wall. Callbacks retain original list identity
and lighting control. No original asset, entrance, clock or collision changes.

The fountain's `src/actor/pc_shrine_back.c_inc` adds local bark/stone closures
after their original draws. It replaces the whole-fixture reflection that put
a second tree through the basin. Water, foliage, and the basin remain original
single draws. Remaining structure types retain the measured-shell fallback;
rock caps are unchanged.
The fountain binds its bubble-scroll list in both OPA and XLU before use;
binding only in XLU would let the earlier opaque pass inherit another actor's
segment B. The actor submission regression covers that draw-order dependency.

The two `grd_s_c4_s_[12]` ramp acres omit three cliff-side triangles. Their
display lists now draw those triangles using existing vertices/materials;
neither collision nor culling rules change.

The diagnostic model viewer can capture the real renderer without a headset:
`--model-viewer 8 --model-viewer-solid --model-viewer-angle 225
--model-viewer-distance 30000 --model-viewer-height 6500
--model-viewer-shot <absolute BMP path>`. Shot mode uses a hidden flat window,
waits for six model frames, writes the resolved backbuffer and exits. Normal
viewer input/defaults are unchanged. Its default window lighting and optional
decorations differ from actor-driven gameplay, so captures verify geometry
and texture placement, not the complete in-game lighting/interaction state.
Appended entries 75/76 preview the police box and 77/78 the fountain (summer/
winter); entries 79–82 reproduce their previous reflected fill for comparison.
Museum entries 83/84 preview the repaired seasonal models; 85/86 retain the
former reflected fill for comparison. Post-office entries remain 32/33. Hidden
capture mode extends the far plane to accommodate wide models at long camera
distances; interactive viewer defaults remain unchanged.
For translations beyond the signed 16.16 matrix range, the viewer scales its
camera-space units and near/far planes together before packing. This fixes
stuck/clipped distant previews without changing the game camera or shared
matrix conversion. In-range matrices are unchanged.

## Files

Optional empty hands use their own left/right OpenVR grip actions, preserving
the existing right tool-tip pose and tool-local alignment. CPU player drawing
reports empty item state plus normal movement with an exact `pc_frame_counter`
stamp. Both current and pending non-movement actions suppress the report;
missing player draws expire it. The VR backend additionally checks first-person
gameplay, pause/dialogue/submenu state, and fresh headset/controller tracking.
Each hand is hidden independently when its pose is unavailable. These poses
never produce game input or collision changes.
Touch and Index bindings use `/pose/handgrip`, validated against the installed
driver profiles. `/pose/grip` is absent from Touch's profile even though a
binding JSON using it loads successfully. Once eligible gameplay has run past
the startup grace period, a never-received hand pose logs once; transient loss
after both poses have been received does not report a missing binding.

Catch/discovery rendering uses `pc_vr_item_presentation_mtx` in active first-person
VR. It transforms the most recent head pose through `W^-1`, offsets it by
`(0, -0.14, -0.90)` meters in head space, and removes world scale from the
orientation. Fossils and regular caught insects replace only their temporary
draw position; fish, firefly/spirit sprites, and the tiny-catch pointer also
use its billboard basis.
Action timing, caught-actor identity, and submenu guards stay in the draw callers.
Actor positions, hand/rod matrices, catch probes, collision, and inventory state
are unchanged. CPU drawing uses the previous tracking sample, matching tools;
this is not a new replay-time late-latching path. Desktop/diorama or invalid
tracking falls back to the original draw path.

`pc_vr_hands.cpp` lazily creates a small static mitten mesh. At the end of the
two world passes it draws into each eye's existing depth buffer, before the UI,
using `(H*E)^-1 * grip_pose` in meters and the same GX eye projection. Its GL
state is restored after each draw. Panel-only/NES submission never calls it.
Initialization failure disables just the optional hands for that VR session.

- `pc/include/pc_vr.h`, `pc/src/pc_vr.cpp` — OpenVR runtime, FBOs, matrices,
  compositor submit, input actions, swing gesture, UI panel composite (C++
  behind a C API).
- `pc/src/pc_fp_camera.c`, `pc/include/pc_fp_camera.h` — first-person camera
  state machine (hooked from src/game/m_camera2.c).
- `pc/src/pc_vr_hands.cpp`, `pc/include/pc_vr_hands.h` — optional controller
  mitten mesh, lazy GL lifecycle, depth-tested rendering and state restoration.
- `pc/lib/openvr/` — vendored OpenVR SDK header + win32 `openvr_api.dll`
  (BSD-3-Clause).
- `pc/src/pc_gx.c` — render-target-aware viewport/scissor/copy scaling,
  flush-time matrix override, ortho→UI routing, mirror blit.
- `src/static/libforest/emu64/emu64.c` — view-notify hook, VR cull widening.
- `src/graph.c` — per-eye interpretation loop.
- `pc/src/pc_vi.c` — VR pacing/submit path.
- `pc/src/pc_pad.c` — VR pad merge.
- `vr_actions/*.json` — action manifest + default bindings.

## Settings

`settings.ini` `[VR]`: `vr_mode` (0 = off, 1 = auto — use headset when
present, 2 = force), `vr_world_scale` (mm per game unit), `vr_ui_distance` /
`vr_ui_size` (cm), `vr_height_offset` (cm). First-person and motion-tool keys
live under `[FirstPerson]`: `fp_mode`, `fp_eye_height`, `fp_snap_degrees`,
`vr_fp_world_scale`, `vr_solid_buildings`, `vr_solid_shell`, `vr_draw_radius`,
`vr_town_residency`, `vr_motion_swing`, `vr_tool_on_hand`, `vr_tool_pitch`,
`vr_empty_hands` (default 0, live Gameplay settings toggle; explicit 1 enables it).
The generated settings.ini documents each. CLI: `--vr`, `--no-vr`. If OpenVR
init fails the game logs once and runs flat — the same binary serves both
modes.

## Frame pacing

WaitGetPoses blocks until ~3 ms before vsync ("running start") and paces the
loop at HMD rate; the 60 fps limiter and window vsync are bypassed in VR. If
GPU headroom runs out the compositor's async reprojection covers head motion.
