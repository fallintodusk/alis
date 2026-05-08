# ALIS Trailer — UE 5.7 Capture Spec

How to capture trailer footage with **Take Recorder** (records gameplay actor state) and render with **Movie Render Queue** (master ProRes + EXR proxies). Shot list in [trailer_storyboard.md](trailer_storyboard.md). Concept and constraints in [trailer_plan.md](trailer_plan.md).

This spec follows current (2025–2026) UE 5.7 indie cinematic best practice. See [References](#references) at the end for primary sources.

## MRG vs MRQ — for this trailer, stay on MRQ

Movie Render Graph (MRG) is the long-term replacement for MRQ — Beta in 5.5, hardened in 5.7, future of the renderer. **For a single-shot 60-second hero trailer with no per-layer compositing, stay on MRQ.** It is still the documented production path and the simpler preset. Switch to MRG only if a shot needs render passes / layered EXR.

## Workflow per shot

### 1. Set up the scene

1. Open `City17_Persistent_WP`
2. Drag any actors the shot needs into the level (Hero spawns from GameMode; gramophone / loot / corpse / props drag from Content Drawer)
3. Engine Scalability → **Cinematic** (top-right gear in viewport)
4. Lighting → `Build → Build Lighting Only → Production` for atmospheric shots

### 2. Record gameplay via Take Recorder

1. **Window → Cinematics → Take Recorder**
2. **Slate** = descriptive name (`gramophone_shot`, `parkour_shot`, etc.)
3. **+ Source → From Player** — captures Hero with full anim/customization
4. PIE → in console (` ` `):
   ```
   ShowFlag.DebugDrawing 0
   ShowHUD 0
   t.MaxFPS 60
   ```
5. **Record** in Take Recorder → play your scene → **Stop**
6. Master Sequence saved to `/Game/Cinematics/Takes/<date>/<slate>_<take>.uasset`

### 3. Add cinematic camera in post

For **hero shots** (gramophone, City17 wide, Hero face reveal): use a **Camera Rig** instead of a free CineCamera — gives smooth, repeatable, non-jittery motion that survives re-cuts.

| Use case | Asset | Notes |
|---|---|---|
| Jib / boom (rise above wide City17) | **Camera Rig Crane** | UE 5.6+; animate Crane Pitch / Crane Yaw / Crane Arm Length in Sequencer |
| Dolly / tracking (Hero walking) | **Camera Rig Rail** | UE 5.6+ supports animated focal length / aperture / focus along the rail |
| Static / mounted (gramophone closeup) | plain `CineCameraActor` | piloted into position, transform locked |

Steps:
1. Open the recorded master Sequence (double-click)
2. Click the **camera icon** in Sequencer toolbar — auto-spawns `CineCameraActor` + creates Camera Cut Track + binds it
3. **Pilot** the camera (right-click → Pilot, or Ctrl+Shift+P), or attach the camera to a Camera Rig Crane / Rail dragged into the level
4. Frame the shot, set Cine Camera → Current Camera Settings:
   - Filmback: `16:9 Digital Film`
   - Focal Length: 28–50mm wide / 85–135mm closeup
   - Aperture: f/2.0–2.8 (cinematic DoF) or f/5.6–8 (sharp)
   - Focus Method: Manual, distance to subject (or Tracking Focus on the action)
5. Keyframe Crane / Rail / camera transform in Sequencer for movement
6. **Camera Shake**: keyed off the **Camera Cut Track**, not the camera actor — this way shake survives re-cuts and edits. Subclass `MatineeCameraShake` per intensity.

### 4. Smooth recorded camera transforms

If recorded gameplay path is jittery: Curve Editor → select Player Transform keys → **Simplify Curves** (tolerance 0.05–0.1) for natural ease.

### 5. Color pipeline — set up before any render

Set this **before the first preview render**, otherwise grading work in DaVinci is wasted. ALIS targets a consistent color-managed pipeline so DaVinci viewport matches UE viewport.

1. **UE side** — enable plugins: `OpenColorIO`, `Apple ProRes Media`. Restart editor.
2. Create an OCIO Configuration asset pointing at an `aces_1.3` or `aces_2.0` studio config (UE 5.7 supports ACES 2.0).
3. In MRQ → +Setting → **Color Output**:
   - Source = `Working Color Space (Linear)`
   - Destination = `ACES 1.0 SDR Video` (or `sRGB - Display` if no ACES pipeline downstream)
   - `Disable Tone Curve` = checked **only if exporting EXR for downstream grading**; leave unchecked for ProRes deliverables
4. **DaVinci side** — Project Settings → Color Management:
   - DaVinci YRGB Color Managed
   - Input Color Space = `ACEScg`
   - Timeline Color Space = `ACEScct`
   - Output Color Space = `Rec.709 Gamma 2.4`
   - Result: Resolve viewport matches UE viewport on import; grading does not fight tone-mapping.

### 6. Render via MRQ

**Window → Cinematics → Movie Render Queue** → +Render → pick the Sequence. Configure as a saved preset (`MRQ_Trailer.uasset`) and reuse across all shots.

#### Settings panel — must-have nodes

| Node | Why | Config |
|---|---|---|
| **Game Overrides** | force scalability to cinematic at render time regardless of editor session state — most-skipped MRQ setting | tick `Cinematic Quality Settings` and `Flush Streaming Managers`; set Game Mode Override to default |
| **Color Output** | feeds the OCIO pipeline | per [section 5](#5-color-pipeline-set-up-before-any-render) |
| **Anti-Aliasing** | quality of edges and motion | per [AA table below](#anti-aliasing-table) |
| **Console Variables** | targeted defect fixes only — do not paste big lists | per [CVar block below](#cvar-block) |
| **Output** | codec, path, frame range | ProRes 422 HQ master + EXR proxy when grading is needed |
| **High Resolution** | only for stills / posters, NOT for 1920×1080 video | leave at default |

#### Anti-aliasing table

Spatial-8 / Temporal-1 is wrong for moving cameras in 5.7. Pick by motion:

| Shot type | Motion Blur Amount | AA Method | Spatial samples | Temporal samples |
|---|---|---|---|---|
| Camera or subjects in motion (default) | **enabled, default 0.5** | TSR | **1** | **9–15** (odd, hits keyframe) |
| Static or near-static subject | enabled | TSR | **9–15** | **1** |
| Thin geometry / wires / foliage detail | as above | **None** | high (8+) | 1 |
| Path-traced shot | n/a (forced off) | **None** (PT requires it) | n/a | n/a — PT uses its own SPP |

Mix-and-match (Spatial high + Temporal high) is wasted compute. Pick a regime per shot.

#### CVar block

Targeted, scoped to defects. Add via MRQ → Console Variables setting on the preset.

| CVar | Value | Purpose |
|---|---|---|
| `r.Streaming.PoolSize` | `8000` | streaming texture pool large enough for cinematic detail |
| `r.Streaming.HLODStrategy` | `2` | force highest-quality HLOD at distance |
| `r.MotionBlurQuality` | `4` | best motion-blur quality |
| `r.PostProcessAAQuality` | `6` | best post-process AA |
| `r.Lumen.HardwareRayTracing` | `1` | HW RT for Lumen GI |
| `r.Lumen.HardwareRayTracing.LightingMode` | `2` | Hit Lighting (cinematic-quality Lumen) |
| `r.Lumen.ScreenProbeGather.RadianceCache.NumProbesToTraceBudget` | `200` | richer Lumen probes |
| `r.RayTracing.Shadows` | `1` | RT shadows (sharp contact, no shadow map dithering) |
| `r.SkyAtmosphere.AerialPerspectiveLUT.SampleCount` | `16` | better atmospheric haze in wide City17 shot |
| `r.VolumetricFog.GridPixelSize` | `4` | denser volumetric fog grid (cinematic light shafts) |
| `foliage.DensityScale` | `1` | full foliage density at render (City17 foliage just restored — see [restore_lost_foliage_instances.md](../restore_lost_foliage_instances.md)) |

Avoid the temptation to paste a 50-CVar dump. Each one is a documented fix for a documented problem.

#### Output codec strategy

| Codec | Use | Bit depth | Why |
|---|---|---|---|
| **Apple ProRes 422 HQ (`.mov`)** | master deliverable | 10-bit | trailer codec — visually indistinguishable from 4444 at 1080p, half the file size |
| **EXR (Multilayer), Compression: PIZ** | grading proxy for hero shots only | 16-bit float | full headroom for grade-then-conform in DaVinci |
| ProRes 4444 | only if compositing with alpha | 12-bit | overkill for this trailer's deliverables; reserve for title-card alpha exports from Remotion |
| PNG sequence | preview-only | 8-bit | fast iteration, do not ship |

#### Settings table

| Setting | Preview | Master |
|---|---|---|
| Resolution | 1920×1080 | 1920×1080 |
| Frame rate | 60 fps | 60 fps |
| Codec | PNG sequence | Apple ProRes 422 HQ (`.mov`) + EXR proxy on hero shots |
| AA | per [AA table](#anti-aliasing-table), Spatial 1 / Temporal 4 for preview | per [AA table](#anti-aliasing-table) for final |
| Motion blur | enabled, default shutter | enabled, default shutter |
| Engine warmup | 32 | **64** (see [gotchas](#known-gotchas-ue-57)) |
| Output dir | `{project_dir}/Saved/Movies/Trailer/` | same |
| Output naming | `<slate>` | `<slate>` |

**Iteration rhythm**: preview = ~5 min/shot (dial in framing), master = ~30–90 min/shot (final).

## Path Tracer — per-shot decision, not the default

For ALIS, use Path Tracer on emotional / static / hero shots (gramophone closeup, Hero at window). Use **Lumen** (with the CVars above) on action shots (parkour, sniper, snap-cut barrage). Path Tracer is too slow per frame for action footage and the visual gain is wasted under fast motion.

Path Tracer settings when used:

| Setting | Value | Notes |
|---|---|---|
| Samples Per Pixel | 256–1024 | start 256, push to 1024 for hero-quality stills |
| Max Bounces | 8–16 | 8 for ALIS interior gramophone shot is enough |
| Reference Motion Blur | enabled | required if any movement in frame |
| Denoiser | **NFOR (Temporal)** | new in 5.7, strictly better than OIDN/Optix for sequences |
| AA Method | **None** | PT requires it; mixing PT + TSR is invalid and produces ghost frames |

Render time at 1080p: ~5–30 min/frame (compare to ~5–30 sec/frame Lumen). Budget accordingly.

## Audio at capture — mute UE, mix in DaVinci

Add `au.MuteAudio 1` (or simply do not enable an Audio Submix in MRQ). In-engine audio bleeds from non-deterministic sources (footstep variants, Niagara, ambient zones) and contaminates the master. Author music (Chaliapin) + SFX (needle drop, vinyl scratch, sniper crack, percussive barrage) cleanly in DaVinci Fairlight from clean stems.

## Notes

**Data-driven Hero**: drag `Hero.Hero` from Content Drawer. In editor preview shows as default UE mannequin (BeginPlay doesn't fire for editor-placed actors). **In MRQ render the capabilities run and Hero assembles correctly with full Mutable customization** — verified 2026-05-02.

**Lighting per chunk** (per [trailer_storyboard.md](trailer_storyboard.md)):
- **Open / Close** — golden hour through curtains, warm. Sun pitch −3° to −10°. Light temperature 2800K–3500K. Practical lights only (lamps).
- **Body** — neutral daylight, slightly cooler. Saves warmth for the framing chunks.
- **Climax (snap-cut barrage)** — mixed; cuts intentionally clash temperatures shot-to-shot to amplify chaos.

**UI compositing for shots 4, 7, 8** (VitalsHUD, InventoryPanel, DialoguePanel): in-engine via WidgetComponent in Sequencer (preferred) OR alpha-pass render + DaVinci composite (backup if data-driven UI doesn't init in MRQ).

## Known gotchas (UE 5.7)

- **Niagara emitters render empty on frame 1.** Engine Warm Up Count ≥ 64 *and* extend the sequence start back ~2 s before the visible cut so emitters tick.
- **Materials with WPO (foliage wind, character cloth) skip first-frame motion vectors** → ghosting under Temporal samples. Same fix: pre-roll + warmup.
- **Foliage HISM under Hardware Ray Tracing**: enable `r.RayTracing.InstancedStaticMeshes 1` and `r.RayTracing.Geometry.InstancedStaticMeshes.EvaluateWPO 1`, otherwise foliage casts wrong shadows under PT / Lumen HWRT. Note this conflicts with the project's current crash-mitigation CVar `r.RayTracing.Geometry.InstancedStaticMeshes=0` in [Config/DefaultEngine.ini](../../../Config/DefaultEngine.ini); the trailer render preset must override it locally — see [improve_foliage_lumen_quality.md](../improve_foliage_lumen_quality.md).
- **MRQ "black first frame"**: disable `r.OneFrameThreadLag` via console var, or raise warmup.
- **TSR + Path Tracer is invalid** — PT requires AA Method = `None` and Reference Motion Blur enabled.
- **"Accumulator Includes Alpha" breaks Niagara** unless a mesh sits behind it; leave off unless you need a true alpha pass.
- **Apple ProRes writer on Windows occasionally drops the final frame**; render +1 frame past the cut.
- **BeginPlay-driven assembly (ALIS Hero)** only fires under PIE / MRQ — not in editor preview. Keep test runs through MRQ, not the viewport.

## Folder layout

| Path | Purpose | Tracked |
|---|---|---|
| `Cinematics/Takes/<date>/<slate>_<take>.uasset` | Take Recorder output | yes |
| `Cinematics/Trailer/MRQ_Trailer.uasset` | MRQ preset (settings + CVars + AA + Color Output) | yes |
| `Cinematics/Trailer/Trailer_<Theme>.uasset` | named master sequences per theme | yes |
| `Saved/Movies/Trailer/<slate>.mov` | MRQ render output (ProRes 422 HQ) | no (gitignored) |
| `Saved/Movies/Trailer/EXR/<slate>/` | hero-shot EXR proxies for grading | no (gitignored) |

## References

UE 5.7 official:
- [Transitioning to MRG from MRQ](https://dev.epicgames.com/documentation/en-us/unreal-engine/transitioning-to-the-movie-render-graph-from-movie-render-queue-in-unreal-engine)
- [Rendering High-Quality Frames with MRQ](https://dev.epicgames.com/documentation/en-us/unreal-engine/rendering-high-quality-frames-with-movie-render-queue-in-unreal-engine)
- [NFOR Denoiser](https://dev.epicgames.com/documentation/en-us/unreal-engine/nfor-denoiser-in-unreal-engine)
- [Anti-Aliasing and Upscaling](https://dev.epicgames.com/documentation/en-us/unreal-engine/anti-aliasing-and-upscaling-in-unreal-engine)
- [Camera Jibs and Dollies](https://dev.epicgames.com/documentation/en-us/unreal-engine/camera-jibs-and-dollies-in-unreal-engine)
- [Movie Render Graph Nodes Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/movie-render-graph-nodes-in-unreal-engine)

Community guides (2025):
- [HyperRender — UE5 MRQ Settings Guide 2025](https://www.hyperrender.run/blog/ue5-mrq-settings-guide-2025)
- [80.lv — UE5 → DaVinci Color Grading 2025](https://80.lv/articles/color-grading-tutorial-unreal-engine-5-to-davinci-resolve)
- [VPI Film Guide — Unreal OCIO Setup](https://vpifg.com/guides/unreal-ocio/)
- [Versluis — OCIO + ACES in UE (Oct 2025)](https://www.versluis.com/2025/10/ocio-workflow-with-aces-in-unreal-engine/)
- [William Faucher — Movie Render Queue Series](https://www.biunivoca.com/public/en/series/william-faucher-movie-render-queue-series/episodes/6)
- [Epic Forum — MRQ Warmup and First-Frame Issues](https://forums.unrealengine.com/t/tutorial-movie-render-queue-warmup-and-first-frame-issues/760883)
- [CG Channel — UE 5.7 Five Key Features](https://www.cgchannel.com/2025/11/unreal-engine-5-7-five-key-features-for-cg-artists/)
