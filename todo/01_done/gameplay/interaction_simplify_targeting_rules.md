# Interaction Targeting - Replace Score Blend with Comparator Chain

**Status:** Implemented and verified (2026-04-27). Build clean on `ProjectInteraction` and `ProjectIntegrationTests`. Full `ProjectIntegrationTests.Interaction.Targeting` suite green: **13/13 tests pass**.
**Priority:** Major
**Date:** 2026-04-26 (planned), 2026-04-27 (landed)
**Builds on:** commit `b8e123f17` (ray-hit `TargetPoint`, `WorldStatic` overlap, regression test)

## Verification log

### v1 (2026-04-27 first pass)

Run via persistent editor (`scripts/ue/test/unit/persistent_editor_run.ps1`) after rebuilding both modules with `scripts/ue/build/rebuild_module_safe.ps1`:

| Test | Result |
|---|---|
| `RayPiercedFrontTargetBeatsRayPiercedBackTarget` | PASS (14.2s, cold first run) - **but false positive: LOS rejected back box before comparator ran** |
| `RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget` | PASS (11.2s) |
| `ConeFallbackMostCenteredWinsWhenRayMisses` | PASS (11.2s) |
| `PriorityBreaksOnlyAfterCenterednessAndDistanceTie` | PASS (11.2s) |
| `HysteresisKeepsCurrentFocusOnNearTie` (geometry updated for same-bucket) | PASS (13.2s) |
| Full `ProjectIntegrationTests.Interaction.Targeting` gate | PASS - 13/13 (14.8s warm) |

### v2 fixes (2026-04-27 second pass, after code review)

Two issues caught and fixed:

1. **`RayPiercedFrontTargetBeatsRayPiercedBackTarget` was a false positive.** `SpawnInteractable` blocks all channels by default; the front box rejected the back candidate via the LOS gate before `IsBetterCandidate` ever saw two pierced candidates. Fix: front box now ignores `ECC_Visibility` (via `SetCollisionResponseToChannel`) so the LOS trace from view origin to the back candidate's `TargetPoint` is unobstructed. This better matches the real "window does not block visibility" case in PIE.
2. **`ShouldReplaceComponent` did not consider `bViewRayPierced`.** For an actor with multiple interactable primitives, the per-actor selection could drop a pierced component for a non-pierced one before the comparator chain ever ran. Fix: per-actor selection now prefers ray-pierced after the target-mesh-match check and before distance, mirroring the global comparator's bucket logic.

### v2 verification

Re-rebuilt both modules and re-ran:

| Test | Result |
|---|---|
| `RayPiercedFrontTargetBeatsRayPiercedBackTarget` (now real comparator-only setup) | PASS (11.3s) |
| Full `ProjectIntegrationTests.Interaction.Targeting` gate | PASS - 13/13 (14.2s warm) |

The front/back test now legitimately exercises the pierced-vs-pierced comparator path: both candidates survive cone, both survive LOS, both pierced, smaller `ViewRayHitDistance` wins.

### v3 model change (2026-04-27 third pass, after PIE testing)

PIE testing in the actual game world exposed a deeper design flaw: **the "pierced beats non-pierced" rule used `LineTraceComponent` as a proxy for "centered", and that proxy is wrong when collision shape doesn't match the visible mesh.** The classic case is a window glass mesh with collision authored only on the wooden frame: the player aims at the glass, the simple-collision trace returns no hit, the kitchen window falls into the non-pierced bucket while a different window across the street (with cleaner collision) falls into the pierced bucket. The pierced-beats-non-pierced rule then picks the wrong window even though the kitchen one is at screen center.

**Fix: drop the pierce concept entirely. `AimDot` is the single centeredness metric.**

Changes in v3:

