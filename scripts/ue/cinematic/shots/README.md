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

## What a viewer should learn

ALIS reconstructs large real territories as playable worlds, at a scale you feel
rather than read, and the same generic pipeline does it for more than one city.

## Takes

**Active: `manhattan_scale_reveal` only.** Everything below it is a frozen idea, not
a plan. Nothing else gets framed, previewed or rendered until the first shot has an
approved framing. The seconds and percentages that used to be totalled here were
counting shots that do not exist yet.

| Shot | What it answers | Move | s |
|---|---|---|---|
| `kazan_city_traverse` | Is the second city as complete? | diagonal climb over the low-rise centre and lakes | 20 |
| `kazan_river_reveal` | Does this work somewhere else? | diagonal across the river onto the far-bank city | 24 |
| `manhattan_cells_long` | How far does it go on? | 4.6 km diagonal traverse, 1650 m to 1200 m, river in shot | 40 |
| `manhattan_density_zoom` | How dense does it get? | targeted zoom 3200 m to 900 m onto the tall core | 26 |
| `manhattan_downtown_pass` | Would I recognise this place? | diagonal descent 1800 m to 1080 m across the core | 22 |
| `manhattan_fly_between` | What is it like down there? | corridor between the 446 m and 544 m towers at 420 m | 22 |
| `manhattan_flyby` |  | 90 deg descending arc ending across the core on the river | 26 |
| `manhattan_massing_descent` | What is actually built here? | descent 2050 m to 1100 m onto the tall cluster | 20 |
| `manhattan_orbit` | Is it built on every side? | full 360 spiralling 2.0 km to 1.6 km out, 1200 m to 850 m | 56 |
| `manhattan_scale_reveal` | How large is this world? | diagonal run-in, 1450 m to 1000 m, toward the river | 24 |
| `manhattan_scale_reveal_high` | same, from twice the altitude | 2450 m to 1900 m, panning to hold the core | 24 |
| `manhattan_skyline_long` | A skyline, or props on a plate? | 35 deg lens, diagonal dolly toward the core | 24 |
| `manhattan_traversal` | Continuous world, or one viewpoint? | diagonal traverse climbing 800 m to 1180 m | 28 |

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
  streaming source is pinned near world origin, so travel direction has no effect
  on the loaded envelope.
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

- **Pitch has a floor, and it is derivable.** Only the world near origin is loaded,
  so anything past that band is a flat void wall. The horizon leaves frame once the
  camera pitches down past the vertical half-FOV plus a margin:

  | fov | vertical half | pitch at or below |
  |---|---|---|
  | 35 | 10.1 | -16 |
  | 40 | 11.6 | -18 |
  | 45 | 13.1 | -19 |
  | 50 | 14.7 | -21 |
  | 60 | 18.0 | -24 |

  A shallower pitch is safe only when the city itself fills the top of frame, which
  means being close to it. Four takes were rejected for ignoring this.
- **Stay over dense city.** A wide, high orbit swings out past the loaded band and
  shows the gap where the cells end. Lower and tighter, with a steeper angle, keeps
  built ground under the camera the whole way round.
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

Written into the camera keys, not a schema field: duplicate the first pose at
`t=2` and the last at `t=duration-2`. The take then holds still for two seconds at
each end and moves in between, which is what lets an editor trim, dissolve, retime
or let a shot breathe. Measured at 45-51 dB across the hold windows against 26 dB
across moving windows, so the holds are genuinely static under cubic auto tangents.

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
