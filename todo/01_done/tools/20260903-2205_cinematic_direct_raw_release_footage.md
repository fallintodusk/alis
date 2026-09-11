# Raw Manhattan and Kazan footage for the release cut

**Status:** DONE - twelve productions verified, consolidated and cleaned
**Created:** 2026-09-03 22:05 Europe/Moscow

## Goal

A 150-180 s library of good raw clips from which a human can montage an approximately
60 s prototype/framework demonstration: 60 % Manhattan and 40 % Kazan. Show geographic
scale, density, runtime streaming and the same open-source, data-driven generation
framework working for two cities. This is an honest blockout/prototype demonstration,
not a claim that either city has final presentation art.

Scout, choose shots, render, look at the frames, stop. No montage, no audio, no baked-in
titles and no cinematic subsystem. The human edit may later add a small number of
truthful overlays and derive 5-10 s shorts from the same clean footage.

## Decisions

- The director skill is the point of this work, not the tooling around it.
- Do not redesign `ProjectCinematic` or alter the Kazan release route. This task made
  one bounded owner-local exception: authenticated MRQ captures proved that the
  Editor-only camera-following World Partition source needed a 3 km radius instead of
  1 km to keep city-scale geography resident. Gameplay streaming remains untouched.
- The retained deliverable is the verified compact library under
  `Saved/CinematicRaw/Final/`: twelve HEVC Main 10 clips plus its integrity manifest.
  After operator approval and manifest/hash/metadata/frame-count/full-decode
  verification, ProRes masters, generated sequences, previews and transient camera
  plans are disposable working data rather than archival authority.
- Scout with authenticated stills and approve framing before authoring a camera move.
- Capture most moving takes as 2-3 s slow entry, 8-12 s meaningful core motion and
  2-3 s slow exit. Establishing takes may be longer. The edit will normally use only
  4-9 s, so every raw take keeps usable handles without looking frozen in full playback.
- Reject any take whose loaded map is not the map that was asked for.
- Do not frame the literal full rectangular territory or its tile edge. Prove full-map
  scale through a recognisable dense core, water or roads as scale references, and
  continuous city in the far distance.
- Distant World Partition cells may appear softly during a traversal. That is honest
  prototype evidence that the generated world streams around the camera. It must remain
  secondary to the city: foreground and middle distance stay dense and stable, with no
  void ahead, nearby pop, missing layer, hard rectangular boundary or repeated flicker.
- Two diagonal traversals are anchor shots, not optional filler: Manhattan crosses the
  densest tower districts toward water; Kazan crosses terrain, roads, water and multiple
  urban districts. Each must make newly reached
  geography legible while preserving strong parallax.
- Raw asset IDs are numbered independently inside each city in editorial priority order:
  `1_manhattan_*`, `2_manhattan_*`, then `1_kazan_*`, `2_kazan_*`. The lowercase form
  satisfies the shot-plan identity contract while the number keeps Explorer ordering
  obvious. Preview folders add only the existing `_preview` suffix.

## Editorial spine

This is a planning target for the human montage, not a rendered sequence or a promise
that every second survives the edit.

| Time | World | Intended proof |
|---|---|---|
| 0-7 s | Manhattan | immediate hero scale reveal |
| 7-15 s | Manhattan | diagonal flight through the dense tower cluster |
| 15-25 s | Manhattan | skyline, river and far-bank extent |
| 25-36 s | Manhattan | density descent or pull-back across generated districts |
| 36-44 s | Kazan | Kazanka/Volga geography and infrastructure reveal |
| 44-52 s | Kazan | diagonal runtime-streaming traverse across multiple layers |
| 52-58 s | Kazan | real packaged PreviewFlight proof |
| 58-60 s | Kazan | strong closing world frame and ALIS identity |

Potential human-edit messages form a claim bank, not twelve mandatory title cards. Use
only about five in the 60 s cut and at most one overlay per scene: `ALIS World Generation
Prototype`, `Real cities. Rebuilt at scale.`, `Open-source, data-driven framework`, `One
pipeline. Multiple cities.`, `Terrain. Roads. Water. Vegetation. Buildings.`, and `World
Partition streams generated cells at runtime`. Manhattan footage proves generated scale
and density; only the packaged Kazan footage may make the current playability claim.

## Shot map

### Manhattan Grand Traverse

