# Cinematics

Router for the ALIS authored capture pipeline: Sequencer and Take Recorder own
shots, Movie Render Queue renders them, and ffmpeg creates delivery encodes.

## Pipeline

```
Sequencer / Take Recorder
  -> Movie Render Queue (MRQ)
    -> PNG/EXR sequence or ProRes
      -> ffmpeg encode (h264/h265 mp4)
        -> trailer / web upload / archive
```

UE handles capture and per-frame rendering. ffmpeg handles container, codec, size, and trim/concat. Do not try to do encoding inside UE - MRQ's built-in mp4 path is fragile compared to a clean image sequence + ffmpeg.

The unattended Kazan route is:

```powershell
.\scripts\ue\cinematic\run_release_capture.ps1
```

It validates one schema-bound request, renders the authored sequence locally in the
launcher Editor, binds the receipt to the accepted packaged release, and promotes
`Saved/CinematicRelease/Kazan/Current` while retaining `Previous` as rollback.

## Files

- [render_setup.md](render_setup.md) - UE side: Take Recorder workflow, MRQ config, decal-safe CVars, recommended output formats.
- [ffmpeg.md](ffmpeg.md) - Post-process: encoding presets, image sequence to mp4, h264 vs h265, size reduction, trim/concat, ffprobe path on this machine.
- [troubleshooting.md](troubleshooting.md) - Common capture-time issues (missing decals in render, hangs mid-render, oversized files).
- [ProjectCinematic](../../Plugins/Editor/ProjectCinematic/README.md) - ownership,
  Shipping boundary, stable Kazan command, evidence, and cleanup.

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
- One MRQ preset per delivery target. The current source-controlled recipes own
  `Content/Cinematics/MP_Config_Dev.uasset` and `MP_Config_Prod.uasset`.
- Never commit raw image sequences. Commit only the final mp4 (or keep it out of git entirely - see the gitignore for `Saved/MovieRenders/`).