1. **`FInteractionTargetCandidate`**: dropped `bool bViewRayPierced` and `float ViewRayHitDistance`. Only `Actor`, `Component`, `TargetPoint`, `Distance`, `AimDot`, `Priority` remain.
2. **`ResolveTargetPoint`**: now returns just the point. Three-step resolution: (a) `LineTraceComponent` for accurate impact when collision matches mesh, (b) `FMath::LineExtentBoxIntersection` against the primitive's bounds AABB - catches window-glass-style cases where the visible mesh has no collision but the player is clearly visually pointing at it, (c) closest-point-on-collision as final fallback.
3. **`IsBetterCandidate`**: flat 4-step chain - higher `AimDot`, smaller `Distance`, higher `Priority`, smaller `ActorID`. No bucket separation.
4. **`ShouldReplaceComponent`**: per-actor primitive selection now uses `AimDot` as the primary key (same metric as the global comparator), with target-mesh-match still winning first.
5. **`ShouldKeepCurrentCandidate`**: single-rule hysteresis on `AimDot` percentage; cross-bucket logic gone.
6. **Resolve loop**: `AimDot` is computed per primitive in the first pass and cached in the candidate state; the second pass just reads it.

The bounds-AABB fallback is the key fix for the windows scene: when `LineTraceComponent` misses the glass (no collision there), the bounds box is what the player visually sees, so a bounds-on-ray entry point gives `AimDot = 1.0` for the kitchen window. The across-street window also gets `AimDot = 1.0`, but the kitchen one wins on the distance tiebreak because it's closer.

### v3 verification

Rebuilt both modules and re-ran:

| Test | Result |
|---|---|
| Full `ProjectIntegrationTests.Interaction.Targeting` gate | PASS - 13/13 (14.8s warm) |

### v4 review correction (2026-04-27 fourth pass)

Reviewer rejected v3's full flattening: dropping `bViewRayHit` and going to pure-AimDot loses the binary "is the view ray actually crossing this candidate" signal. AABB bounds is loose for rotated meshes, so a bounds-hit candidate flattened to `AimDot = 1.0` lets large rotated meshes steal focus. The correct shape: **keep the bucket comparator, but expand the "hit" bucket to include both collision AND bounds intersections.**

Changes in v4 (relative to v3):

1. **Restored fields** on `FInteractionTargetCandidate`: `bool bViewRayHit` (renamed from `bViewRayPierced` since a bounds intersection is not a physical pierce) and `float ViewRayHitDistance`.
2. **`ResolveTargetPoint`** returns a small `FResolvedTargetPoint` struct: `Point`, `bViewRayHit`, `ViewRayHitDistance`. Both collision-hit and bounds-hit set `bViewRayHit = true` with their respective distances; closest-point fallback leaves `bViewRayHit = false` and `ViewRayHitDistance = MAX`.
3. **Comparator chain restored to bucket form**:
   1. View-ray-hit beats fallback.
   2. Among view-ray-hit: smaller `ViewRayHitDistance` (front-most) wins.
   3. Among fallback: higher `AimDot` (most centered in cone) wins.
   4-6. Distance, Priority, ActorID tiebreaks.
4. **`ShouldReplaceComponent`** mirrors the same buckets per-actor: target-mesh -> view-ray-hit -> hit-distance / AimDot -> Distance -> ComponentID.
5. **`ShouldKeepCurrentCandidate`** restored: cross-bucket always switches; same-bucket uses the relevant discriminator with `FocusSwitchHysteresis` percentage.
6. **Debug log** prints both `rayHit=` and `rayHitDist=` alongside `aim`/`distance`/`priority`.

Why this is the right shape for the windows scene:

- **Big kitchen window**: collision is sparse on the glass -> `LineTraceComponent` misses -> bounds AABB intersects ray -> `bViewRayHit = true`, `ViewRayHitDistance = ~150`.
- **Across-street window**: clean collision -> `LineTraceComponent` hits -> `bViewRayHit = true`, `ViewRayHitDistance = ~1500`.
- Both view-ray-hit -> tiebreak by hit distance -> kitchen wins.

For bottle-vs-door:
- **Bottle**: collision-hit -> `bViewRayHit = true`.
- **Off-axis door**: ray crosses neither collision nor bounds -> `bViewRayHit = false`.
- Hit beats fallback -> bottle wins. Door's high priority cannot rescue it.

### v4 verification

Rebuilt both modules and re-ran:

| Test | Result |
|---|---|
| Full `ProjectIntegrationTests.Interaction.Targeting` gate | PASS - 13/13 (14.8s warm) |

