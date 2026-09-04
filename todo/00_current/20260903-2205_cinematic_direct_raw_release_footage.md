# Raw Manhattan and Kazan footage for the release cut

**Status:** IN PROGRESS - route and shot plans ready; renders wait for the editor to close
**Created:** 2026-09-03 22:05 Europe/Moscow

## Goal

A small library of good raw clips a human can montage by hand: more than 60 seconds in
total, mostly Manhattan, with Kazan as second-city proof. Scout, choose shots, render,
look at the frames, stop. No montage, no audio, no titles, no cinematic subsystem.

## Decisions

- The director skill is the point of this work, not the tooling around it.
- Do not touch `ProjectCinematic` or the Kazan release route. Raw takes are disposable
  and carry no release claim; if one ever becomes a master it goes through the release
  route then.
- Scout with authenticated stills and approve framing before authoring a camera move.
- Capture 15-30 s per take with stable frame at each end, not the 3-6 s an edit uses.
- Reject any take whose loaded map is not the map that was asked for.

## What exists

- Skill `cinematic-capture-director` in the operator's skill directory. The repository
  `.claude/skills/` path is denied to this agent's write permission, so it is live but
  not yet version-controlled with the project; moving it is a one-file copy.
- `scripts/ue/cinematic/run_shot_capture.ps1` + `shot_capture_editor.py` +
  `schemas/shot-plan.schema.json` - about 530 lines total. The plan holds only what
  reproduces the camera; reasoning lives in `shots/README.md`.
- Eight shot plans, 186 s total, 76 % Manhattan.
- `docs/cinematics/raw_capture.md`, routed from the cinematics and scripts READMEs.

## The constraint that shapes every shot

The render has only a small part of the world loaded. Its one World Partition
streaming source is a sphere pinned at world origin that does not follow the camera:
`center=X=0 Y=0 Z=0 radius=100000cm`. Anything outside it renders as a flat void wall.

Two fixes were tried and disproved, both recorded in `docs/cinematics/raw_capture.md`
so nobody repeats them: editing the cinematic GameMode class default does not reach
the spawned instance, and `streaming_radius_m` widens the grid loading range, which
that source ignores because it sets `bUseGridLoadingRange = false`. Its commands do
execute - all nine appear in the log - they simply do not apply to this source.

Addressing the source itself means changing `ProjectCinematic`, which is out of scope.
So the shots work inside the envelope instead: subject near origin, camera two to four
kilometres out, pitch about -13 degrees or steeper, and a narrower `fov` because the
loaded band is roughly a kilometre either side of origin.

What this costs: the Lower Manhattan and One World Trade pass is not renderable - that
cluster sits four kilometres from origin. It is replaced by a lateral pan across the
main core. The long-lens skyline moves from 6.8 km to 4.3 km, so less compression.

## Findings worth keeping

Both are recorded in `docs/cinematics/raw_capture.md`; repeated here only as the reason
the guards exist.

- **Wrong-world capture is real.** A plan whose map did not resolve produced a complete,
  well-formed capture of `/City17/Maps/City17_Persistent_WP`. Two cheap comparisons now
  refuse it.
- **The gap is missing geometry, not water.** The building-massing descriptors place
  cells continuously along the sight line and 210 landscape proxies tile the territory.
- **Scouting stills over-promise.** The evidence route loads the full partition, so a
  framing it approves can still render a void wall. Confirm with `-Preview` before
  committing a production take. This cost two blind production renders before it was
  understood.
- Scouting still changed the shots for the better: never frame the whole tile from
  outside it (reads as a diorama), and never go close (the massing has no facades).
  Details in `scripts/ue/cinematic/shots/README.md`.
- **Holds are genuinely static.** Duplicating the first and last pose two seconds in
  gives 45 dB and 51 dB across the hold windows against 26 dB across moving windows,
  measured with a moving control so a false pass could not slip through. Cubic auto
  tangents need no switch to linear.
- On UE 5.8 the Sequencer time-unit enum is `MovieSceneTimeUnit`, not
  `SequenceTimeUnit`; transform channels are Location XYZ, Rotation X/Y/Z, Scale XYZ.

## Loop

One shot at a time, never a blind batch:

1. `-Preview` the shot (Dev preset, promotes to `<id>_preview/`).
2. Look at first/middle/last. Reject a void wall, a visible tile edge, an empty
   foreground, or close facades, and re-aim.
3. Only when the preview reads correctly, render the production take.

## Remaining

- [x] `manhattan_massing_descent` - accepted framing (2000 m to 1050 m onto the core).
- [x] `manhattan_scale_reveal` - re-aimed to fov 40 on the loaded band; preview clean.
- [ ] Production render of `manhattan_scale_reveal`.
- [ ] The other six: preview, re-aim into the envelope, then render.
- [ ] Confirm the accepted clips sum to more than 60 s with Manhattan dominant, then stop
      for the human cut.

## Verified so far

- Four refusal cases observed refusing: absent map, malformed plan, wrong-world capture,
  more than one video master.
- Smoke: 300 frames at 960x540 in 92 s including editor start. First production take:
  720 frames at 1920x1080 in 269 s.
- `git status --short` shows no change under `Plugins/Editor/ProjectCinematic/` or the
  release capture scripts, schema and request.
- Governance text validator passed; no stable doc, script or config references this todo.
