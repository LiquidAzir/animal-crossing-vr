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
model (src/game/m_player_draw.c_inc) while keeping its shadow. Scripted
cameras — doors, events, demos — simply fall through to the stock path, and
first person resumes when they end. Starting a conversation snaps the view to
face the speaker. In VR, first person switches the world scale to
`vr_fp_world_scale` (life size) and back.

Motion tools are two independent pieces:

- **Swing gesture** (`pc_vr_frame_begin`, pc_vr.cpp): sustained right-controller
  speed above 2.2 m/s for ≥ 22 ms fires one virtual A press into the pad
  merge, with a 350 ms refractory so one arc can't double-trigger. Thresholds
  are time-based, so behavior is identical at 72/90/120 Hz. Gated to first
  person with a swingable tool out and no conversation active.
- **Tool on hand** (`pc_vr_hand_tool_mtx`, pc_vr.cpp): exports the controller
  pose as the matrix src/game/m_player_item.c_inc pushes in place of the
  player's `right_hand_mtx` — tool procs derive both their attach point and
  their collision from that stack top, so the net catches where you actually
  swing.

## Files

- `pc/include/pc_vr.h`, `pc/src/pc_vr.cpp` — OpenVR runtime, FBOs, matrices,
  compositor submit, input actions, swing gesture, UI panel composite (C++
  behind a C API).
- `pc/src/pc_fp_camera.c`, `pc/include/pc_fp_camera.h` — first-person camera
  state machine (hooked from src/game/m_camera2.c).
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
`vr_town_residency`, `vr_motion_swing`, `vr_tool_on_hand`, `vr_tool_pitch`.
The generated settings.ini documents each. CLI: `--vr`, `--no-vr`. If OpenVR
init fails the game logs once and runs flat — the same binary serves both
modes.

## Frame pacing

WaitGetPoses blocks until ~3 ms before vsync ("running start") and paces the
loop at HMD rate; the 60 fps limiter and window vsync are bypassed in VR. If
GPU headroom runs out the compositor's async reprojection covers head motion.
