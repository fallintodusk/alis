# Render Setup (UE side)

How to capture a shot in UE so it survives Movie Render Queue intact. Covers Take Recorder, MRQ config, decal-safe CVars, and output format choice.

Router: [README.md](README.md).

## Automated Kazan release route

Run the stable wrapper from the repository root:

```powershell
.\scripts\ue\cinematic\run_release_capture.ps1
```

The request names the map, authored LevelSequence, CineCamera cut, preset, frame
range, accepted packaged-release receipt, and package root. The wrapper cold-starts
the launcher Editor, renders through MRQ, verifies frame count/duration, records peak
memory and hashes, promotes `Current`, retains one `Previous`, and removes its tmp.

Release footage uses `ACinematicGameMode` in Render mode. It blocks input, hides the
gameplay pawn, removes viewport widgets, and keeps World Partition residency near the
active Sequencer camera. Record mode keeps normal input and UI for Take Recorder.

## Take Recorder

Use Take Recorder for gameplay-driven shots (player walking, NPC behaviour, physics). Use plain Sequencer for hand-keyed shots.

Setup:
1. Window -> Cinematics -> Take Recorder.
2. Add **Source** for each tracked actor (player pawn, AI, camera). Avoid recording the whole world.
3. Set **Slate** and **Take** number per shot - the take name is used as the output folder.
4. Root Take Save Dir defaults to `/Game/Cinematics/Takes/` - keep it there so the existing folder convention holds.
5. Record. The resulting `LevelSequence` asset is your input to MRQ.

Notes:
- Take Recorder writes assets while recording. Do not save the editor mid-record.
- Long takes (multi-minute) bloat asset sizes fast; prefer multiple shorter takes you can stitch later.

## Movie Render Queue (MRQ)

Open: Window -> Cinematics -> Movie Render Queue. Add the LevelSequence, then click the config name to edit settings.

### Production preset (current)

The source-controlled release presets are `Content/Cinematics/MP_Config_Dev.uasset`
and `Content/Cinematics/MP_Config_Prod.uasset`. Their recipes live beside the capture
scripts. Dev is the fast structural/visual gate; Prod is the final-quality target.

**Output:**
- Output Directory: `{project_dir}/Saved/MovieRenders/` (flat - one `.mov` per sequence, not a per-shot subfolder)
- File Name Format: `{sequence_name}.{frame_number}` (with ProRes export the `{frame_number}` token collapses; final file is `{sequence_name}.mov`)
- Output Resolution: 1920x1080
- Use Custom Frame Rate: ON, **60 fps**
- Override Existing Output: ON (re-renders overwrite cleanly)
- Handle Frame Count: 0, Output Frame Step: 1
- Use Custom Playback Range: OFF (uses LevelSequence range)
- Auto Version: ON

