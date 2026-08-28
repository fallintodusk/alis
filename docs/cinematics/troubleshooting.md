# Cinematics Troubleshooting

Symptom -> cause -> fix for the trailer/cinematic pipeline. Crash dumps go to [../debugging/crash_investigation.md](../debugging/crash_investigation.md); this page covers the capture pipeline.

Router: [README.md](README.md). UE side: [render_setup.md](render_setup.md). Post-process: [ffmpeg.md](ffmpeg.md).

## Decals visible in viewport, missing in MRQ render

**Symptom:** decal projects fine in viewport / PIE, but disappears or looks incomplete in the rendered frames.

**Current ALIS production baseline** (`Content/Cinematics/MP_Config_Prod.uasset`):

- Override Anti-Aliasing: **OFF** (engine TSR drives AA)
- Temporal Sample Count: 7
- Engine Warm Up Count: 16
- Render Warm Up Count: 16
- Use Camera Cut For Warm Up: OFF
- Verified MRQ CVars (locked under render only):
  - `r.DBuffer=1`
  - `r.CustomDepth=3`
  - `r.MotionBlurQuality=4`
  - `r.PathTracing.SamplesPerPixel=256` (inert when PT off)
  - `r.PathTracing.MaxBounces=8` (inert when PT off)

**Do NOT add (these names do not exist in UE 5.7 and produce MRQ "no cvar by that name" warnings):**

- `r.Decals`
- `r.DBuffer.Decals`
- `r.PostProcessAAQuality`
- `r.PathTracing.Decals`
- `r.PathTracing.Decals.MaxCount`

**Fix order (stop when fixed):**

1. Verify decal actor direction/projection (red arrow points into the receiving surface).
2. Verify the receiving static mesh has **Receives Decals** checked.
3. Verify project setting **DBuffer Decals** is enabled (`Project Settings -> Rendering -> DBuffer Decals` ON).
4. Verify the production MRQ preset still has `r.DBuffer=1` in its Console Variables setting. Re-run `scripts/ue/cinematic/apply_prod_preset.py` if the preset was reverted.
5. If using Path Tracer for this shot, validate with `DumpCVars` for the exact name before adding any new override. Do not copy unverified decal CVars from older drafts of this doc.

## Render hangs, no progress on a frame

**Symptom:** MRQ sits on frame N forever; CPU/GPU idle.

Common causes:

- **Antivirus blocking ShaderCompileWorker.exe.** Confirmed case: [../debugging/cases/shader_compile_worker_hang.md](../debugging/cases/shader_compile_worker_hang.md). Whitelist the engine `Binaries/Win64/` folder.
- **World Partition streaming still resolving.** Let the editor sit on the start frame for 20-30s before launching MRQ.
- **Asset editor open with a stale compile.** Close all asset editors before MRQ.
- **Heavy Lumen scene cache rebuild.** First run after a content change pays the cache cost. Run the shot once at low quality first to warm the cache.

If unclear: break into the editor process, save a minidump, and follow [../debugging/crash_investigation.md](../debugging/crash_investigation.md).

## Output file is enormous

**Symptom:** finished mp4 is hundreds of MB for a 30s clip.

Causes and fixes:

- **MRQ is writing the mp4 directly.** Switch to PNG sequence + ffmpeg. UE's built-in encoder uses very high bitrates.
- **CRF too low.** Trailer encode at `-crf 18`. Quick share at `-crf 23`. See [ffmpeg.md](ffmpeg.md).
- **Wrong preset.** `-preset slow` shrinks the file vs. `medium`/`fast` at no quality cost (only encode time).
- **h264 when h265 is acceptable.** h265 saves 30-50% at the same quality. See [ffmpeg.md#smaller-file-same-quality-h265](ffmpeg.md#smaller-file-same-quality-h265).

## Output file plays in VLC but not in browser/Discord

**Symptom:** mp4 plays locally but fails to upload or shows a black box in Chrome/Discord.

Causes:
- `-pix_fmt yuv420p` missing - browsers need 4:2:0 chroma. Re-encode with `-pix_fmt yuv420p`.
- h265 without `-tag:v hvc1` - QuickTime won't play it. Add the tag.
- `+faststart` missing - file has to download fully before playback. Add `-movflags +faststart`.

## Movie renders folder safety

**Rule:** never touch files in `Saved/MovieRenders/` while a render is in progress.

UE writes frames in place during MRQ. A half-written `.png` or `.mov` is byte-indistinguishable from a finished one until the renderer closes the handle. Moving, renaming, or deleting mid-render either corrupts the in-flight frame or makes the next frame fail with a permission error.

Safe pattern:
1. Wait for MRQ to report "Finished" in the status bar.
2. Verify frame count matches expected (`ffprobe` for video, count files for sequence).
3. Then move/rename/copy.

## Decal looks wrong in render (different from viewport)

If the decal is present but visually different (washed out, wrong colour, doubled), the issue is usually motion blur or temporal accumulation:

- **Motion blur ghosting:** `r.MotionBlurQuality = 4` and lower `r.MotionBlurAmount` if the shot is fast.
- **TSR ghosting on the decal:** drop Temporal Sample Count to 1 and rely on spatial samples (8+).
- **Wrong sort order vs. another decal:** bump `SortOrder` on the decal actor.

## Path Tracer noise on hero shot

Path Tracer needs sample budget. Defaults are far too low for delivery quality.

```
r.PathTracing.SamplesPerPixel = 256   (512 for hero close-ups)
r.PathTracing.MaxBounces = 8
```

Use the **Reference Path Tracer** denoiser in Post Process Volume settings if you have it; otherwise expect 256+ spp for clean output.

## Take Recorder produced an empty Level Sequence

**Symptom:** recording stopped, but the asset is empty / missing tracks.

Causes:
- Source actor was garbage-collected before recording ended (rare; happens with possessed pawns dying mid-record).
- Take Recorder lost focus and a hotkey stopped it.
- Project save was triggered mid-record - this can drop the in-progress track.

Re-record. There is no recovery path for an empty take asset.

## ffmpeg "no such file" with `%04d` pattern

**Symptom:**
```
[image2 @ ...] Could find no file with path 'MyShot.%04d.png' and index in the range 0-4
```

Causes:
- Frame numbering does not start at `0` or `1`. Check the first file; if MRQ wrote `MyShot.0050.png` as the first frame (because the sequence starts at frame 50), pass `-start_number 50`.
- Frames are not contiguous - one missing frame breaks the sequence. Re-render the missing range.
- Padding width is wrong: `MyShot.50.png` needs `%d`, `MyShot.0050.png` needs `%04d`.

## Expected warnings (conditionally benign)

These fire once per render and are benign **only when the documented follow-up succeeds**. If the follow-up doesn't appear in the same render, treat the warning as a real failure.

- `LogInteraction: Warning: [InteractionComponent] SetupPostProcess: No CameraComponent found on owner 'DefinitionCharacter_...'` -- modular pawn creates its camera lazily in `ApplyViewConfig` (after `EAssemblyState::Ready`). The per-tick retry catches it and logs `SetupPostProcess: SUCCESS - PP material added` within a second. **Benign only when that SUCCESS line appears for the same pawn shortly after.** If SUCCESS does not appear, the highlight post-process was not installed and the interaction highlight will not render. SOT for the retry mechanism: comment block at `Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp:145`.

## Related

- Engine-side crash/hang archive: [../debugging/cases/](../debugging/cases/)
- Crash investigation procedure: [../debugging/crash_investigation.md](../debugging/crash_investigation.md)
- ASCII-only doc rule (affects scripts pasted here): [../../CLAUDE.md](../../CLAUDE.md)
