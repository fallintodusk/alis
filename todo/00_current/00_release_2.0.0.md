# ALIS 2.0.0 Release Plan

**Status:** CURRENT RELEASE FOCUS - READY FOR FINAL SOURCE FREEZE
**Active concern:** Public game archive and developer payload transaction.

What 2.0.0 still needs in the repository is the distribution pass that produces a
complete game archive plus a complete developer payload. The approved raw-footage
library is sufficient for the external human montage and does not gate packaging.

This is the single current product-focus and execution router. It aligns with
[VISION.md](../../VISION.md) without duplicating implementation detail from the
current concern todo.

## Release promise

Kazan proved ALIS can reconstruct and ship one real city. Release 2.0.0 must make
that achievement easy to communicate and prove the same system can produce another
globally recognizable territory without a city-specific runtime fork.

```text
accepted playable Kazan
-> second recognizable city through the same generic pipeline
-> approved raw-footage library
-> complete player and developer distribution
-> ALIS 2.0.0 global proof
```

## Required concerns

| Order | Concern | Current todo | State |
|---|---|---|---|
| Complete | Direct and capture raw release footage | [Raw footage](../01_done/tools/20260903-2205_cinematic_direct_raw_release_footage.md) | Twelve-shot final library verified; durable workflow is routed by the ALIS cinematic skill and stable cinematic docs |
| Complete | Global showcase city | [Showcase city](../01_done/world/20260902-1218_world_build_global_showcase_city.md) | Manhattan Development + Shipping route proof accepted; operator visual acceptance granted 2026-09-03 |
| Active | Distribution: game archive + developer payload | [Developer release transaction](20260813-1511_publish_public_developer_release_transaction.md) | Single current implementation concern; see the distribution gate below |

Showcase city and raw-footage capture are done. The speculative cinematic-workflow
plan was dissolved because its useful outcome was achieved through the smaller proven
raw-capture route. Distribution is now the single selected concern.

## Accepted baseline

- Kazan Shipping machine proof and the pre-freeze operator walkthrough are complete.
  The final tracked release-plan correction changes source identity, so one replacement
  source-bound Candidate and short operator product/UX walkthrough remain.
- The exact pre-freeze Candidate remains owner-generated evidence. Its
  replacement is required by the final tracked source change, not by cinematic
  or routing convenience.
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

The local source, player archive, developer payload, privacy report, dependency
closure, and clean-clone evidence are prepared. Production signing and GitHub draft
publication remain explicit operator/external gates in the
[Developer release transaction](20260813-1511_publish_public_developer_release_transaction.md);
they are not hidden side effects of the package command.

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

Freeze tracked source, then finish the replacement Candidate and the
human/external gates in the reviewed
[Developer release transaction](20260813-1511_publish_public_developer_release_transaction.md)
as the single current concern. The human montage may consume
`Saved/CinematicRaw/Final/` independently; do not rerender approved shots or reopen
cinematic engineering without a new concrete defect.
