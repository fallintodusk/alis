# Cinematic scripts

Source-of-truth helpers for the ALIS trailer pipeline. These are NOT every-render scripts - they are recovery / authoring tools.

Routing context: [docs/cinematics/README.md](../../../docs/cinematics/README.md). UE-side setup: [docs/cinematics/render_setup.md](../../../docs/cinematics/render_setup.md).

## Unattended Kazan release capture

```powershell
.\scripts\ue\cinematic\run_release_capture.ps1
```

The wrapper validates `requests/kazan_release_v1.json`, cold-starts the launcher
Editor, drives MRQ, verifies the master, binds it to the accepted packaged release,
and promotes `Saved/CinematicRelease/Kazan/Current` plus one `Previous` rollback.
The authored LevelSequence remains shot authority; the script does not invent camera
motion. Use `audit_capture_package.ps1` to prove capture payload is absent from the
package, and `cleanup_workspace.ps1 -Apply` for completed owner diagnostics.

## Directed raw shot capture

```powershell
.\scripts\ue\cinematic\run_shot_capture.ps1 -PlanPath scripts\ue\cinematic\shots\<world>\<shot_id>.json
```

Renders one directed camera move into a raw editable master under
`Saved/CinematicRaw/<world>/<shot_id>/`. The committed shot plan is the source of
truth and the LevelSequence it builds is compiled output. This route performs no
release binding and never touches the release outputs above. Contract:
[docs/cinematics/raw_capture.md](../../../docs/cinematics/raw_capture.md).

## What lives here

### MRQ preset recipes -- `mrq_config_updater.py` + `apply_*_preset.py`

Two MRQ primary configs ship with ALIS:

| Asset | Purpose | Recipe |
|---|---|---|
| `Content/Cinematics/MP_Config_Prod.uasset` | Trailer-grade render: 1920x1080 @ 60fps, TSR + 8 temporal samples, 32+32 warmup, MotionBlurQuality=4. | [apply_prod_preset.py](apply_prod_preset.py) |
| `Content/Cinematics/MP_Config_Dev.uasset` | Fast preview for movement-artefact checks: 960x540 @ 24fps, 4 temporal samples, 8+8 warmup, MotionBlurQuality=2. Same frame count and motion-blur smoothness as Prod, ~10-30x faster. | [apply_dev_preset.py](apply_dev_preset.py) |

The engine-side logic lives in [mrq_config_updater.py](mrq_config_updater.py); each `apply_*_preset.py` is **pure data** (Output, AntiAliasing, ConsoleVariables, WidgetRenderer, GameOverride sections) plus a call to `run_preset(...)`. The updater:

- Patches only the sections the caller declares -- unrelated settings stay untouched.
- Refuses to create a fresh `MoviePipelineAntiAliasingSetting` when the preset lacks one (it would clobber sample counts / AA method to engine defaults).
- Removes stale CVar names listed under `console_variables.remove_stale` so old patch runs don't leave invented entries behind.
- Resolves the GameMode soft path via `unreal.load_class` (UE Python rejects raw `FSoftClassPath` for that property).
- Writes a full before/after diff to `Saved/cinematic_apply_<flavour>_result.json` so re-runs are auditable.

Verified CVars come from UE 5.7's `DumpCVars` output. Made-up names cause MRQ to log "no cvar by that name" warnings -- neither recipe contains any such name. The path-tracer entries in Prod are inert when Path Tracing is off (current project default) and provide a sane fallback when a shot enables it manually.

### When to run

You do NOT need to run these for every render. Run them when:

- The preset asset is deleted, reverted, or accidentally edited via the MRQ UI.
- A new preset asset is created and needs the same baseline -- copy one of the recipes, change `ASSET`, edit the dicts.
- You want to inspect what's currently on disk before changing anything -- use [inspect_mrq_config.py](inspect_mrq_config.py).

### Running

From inside a running editor (via ue-mcp or the editor's `py` console):

```
py <project-root>/scripts/ue/cinematic/apply_prod_preset.py
py <project-root>/scripts/ue/cinematic/apply_dev_preset.py
py <project-root>/scripts/ue/cinematic/inspect_mrq_config.py /Game/Cinematics/MP_Config_Dev
```

Each writes its result to `Saved/cinematic_apply_<flavour>_result.json` (or `cinematic_inspect_<asset>.json` for the inspector).

### Why these scripts exist when the asset is already patched

The `.uasset` is the compiled output; the `apply_*_preset.py` files are the human-readable recipes. The asset can be deleted, corrupted, version-controlled-reverted, or unintentionally edited via the MRQ UI -- the recipe encodes the intent. Same pattern as a database migration: state persists in the asset, but the migration stays in source control so the schema can always be reconstructed.

### `convert.ps1` -- batch ProRes .mov -> NVENC HEVC mp4

Scans `Saved/MovieRenders/` for `.mov` files. For each one, if `<name>_enc.mp4` already exists, skips it. Otherwise verifies the .mov is stable (not mid-render) and encodes it with the production-blessed NVENC HEVC CQ 19 command from [docs/cinematics/ffmpeg.md](../../../docs/cinematics/ffmpeg.md#prores-mov-re-encode-nvenc-hevc-hardware-accelerated).

Safety: never globs blindly. Every `.mov` goes through a 5-second size-stability check before being touched. Files still being written are skipped with a clear message instead of corrupted. See the standing rule in [docs/cinematics/troubleshooting.md](../../../docs/cinematics/troubleshooting.md#movie-renders-folder-safety).

Run via the Makefile (preferred):

```bash
make cinematics-convert
```

Or directly:

```powershell
.\scripts\ue\cinematic\convert.ps1
.\scripts\ue\cinematic\convert.ps1 -InputDir "E:\other\folder" -StabilityWaitSeconds 10
```

ffmpeg resolution order: `-FfmpegPath` arg -> `Get-Command ffmpeg` -> Kdenlive fallback at `C:\Program Files\Kdenlive\bin\ffmpeg.exe`.

## Manual authoring remains available

Use the Editor UI for shot authoring, Take Recorder, and one-off diagnostic renders.
Release automation consumes those authored assets; it does not replace them.
