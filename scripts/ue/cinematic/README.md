# Cinematic scripts

Source-of-truth helpers for the ALIS trailer pipeline. These are NOT every-render scripts - they are recovery / authoring tools.

Routing context: [docs/cinematics/README.md](../../../docs/cinematics/README.md). UE-side setup: [docs/cinematics/render_setup.md](../../../docs/cinematics/render_setup.md).

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

### `stamp_cinematic_events.py` -- full automation, recommended

Stamps a JSON event plan onto a LevelSequence's `AAlisCinematicProxy` Spawnable. After this script runs:

1. The LevelSequence has a Spawnable binding for `AAlisCinematicProxy` (created if absent, via `LevelSequenceEditorSubsystem` on UE 5.7+, with a fallback to the older `MovieSceneSequence` path).
2. The spawnable template's `ScheduledEvents` UPROPERTY contains the JSON's events.
3. At sequence playback, the proxy's `Tick` fires each method when proxy-elapsed time crosses its `offset_seconds`.

No Sequencer Event Track / Director Blueprint / endpoint binding needed -- the events run from the spawnable's own Tick. This sidesteps UE's fragile Python event-endpoint API.

Clock semantics: `offset_seconds` is measured from the proxy's `BeginPlay`, NOT from the LevelSequence playback timeline. For typical takes (no time dilation, modest warmup) the two clocks are close enough that "seconds since the start of the shot" works. If you need frame-accurate sync with the sequence timeline, drive the proxy from a Sequencer Event Track on the same Spawnable instead -- the proxy's BlueprintCallable methods (`OpenInventory`, `InteractFocused`, ...) are designed for that path too.

Idempotent: re-running with the same JSON overwrites the `ScheduledEvents` array. Re-running with a different JSON replaces the events.

Example event plan (`Saved/cinematic_events/example_kitchen.json`):

```json
{
  "sequence": "/Game/Cinematics/Takes/2026-05-18/Scene_1_02",
  "events": [
    { "offset_seconds":  0.0, "method": "OpenInventory"    },
    { "offset_seconds": 10.0, "method": "CloseInventory"   },
    { "offset_seconds": 10.0, "method": "OpenVitals"       },
    { "offset_seconds": 20.0, "method": "CloseVitals"      },
    { "offset_seconds": 20.0, "method": "OpenMindJournal"  },
    { "offset_seconds": 25.0, "method": "CloseMindJournal" }
  ]
}
```

Run via ue-mcp or the editor's `py` console:

```
py <project-root>/scripts/ue/cinematic/stamp_cinematic_events.py <project-root>/Saved/cinematic_events/example_kitchen.json
```

Result JSON is written to `Saved/cinematic_stamp_result.json`. Allowed methods (case-insensitive): `OpenInventory`, `CloseInventory`, `OpenVitals`, `CloseVitals`, `OpenMindJournal`, `CloseMindJournal`, `InteractFocused`, `InteractActor`.

`InteractActor` events accept an optional `"target_actor"` field with the soft path to the recorded interactable; the proxy resolves it at render and bypasses the focus trace. The editor-side cinematic-take stamper auto-writes this same JSON shape to `Saved/cinematic_events/<TakeName>.json` when Take Recorder finishes a take, so the script input format and the stamper output format are intentionally identical (you can hand-edit a stamped JSON and re-stamp it through this script).

Validation built-in: rejects unknown method names, negative times, times past sequence playback end (non-fatal warning), missing sequence, unbuilt proxy class, non-string `target_actor`.

### `ensure_cinematic_proxy_binding.py` -- minimal Phase 1 (binding only)

Lighter alternative if you want to author event keys manually via the Sequencer UI's Event Track. Ensures the proxy is bound + Event Track exists, nothing more.

```
py <project-root>/scripts/ue/cinematic/ensure_cinematic_proxy_binding.py /Game/Cinematics/Takes/2026-05-18/Scene_1_02
```

Useful when you want frame-by-frame manual choreography. For "lots of takes" with similar event patterns, prefer `stamp_cinematic_events.py` and check the JSON plans into version control.

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

## Not here

- Render-time MRQ launch commands: drive MRQ from the editor UI or via `unreal.MoviePipelineQueueEngineSubsystem` from a session that already has the preset asset patched.