Canonical editorial name: `Manhattan Grand Traverse`. The approved preview artifact
retains its reproducible technical ID: `manhattan_full_diagonal_south_to_north_v5`.
"Grand Traverse" describes the shot's real job - a continuous city-scale journey -
better than "Full Observer", which implies a mostly static overview.

| Time | Shot beat | What the viewer reads |
|---|---|---|
| 0-3 s | Southern-waterfront establish | The generated city begins at a clear geographic edge. |
| 3-13.2 s | Long diagonal push | Continuous scale, density and parallax through the tower core. |
| 13.2-19.2 s | Rising rotational bridge | The camera crosses the world while distant cells remain secondary. |
| 19.2-27.5 s | Descending hero reveal | Skyline, Central Park, river, far bank and foreground city resolve together. |
| 27.5-30 s | Exit handle | The strongest overview remains usable for a cut, title or dissolve. |

### Manhattan Tower Canyon

Canonical editorial and technical name: `2_manhattan_tower_canyon`.

| Time | Shot beat | What the viewer reads |
|---|---|---|
| 0-2.5 s | Dense-core approach handle | The camera is already inside Manhattan's vertical massing. |
| 2.5-12 s | Tower approach | Near and far buildings separate through strong parallax. |
| 12-19.5 s | Canyon pass | One tower briefly fills the frame at speed while the river opens beyond it. |
| 19.5-22 s | River-and-city exit handle | The close density resolves into geographic context. |

### Approved production board

All ten additional candidates passed the preview capture contract and twelve-frame
visual review, then received operator approval. Their production renders passed the
same visual review at 1920x1080 before compact encoding and final-library promotion.

| Priority | Candidate | Agent read |
|---|---|---|
| M3 | `3_manhattan_skyline_compression` | Keep - strongest calm skyline layering. |
| M4 | `4_manhattan_massing_descent` | Keep - useful scale-to-mass transition. |
| M5 | `5_manhattan_central_park_glide` | Keep - strongest land-use contrast and one of the best new shots. |
| M6 | `6_manhattan_waterfront_parallax` | Experiment - distinct low skyline depth; less geographic than M3/M5. |
| M7 | `7_manhattan_grid_tilt_reveal` | Keep - strongest camera experiment, linking street grid to river scale. |
| K1 | `1_kazan_grand_traverse` | Keep - best complete Kazan scale proof. |
| K2 | `2_kazan_canal_axis_push` | Keep - readable canal/infrastructure progression. |
| K3 | `3_kazan_kazanka_parallax_arc` | Keep - strongest Kazan geographic composition. |
| K4 | `4_kazan_river_crossing_approach` | Experiment - clear crossing, but intentionally water-heavy. |
| K5 | `5_kazan_volga_waterfront_pullin` | Keep - broad water-to-city progression. |

Official place research made the Kremlin and Millennium Bridge reasonable scouting
hypotheses, but the generated blockout pixels do not make either landmark visually
identifiable. The rejected landmark-labelled takes were therefore not promoted or
misnamed. The active Kazan board claims only what the footage visibly proves.

The packaged Kazan PreviewFlight proof remains a separate real-runtime capture. It is
not an MRQ shot and is not replaced by these clean editorial takes.

## What exists

- Skill `cinematic-capture-director` in the operator's skill directory. The repository
  `.claude/skills/` path is denied to this agent's write permission, so it is live but
  not yet version-controlled with the project; moving it is a one-file copy.
- `scripts/ue/cinematic/run_shot_capture.ps1` + `shot_capture_editor.py` +
  `schemas/shot-plan.schema.json` - about 530 lines total. The plan holds only what
  reproduces the camera; reasoning lives in `shots/README.md`.
- Twelve approved production shots: seven Manhattan and five Kazan. The accepted
  compact library and its integrity manifest live in `Saved/CinematicRaw/Final/`.
- `docs/cinematics/raw_capture.md`, routed from the cinematics and scripts READMEs.

## The constraint that shapes every shot

The render has only a small part of the world loaded. Its one World Partition
streaming source follows the active Sequencer camera every 0.1 seconds, but its
original 1 km radius could not hold foreground, skyline, and background together.
Three low-oblique MRQ scouts reproduced the flat blue unloaded background, so the
Editor-only cinematic owner now uses a 3 km radius. Gameplay streaming is untouched.

