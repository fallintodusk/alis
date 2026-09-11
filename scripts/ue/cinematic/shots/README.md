# Shot board

How ALIS shots are framed and moved, and what the current set is for. This file is
durable; the shot plans it describes are not.

**Shot plans live in `tmp/cinematic/shots/<world>/<id>.json`, not in git.** They are
transient work for one piece of footage - a trailer, a demo - and they change every
time a framing is rejected. What is worth keeping is here: the rules that make a take
work, and the record of what each take was for. Anyone can regenerate a plan from
those; nobody needs last month's camera coordinates.

Plan contract: [`../schemas/shot-plan.schema.json`](../schemas/shot-plan.schema.json).

```powershell
.\scripts\ue\cinematic\run_shot_capture.ps1 -PlanPath tmp\cinematic\shots\<world>\<id>.json -Preview
.\scripts\ue\cinematic\run_shot_capture.ps1 -PlanPath tmp\cinematic\shots\<world>\<id>.json
```

## Worlds

| World | `map` | Subjects |
|---|---|---|
| Manhattan showcase | `/ProjectWorldData/Generated/Showcase/Manhattan/L_ProjectWorldManhattanShowcase` | tall core at X 0..93k, Y -140k..-47k, peaking 446 m and 544 m; rivers 2.1 km NE (bearing 27) and 2.6 km SW (225) |
| Kazan territory | `/ProjectWorldData/Generated/Territory/L_ProjectWorldKazanTerritory` | low-rise; river confluence, causeway and lakes near origin |

A new take is a JSON file at `tmp/cinematic/shots/<world>/<id>.json` carrying only
what reproduces the camera. Preview it, then render it.

Captured asset IDs use `<city-local-priority>_<city>_<shot>` so Explorer sorts the
editorial order without mixing city priorities: `1_manhattan_grand_traverse`,
`2_manhattan_tower_canyon`, `1_kazan_grand_traverse`, and so on. Preview output adds
only `_preview`. Priorities restart at 1 for each city and remain stable after approval.

## What a viewer should learn

ALIS reconstructs large real territories as playable worlds, at a scale you feel
rather than read, and the same generic pipeline does it for more than one city.

## Takes

**Accepted library:** all twelve numbered shots below were operator-approved and
produced at 1920x1080, 30 fps. Their compact, edit-ready HEVC Main 10 files and
hash-bound manifest live in `Saved/CinematicRaw/Final/`. Per-shot capture bundles and
preview plans are working data, not part of the accepted library.

| Shot | What it answers | Move | s |
|---|---|---|---|
| `1_manhattan_grand_traverse` | How far does it go on? | full useful-city diagonal with a skyline, river and park arrival | 30 |
| `2_manhattan_tower_canyon` | What is it like down there? | corridor between the 446 m and 544 m towers opening onto the river | 22 |
| `3_manhattan_skyline_compression` | Does the generated city read as a real skyline? | long-lens oblique layering near blocks, tall core, river and far bank | 20 |
| `4_manhattan_massing_descent` | What is actually built here? | descent 2050 m to 1100 m onto the tall cluster | 22 |
| `5_manhattan_central_park_glide` | How does its scale relate to open space? | oblique glide keeping park, tower wall, river and far bank in one composition | 20 |
| `6_manhattan_waterfront_parallax` | Does the skyline have readable depth? | lower waterfront move with lateral parallax across the tower silhouette | 20 |
| `7_manhattan_grid_tilt_reveal` | Is the street structure connected to the wider geography? | steep grid view tilting out to skyline, river and far bank | 20 |
| `1_kazan_grand_traverse` | Is the second city continuous at scale? | diagonal traverse from broad water geography into the low-rise city, roads and canal | 30 |
| `2_kazan_canal_axis_push` | Is its infrastructure legible? | lower push from river crossing and stadium forms onto the canal axis | 20 |
| `3_kazan_kazanka_parallax_arc` | Does Kazan have a distinct geographic structure? | wide parallax arc across the Kazanka water system and urban districts | 24 |
| `4_kazan_river_crossing_approach` | Are the two banks connected? | approach that keeps the river crossing and both banks readable | 20 |
| `5_kazan_volga_waterfront_pullin` | How does the city meet the larger water system? | water-led pull-in resolving into roads, buildings and canal | 20 |

Poses come from probe stills rendered through the real capture route - never from
full-partition scouting stills, which show city where the renderer has nothing.

## Camera rules

Source of truth for how ALIS shots are framed and moved.