All 13 existing tests stay green under the restored bucket model:
- `RayPiercedFrontTargetBeatsRayPiercedBackTarget`: both view-ray-hit (collision), front wins on smaller hit distance.
- `RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget`: small is view-ray-hit (collision), off-axis door is fallback (no ray cross). Hit > fallback.
- `ConeFallbackMostCenteredWinsWhenRayMisses`: both fallback, AimDot decides.
- `PriorityBreaksOnlyAfterCenterednessAndDistanceTie`: locks the demoted priority.
- `HysteresisKeepsCurrentFocusOnNearTie`: same-bucket hysteresis on AimDot.

Manual PIE check still needed for the windows scene to confirm the bounds-AABB fallback fixes the actual game-world bug.

### v5 tuning (2026-04-27 fifth pass)

After v4 PIE testing the cone overlap radius was retuned from `300 cm` to `200 cm`. Rationale: 3 m gathered too many far candidates that the player wasn't aiming at; 2 m matches arm's reach plus enough margin for floor pickups when the player bends. The "do NOT shrink" advice in the original v2 planning section is therefore superseded by this retune.

Test geometry adjustment: `RayPiercedFrontTargetBeatsRayPiercedBackTarget` had Back at +220 cm (now outside the 200 cm overlap sphere). Moved Back to +180 cm. The test still legitimately exercises pierced-vs-pierced - both candidates inside the cone, both view-ray-hit, smaller `ViewRayHitDistance` wins.

Files changed: `InteractionComponent.h`, `InteractionTargetResolver.h` (cosmetic struct default), `InteractionTargetResolverIntegrationTests.cpp`, `README.md`. Gate stays 13/13.

## Problem

In-PIE testing of `b8e123f17` surfaced a second focus failure: aiming at a tall foreground window with a smaller window in line behind/above selects the small window. Root cause - both windows are pierced by the view ray, so both saturate `AimScore = 1.0`. The deciding factor falls to the score blend's `DistanceWeight` and `PriorityWeight`, neither of which represents "what is in the foreground at the crosshair". The score blend conflicts with the player rule.

## Player rule (single sentence)

**The most centered candidate wins. If multiple candidates are equally centered, the closer one wins.**

Pierced candidates count as "fully at center". Among them, "closer" means smaller ray hit distance (front-most). Non-pierced candidates rank by `AimDot`. Distance is only ever a tiebreak.

## Comparator chain (deterministic, six steps)

`IsBetterCandidate(A, B)` becomes:

1. Pierced beats non-pierced. (`A.bViewRayPierced != B.bViewRayPierced` -> the pierced one wins.)
2. Among pierced: smaller `ViewRayHitDistance` wins.
3. Among non-pierced: higher `AimDot` wins.
4. Tiebreak: smaller `Distance`.
5. Tiebreak: higher `Priority`.
6. Final tiebreak: `Actor->GetUniqueID()` (deterministic).

Use `FMath::IsNearlyEqual` for float comparisons in steps 2-4 to avoid floating-point flicker.

## Why this is simple

- Pierced candidates collapse to one bucket ("at center").
- Non-pierced rank by aim only.
- Distance never outweighs aim - only a tiebreak.
- Priority is a final tiebreak, not a force - it can no longer steal aim from a centered candidate.
- No weights to tune. The score blend goes away entirely.

## Resolver state changes

### `FInteractionTargetCandidate` ([InteractionTargetResolver.h](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionTargetResolver.h))

Add:
- `bool bViewRayPierced = false;`
- `float ViewRayHitDistance = TNumericLimits<float>::Max();`

Drop:
- `float Score = 0.0f;`

This is `bViewRayHit` brought back, but with a comparator that actually consumes it.

### `ResolveTargetPoint` ([InteractionTargetResolver.cpp](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionTargetResolver.cpp))

Currently returns just `TargetPoint`. Change signature to also propagate `bViewRayPierced` and `ComponentHit.Distance` out (out-params or a small struct). When `Component->LineTraceComponent` succeeds, set `bViewRayPierced = true` and record `ComponentHit.Distance`. The fallback closest-point branch leaves both at their defaults.

### `IsBetterCandidate` ([InteractionTargetResolver.cpp](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionTargetResolver.cpp))

Replace with the six-step comparator chain above. Delete the score-computation block (current resolver lines ~345-359).

