# Fix Capture Readiness for Mesh Terrain

**Status:** PROTOTYPE - not investigated and not approved. Run `/investigate-change` before any
implementation; nothing below is a decision or a plan.
**Owner:** the World capture route: `ProjectWorld.CaptureEvidence` (ProjectWorldEditor), its
wrapper `scripts/ue/world/capture_visual_evidence.ps1`, and the capture stage of
[Visual Verification](../../tools/World/VisualVerification/README.md)
**Origin:** P1 of `20260930-2139_content_restore_black_box_independence.md`

## Problem

On Kazan, the capture route can return an `accepted` receipt whose frames show buildings, roads,
and water over an empty sky-coloured void: the Mesh Terrain is not drawn. The receipt cannot tell,
because `accepted` only means the requested map was open, every vantage rendered, the files hash,
and no two poses are byte-identical. The route is operator evidence, never a gate on its own, but
its readiness wait exists to prevent visually false frames and does not cover Mesh Terrain. Such
frames mislead an operator and confound before/after comparisons: P1's first baseline was
unusable for this reason.

## Verified evidence

- Readiness today
  (`Plugins/World/ProjectWorld/Source/ProjectWorldEditor/Private/ProjectWorldEditorModule.cpp:74-217`,
  `ProjectWorldEvidenceReadiness.h`): the command loads the full World Partition editor bounds,
  then captures after three distinct frames with zero `FAssetCompilingManager` remaining assets,
  or writes a rejected receipt after 180 s. Under `-unattended` the editor exits right after.
  Nothing checks that a subject is drawn.
- That wait is the fix for Landscape-era checkerboard frames
  ([ProjectWorld pitfall 16](../../Plugins/World/ProjectWorld/docs/pitfalls.md)); terrain is now
  Mesh Terrain.
- Reproduced at least twice through the wrapper alone, read-only and with nothing saved: P1's
  first baseline (2026-09-30), and a rerun with an accepted receipt whose oblique frame shows no
  terrain (2026-10-01, `tmp/material/blackbox/capture_readiness/`). P1's driver sessions, which
  loaded every actor descriptor and let the editor run at least 150 frames and 45 s twice before
  invoking the same command, drew the terrain (`tmp/material/blackbox/p1/`). Both folders are
  transient evidence.
- A separate limit, not this defect: Editor worlds never generate ProjectTexture's runtime pattern
  pixels (`Plugins/Resources/ProjectTexture/Source/ProjectTexture/Private/ProjectTextureRuntimeSubsystem.cpp:25-30`),
  so pattern-driven appearance is absent from every Editor capture whatever the readiness.

## Consumers

- `tools/World/VisualVerification/app/verify_capture.py` (re-hashes files, blind to content),
  `scripts/ue/world/test/water_temporal_stability.ps1` when it is given Editor captures (the
  release's Shipping Water proof feeds it frames from the packaged playable tour instead,
  `scripts/ue/world/test/performance/run_kazan_playable_tour.ps1`), cinematic scouting stills
  ([raw capture](../../docs/cinematics/raw_capture.md)), and operator evidence for World changes.
- No 3.0.0 release gate uses this route: R2 step 6, operator visual acceptance of the terrain
  material, is a packaged property of the frozen candidate. Do not route a release gate through
  this capture until the fix lands.

## For the investigation

- Which installed-engine signal says Mesh Partition sections are built, registered, and rendering
  in an Editor world? Read the installed source before choosing.
- Wait on that signal, check that the subject is present (for example terrain primitives in the
  capture frustum), or both? Whatever is chosen must first reject the reproduced no-terrain case
  on Kazan (the known-bad control) and accept a session that draws the terrain.
- Should the receipt record a terrain readiness fact, so a missing subject is visible without
  opening the PNGs?
- Whether pattern-driven appearance needs a generation-world capture (PIE or game preview) is a
  separate decision; do not fold it in.
- When the fix lands: update the README capture section and pitfall 16, and correct two stale
  README lines ("route not yet implemented" under Output locations; "the sweep test" under Scope).

## Boundaries

- Reproduce and prove read-only on existing generated content: no World Apply, regeneration, cook,
  or package.
- The capture stays operator evidence, never an acceptance gate on its own.

## Authority register

- **D1** Track this in `00_current` as a prototype, and run `/investigate-change` before
  implementing.
  - Reason: not stated.
  - Date/source: 2026-10-01, operator request.
