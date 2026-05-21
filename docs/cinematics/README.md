# Cinematics

Router for the ALIS trailer / cinematic capture pipeline: Take Recorder and Movie Render Queue inside UE, ffmpeg outside.

## Pipeline

```
Sequencer / Take Recorder
  -> Movie Render Queue (MRQ)
    -> PNG/EXR sequence or ProRes
      -> ffmpeg encode (h264/h265 mp4)
        -> trailer / web upload / archive
```

UE handles capture and per-frame rendering. ffmpeg handles container, codec, size, and trim/concat. Do not try to do encoding inside UE - MRQ's built-in mp4 path is fragile compared to a clean image sequence + ffmpeg.

## Files

- [render_setup.md](render_setup.md) - UE side: Take Recorder workflow, MRQ config, decal-safe CVars, recommended output formats.
- [ffmpeg.md](ffmpeg.md) - Post-process: encoding presets, image sequence to mp4, h264 vs h265, size reduction, trim/concat, ffprobe path on this machine.
- [troubleshooting.md](troubleshooting.md) - Common capture-time issues (missing decals in render, hangs mid-render, oversized files).

## Related

- Crash / hang case archive: [../debugging/cases/](../debugging/cases/)
- Engine config: [../../Config/DefaultEngine.ini](../../Config/DefaultEngine.ini)
- Cinematics content folder: [../../Content/Cinematics/](../../Content/Cinematics/)
- Output location safety rule (do not touch mid-render): [troubleshooting.md#movie-renders-folder-safety](troubleshooting.md#movie-renders-folder-safety)

## Conventions

- **Default trailer master: Apple ProRes 422 HQ .mov.** Short stable shots (5-30 s) under a clean editor render directly to ProRes 422 HQ via the production preset. One self-contained file, then ffmpeg post-encodes to delivery mp4.
- **PNG sequence** for long takes, resumable renders that may fail mid-way, or any shot that will be hand-graded in DaVinci/After Effects.
- **EXR sequence** only when linear scene-referred colour, deep AOVs, or multi-channel compositing is needed.
- **Final delivery mp4 is produced by ffmpeg, not MRQ.** UE's built-in mp4 path is fragile compared to a clean ProRes/sequence master + ffmpeg pass.
- One MRQ preset per delivery target. Save presets under `Content/Cinematics/Presets/` (current production: `Pending_MoviePipelinePrimaryConfig_Final.uasset`).
- Never commit raw image sequences. Commit only the final mp4 (or keep it out of git entirely - see the gitignore for `Saved/MovieRenders/`).
