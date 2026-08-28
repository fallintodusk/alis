# ProjectCinematic

Editor-only owner for authored gameplay recording and deterministic Movie Render
Queue execution. Packaged gameplay has no dependency on this plugin.

## Ownership

- Sequencer and CineCamera assets own shot composition and camera motion.
- `ACinematicGameMode` adapts normal single-player startup for Record and Render.
- Record keeps input and gameplay UI active for Take Recorder.
- Render blocks input, hides the phantom gameplay pawn, removes viewport widgets,
  and follows the active camera with a World Partition streaming source.
- Scripts own request validation, MRQ execution, evidence, `Current`/`Previous`
  promotion, and owner-scoped cleanup.

ProjectCinematic does not generate autonomous camera paths and ProjectWorld exposes
no cinematic API.

## Kazan release capture

Run from the repository root:

```powershell
.\scripts\ue\cinematic\run_release_capture.ps1
```

The schema-bound request is
`scripts/ue/cinematic/requests/kazan_release_v1.json`. It binds the authored map,
sequence, preset, frame range, operator acceptance receipt, and exact Shipping
Candidate. The wrapper independently recomputes the package tree and staged
executable hashes, then cross-checks source, runtime-profile, release-operation,
and composite identity before launching the Editor. A missing or mismatched
acceptance refuses before render.

Promoted output:

```text
Saved/CinematicRelease/Kazan/Current/
Saved/CinematicRelease/Kazan/Previous/
```

`Current/receipt.json` authenticates the Editor/MRQ execution separately from the
accepted Shipping executable and package hashes. MRQ may not claim to be the Shipping
runtime or silently increase product scalability, LOD, or streaming quality.

## Shipping boundary

The plugin modules and capture dependencies are Editor-target only. Packaging also
excludes authoring content. The package audit checks staged manifests, loose files,
and IoStore contents; dependency metadata is not mistaken for executable payload.

## Cleanup

The capture wrapper deletes each run's `tmp/cinematic/release_capture/<run-id>` tree.
For completed diagnostics, run:

```powershell
.\scripts\ue\cinematic\cleanup_workspace.ps1
.\scripts\ue\cinematic\cleanup_workspace.ps1 -Apply
```

Cleanup preserves both promoted output slots.
