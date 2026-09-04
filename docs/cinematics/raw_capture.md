# Raw shot capture

Renders one camera move over an ALIS world into a raw clip for a human to cut.

Not the release route: it binds to no Candidate and touches nothing under
`Saved/CinematicRelease`, `Saved/PackageRelease` or `Saved/Validation`. Release
masters stay with [ProjectCinematic](../../Plugins/Editor/ProjectCinematic/README.md).

```powershell
.\scripts\ue\cinematic\run_shot_capture.ps1 -PlanPath tmp\cinematic\shots\<world>\<id>.json
.\scripts\ue\cinematic\run_shot_capture.ps1 -PlanPath <plan> -Preview   # composition only
```

`-Preview` renders the same camera at 480x270 with one temporal sample and promotes
to `Saved/CinematicRaw/<id>_preview/`, so a composition check can never be mistaken
for the real take. That resolution is deliberately low: a preview answers whether the
framing and the motion are right, and a void wall, a tile edge or a badly placed
subject are all obvious at a quarter of the linear resolution. Streaming and camera
path are untouched, so what a preview shows still holds at production. Roughly a
third of the time and a twentieth of the disk of a real take, with editor start-up
now the floor rather than the render. Use it for every framing change.

The wrapper cold-starts an isolated editor on the plan's map, renders through the
existing MRQ production preset, checks the master with `ffprobe`, and promotes to
`Saved/CinematicRaw/<id>/` with the clip, the plan, a small receipt, the editor log,
and first/middle/last frames. `Saved/` is untracked.

Why a shot looks the way it does is owned by
[visual_language.md](visual_language.md). How it is framed and moved in numbers -
speed, pitch floors, diagonal travel, water, what a corridor must clear - is owned by
the shot board:
[`scripts/ue/cinematic/shots/README.md`](../../scripts/ue/cinematic/shots/README.md).
Every rule there was paid for by a rejected take.

## Shot plan

Schema: [`shot-plan.schema.json`](../../scripts/ue/cinematic/schemas/shot-plan.schema.json).
It holds only what reproduces the camera: `id`, `map`, `duration`, `fps`, `fov`, and
two or more pose keys, plus optional `interpolation`, `resolution`, `preset` and
`timeout_seconds`.

Plans live in `tmp/cinematic/shots/<world>/<id>.json` and are not committed: they are
transient work for one piece of footage. The durable parts are this contract, the
route, and the shot board's rules - together enough to regenerate any plan. Why a
shot exists belongs in the shot board, not in engine schema.

`fov` is horizontal degrees; the focal length is solved from the camera's real
sensor width so the render matches the scouting still, which is captured by FOV.

The generated LevelSequence is compiled output under `Content/Cinematics/Generated/`,
gitignored and rebuilt from the plan every run - the same relationship the MRQ preset
recipes have with their assets.

## Two things that have actually bitten

**The editor renders the wrong world.** When the requested map fails to load the
editor falls back to its startup map and produces a complete, well-formed capture of
somewhere else - it has produced one of `/City17/Maps/City17_Persistent_WP` here. The
driver compares the loaded world to the request before authoring, and the wrapper
compares the receipt to the plan. Keep both.

**The render only has a small part of the world loaded.** The render's one World
Partition streaming source is a sphere pinned at world origin, sized for interior
scenes and not following the camera:

```text
WP streaming source OK | center=X=0.000 Y=0.000 Z=0.000 radius=100000cm
```

Everything outside it renders as a flat void wall - the ocean plane showing under the
missing cells. Two routes out of this do **not** work, both tried and disproved, so do
not spend time on them again:

- Editing the cinematic GameMode's class default from the driver. The spawned
  instance keeps the default radius; the log above is from a run where the default
  had been set ten times larger.
- A `streaming_radius_m` plan field expanding into World Partition console commands.
  The commands demonstrably executed - all nine appeared in the log - but they widen
  the *grid* loading range, and this source sets `bUseGridLoadingRange = false`, so it
  ignores them. The measured footprint was identical with and without them. The field
  and its commands have been **removed**; do not reintroduce them. Warm-up ticks are
  now applied unconditionally, which is the part that did matter.

The loaded footprint is measured by rendering a nadir preview and reading the cell
blob off the frame - not from config, and not from a scouting still. It came out
cross-shaped, roughly 5.0 x 4.7 km around origin. Anything outside is void.

Until the source itself is addressed, the rule is simply: the camera and every part
of the visible ground stay inside the measured footprint, and the `-Preview` pixels
are the authority. Do not carry a fixed standoff distance around - four kilometres
from origin can already be outside the loaded region, so the envelope decides the
distance, not a number in a document. The pitch floor per lens is in the shot board.

**Scouting stills over-promise.** `capture_visual_evidence.ps1` loads the full
partition, so it shows a complete world at poses the renderer cannot fill. A framing
approved from a still can still render a void wall. Confirm composition with
`-Preview` before committing a production take.

## Verifying

The receipt says what was rendered, not whether it shows anything. Look at
`frames/first.png`, `middle.png` and `last.png`. MRQ producing bytes is not evidence.

## Scouting

Candidate framings are captured as authenticated stills before any camera move:

```powershell
.\scripts\ue\world\capture_visual_evidence.ps1 -Map //<plugin>/<path>/<map> `
    -VantagePlan tmp\cinematic\scout\<name>.json -OutputDirectory <dir> -ReceiptPath <receipt>
```

A vantage plan carries `capture_width`, `capture_height`, `fov_degrees` and poses with
`x`, `y`, `z`, `pitch`, `yaw` and optional per-pose `fov`. The leading `//` is required
from Git Bash, which otherwise rewrites a leading `/Plugin/...` into a Windows path and
captures the wrong world. Contract:
[Visual Verification](../../tools/World/VisualVerification/README.md).