Two fixes were tried and disproved, both recorded in `docs/cinematics/raw_capture.md`
so nobody repeats them: editing the cinematic GameMode class default does not reach
the spawned instance, and `streaming_radius_m` widens the grid loading range, which
that source ignores because it sets `bUseGridLoadingRange = false`. Its commands do
execute - all nine appear in the log - they simply do not apply to this source.

Shots work inside the implemented moving envelope: lower camera, readable skyline,
dense ground under the frustum, and a preview before production. The preview pixels
are the authority.

The earlier origin-pinned limitation made Lower Manhattan and the One World Trade area
unusable. The camera-following source retires that limitation. The remaining constraint
is local: a fast arrival near a territory boundary needs enough exit handle for nearby
cells to finish loading, and a camera must not point beyond the generated extent.

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
- [x] `manhattan_scale_reveal` - fov 45 pull-back and rise from the hero
      cluster into turquoise water and far-bank city; authenticated endpoints and
      a twelve-frame contact sheet show three depth layers with no tile edge or
      unloaded blue wall.
- [x] Retimed `manhattan_scale_reveal` composition probe: preserved the approved endpoints
      and fov 45 while changing only timing to a 14 s take. Operator rejected it as the
      requested traversal because it remains a short backward reveal; keep it only as an
      optional establishing candidate and do not produce it yet.
- [x] `manhattan_cells_long` diagonal density/streaming preview: 4.56 km southwest-to-
      northeast forward traversal in an 18 s take with 3 s handles and a 12 s,
      approximately 380 m/s core. Its frames cross the dense core and river into a new
      far-bank district, but the operator rejected it because it starts inside the city
      and covers only about one third of the generated north-south extent. Do not render
      it in production.
- [x] `manhattan_full_diagonal_south_to_north_v2` diagnostic preview: its first 12 s
      established the approved southern-waterfront start and full-city direction, but
      frames 16-20 expose the unloaded boundary across roughly half the image. Reject it
      visually despite its technically accepted receipt; do not render it in production.
- [x] Runtime control-point previews at the 15 s bridge, 18 s overhead transition and
      20 s recovery positions. All three use the real camera-following 3 km streaming
      envelope and finish with loaded ground. The overhead point is deliberately brief
      because it proves continuity but is weaker as a destination frame.
- [x] `manhattan_full_diagonal_south_to_north_v4` preview: keep the approved first 12 s,
      then rise from 900 m to 1600 m and pitch down while rotating through only the three
      proven control points. Stop one canonical cell before the sparse technical boundary.
      The resulting 11.3 km diagonal still crosses the complete useful city presentation
      from the southern waterfront through the dense core, while frames 12-29 contain no
      half-frame unloaded edge. The operator accepted the loading correction but rejected
      its dull top-down destination and excessive final 8 s hold. Do not produce v4.
- [x] Compare three separately rendered runtime endpoints after the long traverse. Select
      the river-oblique candidate over the northern and central alternatives because it
      combines foreground blocks, Central Park, the river/far bank and the tall skyline.
- [x] `manhattan_full_diagonal_south_to_north_v5` preview: retain the accepted opening and
      loading-safe control points, then continue moving from 22.2-27.5 s in a 1.3 km
      descending return toward the selected hero view. Only the final 2.5 s are static
      editing handle. Frames 20-29 preserve loaded geography and arrive on the skyline,
      river and park rather than unremarkable top-down blocks.
- [x] Operator visually approved `manhattan_full_diagonal_south_to_north_v5` on
      2026-09-07 as the `Manhattan Grand Traverse` composition. This approves the
      preview framing and motion; the numbered production bundle below preserves it.
- [x] Produce the exact approved camera as numbered asset
      `Saved/CinematicRaw/1_manhattan_grand_traverse/`. The ID-only rename preserves
      the approved map, duration, lens and every camera key.
- [x] Scout Tower Canyon approach, gap and exit control points. Reject the first
      river-facing midpoint and both distant exits because they lose the tower-canyon
      subject; select the axis approach, close pre-gap and nearer river opening.
- [x] Preview `2_manhattan_tower_canyon`: a 22 s, approximately 100 m/s close pass with
      2.5 s handles. The sampled timeline shows the intentional near-tower occlusion
      clearing into river and city, with no unloaded boundary or stuck final motion.