## Hysteresis migration (must do at the same time)

Current hysteresis lives in [InteractionComponent.cpp](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp) `ShouldKeepCurrentFocus` and reads `CurrentFocus.Score` / `NewWinner.Score`. Removing `Score` breaks this file - the build will fail unless hysteresis is migrated in the same change.

**Chosen approach: move the decision into the resolver, comparator-aware.**

Replace the existing helper with a single resolver-side function the component calls:

```
bool FInteractionTargetResolver::ShouldKeepCurrentCandidate(
    const FInteractionTargetCandidate& Current,
    const FInteractionTargetCandidate& NewWinner,
    float FocusSwitchHysteresis);
```

Semantics:
- If `Current.Actor == NewWinner.Actor` -> false (same target, no switch).
- If buckets differ (pierced vs non-pierced) -> false (always switch; no hysteresis can save a non-pierced incumbent from a pierced challenger, and vice versa is impossible).
- If both pierced -> keep current if `Current.ViewRayHitDistance <= NewWinner.ViewRayHitDistance * (1 + FocusSwitchHysteresis)`. (Smaller hit distance is "better"; allow incumbent a small leeway in absolute terms.)
- If both non-pierced -> keep current if `Current.AimDot >= NewWinner.AimDot * (1 - FocusSwitchHysteresis)`. (Higher AimDot is "better"; same percentage logic as before, just on AimDot instead of Score.)

This is the smallest honest replacement: same `FocusSwitchHysteresis` semantics ("a small handicap to keep the incumbent stable"), applied to whichever discriminator the comparator is using.

The `InteractionComponent.cpp` change is then just: delete the local `ShouldKeepCurrentFocus`, call `FInteractionTargetResolver::ShouldKeepCurrentCandidate(...)` instead. No new targeting rule lives there.

## Component UPROPERTY cleanup (same change)

Per ALIS public-repo migration policy ("remove obsolete reflected fields in the same change"):

### Drop from `UInteractionComponent` ([InteractionComponent.h](../../Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Public/InteractionComponent.h))

- `AimWeight`
- `DistanceWeight`
- `PriorityWeight`

### Shrink `FInteractionTargetingWeights` and `BuildWeights`

Keep only:
- `InteractionRadius`
- `ShortCircuitRadius`
- `MinAimDot`

`InteractionComponent.cpp` is touched only to remove score-based hysteresis usage and delegate the keep-current decision to `FInteractionTargetResolver::ShouldKeepCurrentCandidate(...)` (see "Hysteresis migration"). No new targeting rule is added there.

## Tunables that stay unchanged

| Tunable | Default | Why kept |
|---|---|---|
| `InteractionRadius` | 300 cm | Floor pickups need ~2.5 m reach. Do NOT shrink to 100. |
| `MinAimDot` | 0.85 | Cone half-angle ~31.8 deg, the gather gate. |
| `ShortCircuitRadius` | 60 cm | LOS skip below this. |
| `FocusSwitchHysteresis` | 0.10 | Anti-flicker on switch. Stays as a component UPROPERTY. The hysteresis *function* moves to the resolver (see "Hysteresis migration"); the *threshold* is still authored on the component. |

`InteractionComponent.cpp` IS touched, but only to delete the score-based `ShouldKeepCurrentFocus` and call the resolver's comparator-aware helper instead. No new targeting rule is added there.

## Cascade work (must land in the same change)

- Update [Plugins/Gameplay/ProjectInteraction/README.md](../../Plugins/Gameplay/ProjectInteraction/README.md) "Targeting" section: describe the comparator chain instead of the score blend; remove mentions of `AimWeight` / `DistanceWeight` / `PriorityWeight`.
- Audit constructor / `BeginPlay` / verbose log strings in `UInteractionComponent` for any reference to the dropped weights and clean up.
- Audit existing automation tests for any that assume `Priority` can outweigh aim. If found, replace with `PriorityBreaksOnlyAfterCenterednessAndDistanceTie` (see below). The currently green tests `OverlapFindsOffAxisInteractableWithoutDirectLineHit`, `VisibleCandidateBeatsOccludedCenteredCandidate`, `BackCandidateRejectedByAimGate`, `PeripheralCandidateRejectedByAimGate`, `ShortCircuitBypassesLosButNotAim`, `WorldStaticInteractableCandidateCanResolve`, `RayHitOnLargeMeshPassesAimGate`, `RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget` should all stay green - the cone gate, LOS gate, and ray-hit `TargetPoint` are unchanged.

