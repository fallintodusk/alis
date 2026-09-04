# ALIS 2.0.0 Release Plan

**Status:** CURRENT RELEASE FOCUS - CINEMATIC ACTIVE
**Active concern:** Agent cinematic/release workflow. Showcase city is machine-complete with
operator visual acceptance.

What 2.0.0 still needs, in full: the release footage, and a packaging pass that actually
produces a complete game archive plus a complete developer payload. Nothing else gates it.

This is the single current product-focus and execution router. It aligns with
[VISION.md](../../VISION.md) without duplicating implementation detail from the two
current concern todos.

## Release promise

Kazan proved ALIS can reconstruct and ship one real city. Release 2.0.0 must make
that achievement easy to communicate and prove the same system can produce another
globally recognizable territory without a city-specific runtime fork.

```text
accepted playable Kazan
-> agent-authored, deterministic release footage
-> second recognizable city through the same generic pipeline
-> ALIS 2.0.0 global proof
```

## Required concerns

| Order | Concern | Current todo | State |
|---|---|---|---|
| Active | Direct and capture raw release footage | [Raw footage](20260903-2205_cinematic_direct_raw_release_footage.md) | Selected now; produces the raw Manhattan and Kazan material the release footage is cut from |
| Deferred | Agent cinematic release-render workflow | [Cinematic workflow](20260902-1218_cinematic_build_agent_release_workflow.md) | Retains only the deterministic release render bound to an accepted Candidate, and post-production |
| Complete | Global showcase city | [Showcase city](20260902-1218_world_build_global_showcase_city.md) | Manhattan Development + Shipping route proof accepted; operator visual acceptance granted 2026-09-03 |
| Required to publish | Distribution: game archive + developer payload | [Developer release transaction](publish_public_developer_release_transaction.md), [Assets mirror policy](publish_project_assets_mirror_policy.md) | See the distribution gate below |

Showcase city is done. The cinematic concern is selected next; do not execute both in
parallel. Distribution is not a feature concern but must work before anything can be
published.

## Accepted baseline

- Kazan Shipping product proof and operator walkthrough are complete.
- The exact accepted Candidate remains owner-generated evidence and is not rebuilt by
  this release-routing change.
- ProjectCinematic already owns deterministic Editor/MRQ capture and release binding.
- ProjectWorld already owns the generic canonical compilation and Unreal realization
  path used by Kazan.
- Old City 17 remains the survival/onboarding experience.
- Kazan and future reconstructed showcase maps are demo/scale surfaces: their normal
  product route starts the real character in `PreviewFlight` and does not require a
  survival micro-scenario.
- Reusable survival mechanics may remain available to explicit test operations, but
  they are not a Kazan or showcase-city product objective.

## Release gates

### Raw footage concern

- every shot states what a viewer who knows nothing about ALIS learns from it;
- framing approved from authenticated stills before any camera motion is authored;
- each take captured longer than its expected edit use, with handles both sides;
- every promoted master authenticated against the map it claims, and looked at;
- promoted durations sum to more than 60 seconds, Manhattan dominant.

### Release-render concern (deferred)

- existing ProjectCinematic owner renders and authenticates the selected product
  Candidate without becoming a Shipping dependency.

### Showcase-city concern

- read-only same-footprint source census before generation;
- reviewer decision that current admitted providers make the candidate worthwhile;
- one generic owner-correct compilation/realization path with no city branch;
- normal menu/loading/character/PreviewFlight packaged proof;
- operator visual acceptance if selected for the release Candidate.

### Distribution concern

What actually has to work to publish, beyond the two feature concerns:

- the packaging owner produces a COMPLETE game archive: the split 7-Zip release outputs from
  `scripts/ue/package/package_release.ps1 -CreateReleaseArchive` (GitHub-safe split at
  `-SplitSizeMB`, default 1700 MiB);
- the mirror owner produces a COMPLETE developer payload:
  `ALIS_DeveloperProject_v1_<identity>.zip` (`.001`/`.002` parts above the threshold) from
  `scripts/git/mirror/compose_developer_payload.py`, carrying the canonical ZIP bundles and
  active indexes that generation needs and that are too large to keep in git or LFS;
- the payload installs and verifies through `install_developer_payload.ps1` from a clean
  checkout, so the published source is actually usable for generation.

Known gaps recorded by their own owners, not re-derived here: the split 7-Zip outputs are not
yet wired into the GitHub release publish
(`TODO(ALIS-Release)` in `scripts/ue/package/package_release.ps1`), and GitHub draft release
staging/upload is unimplemented per
[Developer release transaction](publish_public_developer_release_transaction.md).

### Not a release blocker: Shipping performance delta

An operator-reported Shipping-only performance discrepancy did NOT reproduce in any
Development configuration. It is a watch item, not release scope, and it does not gate 2.0.0.

If someone happens to run the Shipping Candidate before publishing, using the same envelope as
the Development evidence (RTX 4070, 2560x1440, High level 2, same VSync/FPS-cap and window
mode) makes the observation comparable and worth recording. That is opportunistic, not
required.

Evidence and hypotheses:
[Shipping performance delta](../02_backlog/world/20260903-1545_world_manhattan_shipping_performance_delta.md).

No World optimization is justified by anything measured so far: long traversal showed bounded
cells (~24-25), flat memory (~2.2 GB), no drift, zero streaming failures, and GPU cost at or
below accepted Kazan.

## Release constraints

- Do not reopen accepted Kazan stabilization without a concrete regression.
- Do not weaken Candidate/source/composite binding for cinematic convenience.
- Do not add a new city provider, landmark hardcoding, or city-specific runtime fork
  inside the first showcase slice.
- Do not create a second render pipeline or expose cinematic tooling in Shipping.
- Do not promote, publish, or commit without the required operator boundary.

## Immediate execution

Collect the raw release footage through
[Raw footage](20260903-2205_cinematic_direct_raw_release_footage.md): scout each shot
family with authenticated stills, render the approved camera moves, and verify every
promoted master. The showcase-city concern is complete and needs nothing further.