- [x] Operator visually approved `2_manhattan_tower_canyon` on 2026-09-07; render and
      authenticate the production take without changing its approved camera.
- [x] Encode both approved Manhattan production masters with the repository-owned HEVC
      delivery route. Use the verified compact MP4s as the retained montage sources;
      the superseded authenticated ProRes masters may be cleaned after final-library
      promotion.
- [x] Preview and visually curate five additional Manhattan candidates: skyline
      compression, massing descent, Central Park glide, waterfront parallax and grid
      tilt reveal. Replace the redundant second park arc and remove frozen handles.
- [x] Preview and visually curate five Kazan candidates: Grand Traverse, canal-axis
      push, Kazanka parallax arc, river-crossing approach and Volga waterfront pull-in.
      Reject misleading Kremlin and Millennium labels when the blockout cannot prove
      those landmarks visually.
- [x] Encode all ten retained previews through the repository-owned HEVC route and run
      complete decode/frame-count checks. Treat their authenticated MOVs as disposable
      working masters after the approved cameras are produced and the compact final
      library is verified.
- [x] Operator approved all ten preview candidates; produce their exact accepted cameras.
- [x] Consolidate the twelve compact production files into one final library and verify
      metadata, frame counts, full decode, file hashes and manifest binding.
- [x] Remove superseded ProRes masters, preview bundles and `tmp/cinematic/` through
      the ProjectCinematic owner cleanup route while preserving the accepted final library.

## Verified so far

- Four refusal cases observed refusing: absent map, malformed plan, wrong-world capture,
  more than one video master.
- Smoke: 300 frames at 960x540 in 92 s including editor start. First production take:
  720 frames at 1920x1080 in 269 s.
- Cinematic-only 3 km radius proved by runtime log at `300000cm`. The exact same
  southwest pose changed from a flat unloaded background at 1 km to geographic
  water and distant city at 3 km.
- Retimed preview authenticated the requested Manhattan map, UE 5.8.1, 14 s, 420
  frames at 480x270 and scalability 2. Its master SHA-256 is
  `303258d6727e14430d7e2b420e9cf76d019848507274d1ab7179e0c7a07ae86f`.
- Diagonal preview authenticated the requested Manhattan map, UE 5.8.1, 18 s, 540
  frames at 480x270 and scalability 2. Its master SHA-256 is
  `10b53e692dea1f3a0c8ac0b886117129491059a478d0100ea55f80bc8c89a169`.
- Rejected v2 preview authenticated the requested Manhattan map, UE 5.8.1, 30 s,
  900 frames at 480x270 and scalability 2. Authentication did not override the visual
  failure at 16-20 s.
- Corrected v4 preview authenticated the requested Manhattan map, UE 5.8.1, 30 s,
  900 frames at 480x270 and scalability 2. Its plan SHA-256 is
  `05984b61651620a64ac722dba38801a44968e348372475cb65a3a13a86f2e385` and its
  master SHA-256 is
  `2f7c7f867d6f7a077d5121b875748815b6996519c60b1ffce9ef61019a724447`.
- Hero-ending v5 preview authenticated the requested Manhattan map, UE 5.8.1, 30 s,
  900 frames at 480x270 and scalability 2. Its plan SHA-256 is
  `d3c6e78b25241f7e5d319d082e9777a1d4f143550defd0f6faedf6ea78899bfc` and its
  master SHA-256 is
  `de409895448a8fe0405b39d84e2314bfd3c85cb5dd6f12a6b9d1ee45d7387dcf`.
- Operator visually approved that exact v5 preview as `Manhattan Grand Traverse` on
  2026-09-07. The approval does not promote the disposable preview to a release master.
- Numbered Manhattan Grand Traverse production authenticated the requested Manhattan
  map, UE 5.8.1, 30 s, 900 frames at 1920x1080 and scalability 2. Its plan SHA-256 is
  `2c08b150978a6c6fe85447da6d7de752bd80a967dc50d966817ef5e1c0636e93` and its
  master SHA-256 is
  `b2cdfee0cefabd92aa7ee1448484b26755559d54e4cbb953fcfa805bb675356a`.
  First, middle and last production frames visibly preserve the approved geographic
  start, dense traversal and skyline/river/park endpoint.