## Required regression tests

All in [InteractionTargetResolverIntegrationTests.cpp](../../Plugins/Test/ProjectIntegrationTests/Source/ProjectIntegrationTests/Private/Integration/InteractionTargetResolverIntegrationTests.cpp), tagged `[Fast][Integration][Interaction]`:

- `ProjectIntegrationTests.Interaction.Targeting.RayPiercedFrontTargetBeatsRayPiercedBackTarget` - the windows case. Spawn two interactables co-linear with the view ray, one in front of the other. Assert the front one wins regardless of priority on the back one.
- `ProjectIntegrationTests.Interaction.Targeting.RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget` - already landed in `b8e123f17`. Comparator change must keep it green.
- `ProjectIntegrationTests.Interaction.Targeting.ConeFallbackMostCenteredWinsWhenRayMisses` - two non-pierced candidates inside the cone. Higher `AimDot` wins; closer breaks tie when `AimDot` is equal.
- `ProjectIntegrationTests.Interaction.Targeting.PriorityBreaksOnlyAfterCenterednessAndDistanceTie` - locks the demoted role of `Priority`. Spawn a low-priority candidate that is more centered (or more pierced, or closer) than a high-priority candidate; assert the low-priority one wins. Then spawn two candidates that are identical on bucket + AimDot + Distance and differ only in `Priority`; assert the higher-priority one wins. This is the test that prevents future regressions where someone reintroduces priority-as-force.

Existing aim-rejected, LOS-rejected, short-circuit, off-axis-found, WorldStatic, large-mesh-aim-gate, and small-target-vs-off-axis-high-priority tests must keep passing - the cone gate, LOS gate, and ray-hit `TargetPoint` are unchanged.

Reuse existing test helpers in the same file: `SpawnInteractable` (already takes a `Priority` argument), `SpawnPawn`, `AttachInteractionComponent`, `TestOnly_ResolveBestInteractionTarget`.

## Out-of-scope (do later only if a real gameplay case asks for it)

- `DistanceWeight = 0` - irrelevant once the score blend is deleted; covered automatically by removing the field.
- `InteractionRadius` retuning - leave at 300; revisit only with telemetry.
- Any Niagara / feedback / focus-presentation changes.
- Any `InteractionComponent.cpp` edit.

## Verification at implementation time

Per [docs/agents/canonical.md](../../docs/agents/canonical.md) Dev Loop Contract, exact-name single-test runs on a warm editor:

```powershell
scripts\ue\test\unit\iterate.ps1 -TestFilter ProjectIntegrationTests.Interaction.Targeting.RayPiercedFrontTargetBeatsRayPiercedBackTarget
scripts\ue\test\unit\iterate.ps1 -TestFilter ProjectIntegrationTests.Interaction.Targeting.RayPiercedSmallTargetBeatsOffAxisHighPriorityTarget
scripts\ue\test\unit\iterate.ps1 -TestFilter ProjectIntegrationTests.Interaction.Targeting.ConeFallbackMostCenteredWinsWhenRayMisses
scripts\ue\test\unit\iterate.ps1 -TestFilter ProjectIntegrationTests.Interaction.Targeting.PriorityBreaksOnlyAfterCenterednessAndDistanceTie
```

Plus manual PIE sanity:
- Aim at the foreground tall window with a smaller window in line behind it - foreground wins.
- Aim at a small bottle next to a large door - bottle wins.
- Aim at a pickup on the floor at ~2 m - pickup is found.

## Cross-refs

- [Plugins/Gameplay/ProjectInteraction/README.md](../../Plugins/Gameplay/ProjectInteraction/README.md) - update "Targeting" section to describe the comparator chain instead of the score blend, in the same change.
- Prior fix this builds on: commit `b8e123f17` (ray-hit `TargetPoint`, `WorldStatic` overlap, regression test).

## Blocking issues

None. Implementation landed in working tree and verified clean. User commit pending.