**Image Output Format (Exports):**
- **Apple ProRes [10bit]** with Codec = **Apple ProRes 422 HQ**.
- Drop Frame Timecode: OFF, Include Audio: OFF, Override Max Encoding Threads: OFF.
- ProRes 422 HQ runs ~432 Mbps for 1080p60 - a 30 s shot lands around 1.5 GB. That is intentional: ProRes is the mastering codec. Compress to deliverables via NVENC HEVC post-encode (see [ffmpeg.md](ffmpeg.md#prores-mov-re-encode-nvenc-hevc-hardware-accelerated)).
- Alternative for long takes or grade-pipeline shots: switch the export to **.png Sequence (8bit)** (lossless, resumable, frame-numbered) or **.exr Sequence** (linear, for DaVinci/AE comp). See [Sequence vs. ProRes](#sequence-vs-prores).
- Do **not** use MRQ's built-in mp4/avi exports - encoding inside UE is the wrong layer; let ffmpeg do it.

**Anti-Aliasing:**
- Override Anti Aliasing: **OFF** (the engine's per-camera TSR drives AA - cleanest result).
- Spatial Sample Count: **1**
- Temporal Sample Count: **7** (TSR accumulates these into a smoothly motion-blurred frame; odd values land a sample at frame-center, which is cleaner on animated impacts than even values).
- Use Camera Cut for Warm Up: OFF
- Render Warm Up Frames: OFF
- Render Warm Up Count: **16**
- Engine Warm Up Count: **16**

The values above are the **source-controlled baseline** (set by `scripts/ue/cinematic/apply_prod_preset.py`). The pre-2026-05-19 baseline (TSC=8, warmup=32+32) was reduced to target a ~30% wall-clock saving with no visible difference in TSR-accumulated motion blur or first-frame convergence. To revert, edit the recipe and re-run.

Earlier notes warned that high Temporal Sample Counts break DBuffer decals - that applies when `Override Anti Aliasing` is ON with a non-TSR method. With TSR (Override OFF), TSC = 7 is the right value for film-quality motion blur and does not break decals.

**Game Overrides:**
- Game Mode Override: `/Script/ProjectCinematic.CinematicGameMode`.
- Cinematic Quality Settings: OFF.
- Texture Streaming: project truth (`NONE`), not forced off.
- LOD zero, HLOD disabling, shadow/view-distance inflation, grass cull inflation,
  and grass-density overrides: OFF.

The release render must show the same product-quality world accepted by the packaged
game. MRQ is an offline renderer, not permission to hide streaming/LOD defects.

**Deferred Rendering:**
- Accumulator Includes Alpha: OFF
- Disable Multisample Effects: OFF
- Additional Post Process Materials: 2 array elements (project-specific post FX; keep as authored).
- Render Main Pass: ON
- Add Default Layer: OFF
- Actor Layers: 0, Data Layers: 0

**Console Variables (verified against UE 5.7 registry, 2026-05-18):**
The preset bundles a `Console Variables` setting MRQ applies at render start and restores at render end. Every CVar listed below has been confirmed to exist via `DumpCVars` output - names that the engine does not recognise produce "no cvar by that name" warnings in the MRQ message log and are NOT included here.

| CVar | Value | Why |
|---|---:|---|
| `r.DBuffer` | 1 | DBuffer decal path. Project default is 1; locked under MRQ to survive any scalability swing. |
| `r.CustomDepth` | 3 | Custom-depth + stencil pass. Required for the interaction highlight post-process material (see [Highlight in trailers](#highlight-in-trailers)). 3 = depth + stencil; gives headroom for stencil-driven highlight variants (per-category colour, etc.) without changing the project default of 1. Render-time only; reverts on render end. |
| `r.MotionBlurQuality` | 4 | Highest quality motion blur. Project default is 0; we push to 4 for trailer cinematic look. |
| `r.PathTracing.SamplesPerPixel` | 256 | Path Tracer SPP baseline. Inert when Path Tracing is off (project default `r.PathTracing=0`). When a shot enables PT manually this provides a clean fallback instead of `-1` (which means "use Post Process Volume value"). |
| `r.PathTracing.MaxBounces` | 8 | Path Tracer ray-bounce cap. Same inert-when-off behaviour as `SamplesPerPixel`. |

The earlier draft of this section also included `r.Decals`, `r.DBuffer.Decals`, `r.PostProcessAAQuality`, `r.PathTracing.Decals`, and `r.PathTracing.Decals.MaxCount`. **Those CVar names do not exist in UE 5.7** - the engine ignores them with a warning. They have been removed. For future path-tracer tuning, additional verified knobs include `r.PathTracing.MeshDecalRoughnessCutoff`, `r.PathTracing.MeshDecalBias`, `r.PathTracing.MaxRaymarchSteps` (see `DumpCVars` for the full registered set).

There is no global "enable decals" CVar in UE 5.7 - decals are gated by `r.DBuffer` (above) and by individual decal-actor properties.

**UI Renderer pass:**
The presets retain `Widget Renderer` so an explicitly authored UI capture can be
composited. The automated Kazan release render intentionally removes normal gameplay
UMG from the viewport in `ACinematicGameMode`; its aerial master must be clean. Record
mode retains the normal UI for the operator. Do not infer visible HUD from the presence
of the Widget Renderer setting alone.

### Saving the config as a preset

In the MRQ config window, **Presets -> Save As Preset**. Store under `Content/Cinematics/Presets/MRQ_<target>.uasset` (e.g. `MRQ_Trailer_ProRes_1080p`, `MRQ_Archive_EXR_4K`). Reuse across shots; do not re-tune per shot. Resave when a setting actually changes.

### Reapplying the patch from script

The complete recipes are encoded in
[`apply_dev_preset.py`](../../scripts/ue/cinematic/apply_dev_preset.py) and
[`apply_prod_preset.py`](../../scripts/ue/cinematic/apply_prod_preset.py). Shared
idempotent asset mutation lives in
[`mrq_config_updater.py`](../../scripts/ue/cinematic/mrq_config_updater.py).

From an open editor with `ue-mcp` running, or via the editor's `py` console command:

```
py <project-root>/scripts/ue/cinematic/apply_dev_preset.py
py <project-root>/scripts/ue/cinematic/apply_prod_preset.py
```

Each recipe owns one source-controlled preset identity; do not retarget it ad hoc.
Result JSON is written under `Saved/`. Script README:
[scripts/ue/cinematic/README.md](../../scripts/ue/cinematic/README.md).

## Output format decision matrix

| Goal | MRQ output | Post step |
|---|---|---|
| Web trailer (YouTube, Discord, Drive) - **current default** | ProRes 422 HQ .mov | ffmpeg -> NVENC HEVC mp4 (`_enc` suffix) |
| Archive / master, hand-graded later | PNG or EXR sequence | ffmpeg -> h265 mp4, keep sequence |
| Compositing in DaVinci/AE | EXR sequence (linear) | grade -> render in NLE |
| Quick share with team | ProRes .mov OR PNG sequence | ffmpeg -> h264/h265, crf 22-26 |

## Sequence vs. ProRes

UE can write ProRes directly OR as a frame-numbered image sequence. The production preset uses ProRes because the trailers shot so far are short (5-30 s) and the editor has been stable through them. Switch to a PNG/EXR sequence when one of these is true:

1. **Long takes (multi-minute).** A ProRes render that dies at frame 4800 of 5000 restarts from frame 0; a sequence resumes from the missing frame.
2. **Downstream comp/grade.** DaVinci/After Effects expect frame-numbered sequences for frame-accurate edits and re-export.
3. **EXR-only data needed.** Linear scene-referred colour, deep AOVs, multi-channel.

For everything else, ProRes 422 HQ is a single self-contained file that is trivially fed to ffmpeg post-encode.

## Highlight, HUD, and gameplay systems in trailers

The preset uses `/Script/ProjectCinematic.CinematicGameMode`. This Editor-only
adapter inherits normal `ASinglePlayerGameMode` startup without introducing a
ProjectSinglePlay dependency on cinematic tooling.

- Record mode keeps input, pawn, UI, and gameplay interaction active for Take Recorder.
- Render mode lets Sequencer own the camera, blocks player movement/turning, hides the
  phantom gameplay pawn, and removes viewport UMG for a clean master.
- Recorded custom-depth tracks remain the highlight authority during render; do not
  add a second live highlight command or cinematic event bus.
- A shot that intentionally needs UI must author that presentation explicitly and pass
  its own visual gate; the Kazan aerial release request does not carry gameplay HUD.

The stable implementation and invocation contract lives in
[ProjectCinematic](../../Plugins/Editor/ProjectCinematic/README.md).

## Dynamic spawns in trailers (Take Recorder side)

ALIS spawns actors from definitions during gameplay (loot drops, definition-driven world objects, inventory containers). Take Recorder must capture both kinds and both are mandatory:

- **Persistent actors that exist at recording start** (player pawn, placed level dressing) -> recorded as **Possessable**. Sequence drives the existing instance instead of creating a clone.
- **Actors that spawn during the take** (definition-driven, gameplay-spawned) -> recorded as **Spawnable**. Sequence recreates them on the same frames at render time.

**This split is the requirement; whether ALIS's Take Preset already does it is NOT verified yet.** Take Recorder's "vanilla" defaults are not a guarantee for ALIS's `ObjectDefinition` / async-spawn paths. Treat this as a contract to verify, not a capability to assume.

Two practical knobs to set on the project's Take Preset before recording:

- **Player source -> Record to Possessable.** Stops the sequence from cloning the pawn at render (the cinematic gamemode spawns the real one).
- **World source** added (or the "record nearby spawns" option on the player source enabled). One of these captures mid-take spawns as Spawnables.

After a test take, **open the LevelSequence in Sequencer and inspect the bindings list directly**: confirm a single Possessable for the player pawn + a Spawnable entry for every actor that came into existence mid-take. Don't infer this from "it looked right in the render".

Acceptance - what the Sequencer outliner must show after a test take:

- a Camera Cuts track (from Take Recorder);
- the player pawn as a single **Possessable** binding, with its
  transform/anim/audio tracks;
- for every actor the player interacted with, a **Possessable** binding plus
  child Possessable bindings for each of its components, each carrying a
  transform track with the real sampled motion;
- a **Spawnable** entry for every actor that came into existence mid-take;
- a master Event Track at the sequence root with one trigger key per UI panel
  toggle.

Scrubbing the playhead must reproduce the motion the player saw, including
spring overshoot and settle. UI events do NOT fire on scrub - that is an engine
limitation and expected; press Play to see them.

## Path Tracer

Only enable Path Tracing when the deferred Lumen pass is not good enough for the deliverable (raytraced caustics, ground-truth GI). Tradeoffs:
- 10x-100x slower per frame.
- Translucent materials behave differently - re-validate every shot.
- The preset's Console Variables setting already includes `r.PathTracing.SamplesPerPixel=256` and `r.PathTracing.MaxBounces=8` as inert baselines; both activate automatically if a shot turns Path Tracing on. No extra CVars needed for decals (UE 5.7 handles them via `r.PathTracing.MeshDecalBias` / `MeshDecalRoughnessCutoff` defaults; only override if `DumpCVars` confirms the exact name and you have a specific tuning need).

## Editor stability tips

- Close all asset editors before starting MRQ.
- **Disable the MRQ in-render preview window** (the "Render Preview (Low Quality)" panel). It re-rasterizes to a separate output every frame and costs GPU time. Hide it from the MRQ UI before clicking Render.
- Set `r.OneFrameThreadLag = 0` for the render session if you see ghosting tied to subsampling.
- If the project has heavy World Partition streaming, let the editor sit on the start frame for 20-30s before launching MRQ so streaming completes.
- Antivirus killing `ShaderCompileWorker.exe` will silently hang the render - see [../debugging/cases/shader_compile_worker_hang.md](../debugging/cases/shader_compile_worker_hang.md). Add an AV exclusion for `ShaderCompileWorker.exe` on the render machine; first-render shader-cache misses become free instead of stalling for minutes.