- Tower Canyon preview authenticated the requested Manhattan map, UE 5.8.1, 22 s,
  660 frames at 480x270 and scalability 2. Its plan SHA-256 is
  `8828bb3e4c85ed0660bcb35a64120c6db7f78882d41d5222c6b0d9cf418b8a63` and its
  master SHA-256 is
  `4353dc5dcfd40d0c681699063bef9ff1039ce26b9b5a3b7c84b6c03ee0be7c44`.
  First/middle/last plus 3-19.5 s timeline samples were visually inspected.
- Operator visually approved that Tower Canyon preview on 2026-09-07. The numbered
  production authenticated the requested Manhattan map, UE 5.8.1, 22 s, 660 frames at
  1920x1080 and scalability 2. Its plan SHA-256 is
  `0fee59dc6dda68972203327ea7e986b1671f5b4d4042edabbb83338776c60ac9` and its
  ProRes master SHA-256 is
  `49195f8d8f8b7e60fc902ce6df742500a3d5d87a4cbdece5ca5db6098281b3ab`.
  The literal first frame contains temporal warm-up residue, but samples from 0.5 s
  through the complete 2.5 s entry handle are stable and preserve the approved opening.
- `convert.ps1` produced verified HEVC Main 10 delivery files without changing either
  authenticated ProRes master. `1_manhattan_grand_traverse_enc.mp4` is 32,417,229 bytes
  (25.4x smaller), 30 s/900 frames, SHA-256
  `f20a57951409ee88addcc2b4173c4dca9c6aca7277c22cd111314b6c08e144d4`.
  `2_manhattan_tower_canyon_enc.mp4` is 20,194,304 bytes (27.3x smaller), 22 s/660
  frames, SHA-256
  `01a0f2c87bf75d6c9c23242921066edad9f5aa12afa0d25fb2ee6865bad5aab3`.
  Both are 1920x1080, 30 fps, `yuv420p10le`; complete decode checks passed and decoded
  midpoint frames visibly match their approved compositions.
- `Saved/CinematicRaw/` now contains numbered Grand Traverse and Tower Canyon production
  bundles plus the approved Tower Canyon preview. The 26 older unnumbered experiments were moved
  intact to the recoverable `Saved/CinematicRaw_PreNumbered/` holding directory because
  this execution environment refused permanent recursive deletion.
- This camera correction changed only its disposable plan and this todo. Existing
  in-progress `ProjectCinematic`, capture-script and durable-doc changes remain a
  separate review surface; this preview neither expands nor reverts them.
- Ten new preview receipts authenticate the requested Manhattan or Kazan map at 480x270,
  30 fps and 20-30 s. The capture route authenticated 6,480 MOV frames; all HEVC review
  copies independently decode to the same per-shot frame counts. The review copies total
  47.8 MB versus 1,240.0 MB for the masters and preserve HEVC Main 10 `yuv420p10le` output.
- Twelve-frame contact sheets for every retained preview were inspected for composition,
  loaded-world boundaries, mid-frame pop and final motion. No retained shot exposes a
  hard territory edge or frozen entry/exit handle.
- Operator approved all twelve shot framings. The ten newly produced masters were
  inspected through twelve-frame production contact sheets before encoding; no hard
  territory edge, mid-frame unloaded wall, frozen tail or production-only composition
  regression was found.
- `Saved/CinematicRaw/Final/` contains exactly twelve shot-named MP4 files plus
  `manifest.json`: 8,040 frames and 268 s total, split 154 s Manhattan / 114 s Kazan
  (57.5 % / 42.5 %), at 1920x1080, 30 fps, HEVC Main 10 `yuv420p10le`. The files total
  275,689,540 bytes (262.9 MiB). Every MP4 passed a complete FFmpeg decode with zero
  diagnostics; its frame count, byte count and SHA-256 match the accepted manifest.
- Direct recursive shell cleanup was refused before process creation. The existing
  ProjectCinematic cleanup owner gained an explicit fail-closed raw-shot mode instead:
  it requires an accepted `Final/manifest.json`, excludes `Final/`, and exposes dry-run
  before apply. Its accepted cleanup receipt removed 24 targets and 9,664,954,073 bytes,
  comprising 23 superseded capture bundles plus `tmp/cinematic/`.
- Governance text validator passed; no stable doc, script or config references this todo.
