# Research Kazan UE 5.8 presentation capabilities

**Status:** DONE - RESEARCH COMPLETE; PRIORITY SELECTED
**Research progress:** 7/7 concerns complete
**Selected implementation:** Packaged playable tour (Track P)
**Closed:** 2026-08-26

## Outcome

The seven accepted concern packets were compared by product necessity, visible value,
dependency depth, authority/rollback risk, physical RTX 4070 risk, acceptance quality,
and unlock value. Track P is first because it is release-required, independent of the
material compiler, reversible, and establishes the real packaged-player truth surface
without migrating generated geography.

At this research close-out, Track V remained a mandatory later World-release gate and
was not current. Track P is now accepted, and Track V has been transferred to the
current Release 2.0.0 cinematic concern without reopening this completed research.

## Routes

- [Completed campaign](world_plan_kazan_presentation_campaign.md)
- [Completed Track P implementation](world_enable_kazan_playable_tour.md)
- [Release 2.0.0 cinematic workflow](../../00_current/20260902-1218_cinematic_build_agent_release_workflow.md)
- [Territory generation router](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory authority contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Visual Verification owner](../../../tools/World/VisualVerification/README.md)

## Closed concern register

| Concern | Decision at research close-out |
|---|---|
| Environment | R1 PASS; genuine runtime day/night through a thin ProjectWorldEnvironment adapter over Epic Day Sequence; static environment is rollback only |
| Landscape | R2 PASS; universal ProjectMaterial terrain resources plus ProjectWorld semantic binding; immutable authority migration at cutover |
| Water | R1 PASS; universal Single Layer Water resource plus existing ProjectWorld geometry; no second water authority |
| Roads | R1 PASS; universal textureless asphalt plus independent `presentation:v1` assignment on frozen geometry |
| Vegetation | R1 PASS; V0 diagnosis selects no-op, A1, G1, S1, and conditional D1 branches; no PCG or experimental Nanite Foliage authority |
| Buildings | R1 PASS; universal textureless B1 plus independent `presentation:v1` assignment on frozen massing |
| Playable tour + release capture | R1 PASS; Track P and Track V are separate mandatory release gates; Track P selected first |

## Selection boundaries

- Physical target: RTX 4070, D3D12, High, 2560x1440, 60 FPS.
- RTX 3060-class remains explicitly unqualified until real hardware exists.
- Canonical geography, generated packages/manifests, collision authority, and the
  selected `512/1536` runtime profile remain frozen.
- Track P does not consume ProjectMaterial and does not authorize Track V,
  presentation, or World-generation implementation.
- The material core remains the prerequisite for whichever material-dependent concern
  is selected later. Current material research does not unconditionally select
  Landscape as its first consumer.
- Effort and GPU risk labels used during comparison were estimates, not measurements.

## Reviewer corrections recorded

- Track P used the required flat current path during implementation and now lives in
  `todo/01_done/world/world_enable_kazan_playable_tour.md`.
  The proposed `todo/00_current/world/...` path contradicted the todo router.
- The claimed stale unconditional "Landscape first" wording was already absent from
  the current material packet; no material todo edit was needed.
- Track V fallback is permitted only after a confirmed Track P architecture violation
  and complete rollback. An ordinary failing focused test remains a Track P debug loop.
- Existing scripted/direct-position World gates do not prove real player flight input;
  Track P must add owner-correct control proof and character parity evidence.

## Completion

- [x] Environment research accepted.
- [x] Landscape research accepted.
- [x] Water research accepted.
- [x] Playable-tour/release-capture research accepted.
- [x] Roads research accepted.
- [x] Vegetation research accepted.
- [x] Buildings research accepted.
- [x] Cross-concern comparison completed.
- [x] Track P selected and routed as the only current World implementation todo.
- [x] Track V and all unselected concerns retained in backlog.