Two kinds of rule live here, and the difference matters. **Measured constraints** -
the loaded envelope, the pitch floor, what a corridor must clear - came from a
rejected take or a probe, and breaking one produces a broken frame. **Working
defaults** - speed, rotation rate, lens choices, the shot grammar - are directing
heuristics that have not all been through visual approval yet. Treat the first as
binding and the second as a starting point to argue with.

These are the numbers that enforce the style. The style itself - what the footage
claims, what the viewer must understand, what we will not shoot - is owned by
[docs/cinematics/visual_language.md](../../../../docs/cinematics/visual_language.md).

### Motion

- **Speed: about 100 m/s** of translation (10000 world units a second). Approved by
  the operator after watching it: "camera is moving and it's already fine, so we
  could keep this speed as default".
- **Rotation: 6-8 degrees a second** (working default, not yet visually approved).
  An orbit is judged by angular rate, not ground speed, so a full turn lands near a
  minute.
- **Never travel straight along your own view axis.** Move diagonally in X and Y,
  and keep the view yaw 25-40 degrees off the direction of travel. Flying straight
  at what you are looking at only makes the subject grow; off-axis travel slides it
  across frame and puts near and far geometry in relative motion. This is a
  composition and parallax rule only - it does NOT change which cells load. The
  3 km World Partition source follows the active camera, so travel moves the loaded
  envelope but does not make it wider.
- **Always change altitude across a move.** A constant height is inert; a rise or
  descent keeps the eye working. Fifteen thousand units is the minimum worth having.
- **Yaw slightly off the direction of travel.** Looking exactly where you are going
  kills the sense of passing through anything.
- **Yaw across an orbit must increase past 360** rather than wrap, or the
  interpolation takes the short way round and the camera reverses mid-turn.
- **Truck-and-pan does not survive here.** A lateral truck with a pan walks the
  camera off the loaded band and fills the frame with void. Use a circular arc
  about the subject: it holds the subject centred and the camera at a constant
  distance from origin.

### Framing

- **Pitch has a floor, and it is derivable.** Only the world inside the 3 km
  camera-following source is loaded, so a wide aerial frustum can still see a hard
  edge. The horizon leaves frame once the camera pitches down past the vertical
  half-FOV plus a margin:

  | fov | vertical half | pitch at or below |
  |---|---|---|
  | 35 | 10.1 | -16 |
  | 40 | 11.6 | -18 |
  | 45 | 13.1 | -19 |
  | 50 | 14.7 | -21 |
  | 60 | 18.0 | -24 |

  A shallower pitch is safe only when the city itself fills the top of frame, which
  means being close to it. Four takes were rejected for ignoring this.
- **Stay over dense city.** A wide, high view can see beyond the moving 3 km envelope
  and shows the gap where cells end. Lower and tighter, with a steeper angle, keeps
  the visible ground inside the loaded region.
- **Put water in the wide shots.** City edge to edge reads flat. The rivers sit
  about 2.1 km north-east of the core (bearing 27 degrees) and 2.6 km south-west
  (bearing 225). Aim across the core toward one of them and water lands in the
  upper third.
- **A zoom needs a target.** Aim both ends of the move at a subject - the tall core
  around (41, -46545), peaking at 446 m and 544 m - so the shot arrives somewhere
  instead of descending onto whatever happens to be underneath.
- **Never go close to the massing.** It has no facades, so anything near reads as
  untextured slabs. The one exception is a corridor flight, where near objects
  passing at speed are the point.
- **A corridor must clear the towers.** Flying at street height goes through
  buildings. The gap between the two tallest cells, (41, -46545) at 446 m and
  (93041, -46545) at 544 m, runs up X around 67000; at 340-420 m the camera passes
  between those two and above everything else in the core.
- **Never frame the whole territory from outside it.** The tile has hard edges and
  reads as a floating diorama - the opposite of scale.

### Handles

Written into the camera keys, not a schema field: keep 2-3 seconds of slower motion
at each end while the core move remains faster. Handles let an editor trim, dissolve,
retime or let a shot breathe without making the raw clip look stuck when played whole.
Do not duplicate the first or final pose by default. A genuinely static hold is used
only when the shot calls for one and its complete playback has been visually approved.

### Grammar

A push commits the viewer to a subject. A traverse proves continuity, because only
relative motion between near and far objects shows the depth is real. An orbit
describes an object but proves nothing about the world around it, so it needs a
subject worth circling. A descent turns scale into human scale. A corridor flight
sells speed through near objects passing close.

### Verify before committing

Scouting stills load the full partition and therefore over-promise: a framing they
approve can still render a void wall. Always `-Preview` a changed shot and look at
first, middle and last before spending a production render.
