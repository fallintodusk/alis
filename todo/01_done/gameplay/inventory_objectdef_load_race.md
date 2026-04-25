# Inventory — ObjectDefinition Load Race (Silent Pickup Failure)

**Status:** TODO. Investigation complete, implementation pending. Separate session on a `bot/*` branch.
**Priority:** Major (gameplay-blocking, intermittent, silent)
**Date started:** 2026-04-21
**Related:** `Plugins/Features/ProjectInventory/docs/design_vision.md` | `docs/agents/architecture.md` | `Plugins/Systems/ProjectLoading/*`

---

## Problem

Pickup of `Crowbar` (and, latently, any `ObjectDefinition`) randomly fails. Nothing enters the inventory. No toast, no error, no player-visible feedback. Restarting the editor "fixes" it — because the post-gameplay async warmup gets more wall-clock time before the player reaches the pickup.

This is not a UI bug. It is a three-layer **loading contract** bug spanning ProjectLoading, ProjectCore, and ProjectInventory.

## Root cause (three layers)

### Layer A — Dead pipeline field

`FLoadRequest::WarmupAssetIds` is populated but never consumed.

- Populated: [Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/Loaders/InitialExperienceLoader.cpp](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/Loaders/InitialExperienceLoader.cpp) (`AddAssetIds(Descriptor->LoadAssets.WarmupAssets, OutRequest.WarmupAssetIds, ...)` ~line 111).
- Declared: [Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Types/ProjectLoadRequest.h](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Types/ProjectLoadRequest.h) (`WarmupAssetIds` ~line 93).
- Phase 6 executor: [Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/WarmupPhaseExecutor.cpp](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/WarmupPhaseExecutor.cpp) — does shader precompile + optional level streaming, never touches `WarmupAssetIds`.

Phase 6 completes while `ObjectDefinition` assets for that experience are still uncommitted to memory.

### Layer B — Post-gameplay async warmup

`ObjectDefinitionCache::Warmup` is kicked from [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/ProjectInventory.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/ProjectInventory.cpp) ~lines 90-101, asynchronously, **after** the feature is initialized — which happens around pawn spawn. There is no gate preventing the player from interacting with pickups before `Warmup` finishes.

Race window: first frames after pawn spawn, variable length depending on disk, shader state, and asset count.

### Layer C — Silent failure at the sharp end

[`UProjectInventoryComponent::Internal_AddItem`](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) (~lines 2839-2858):

```
const EInventoryItemDataResolveState ResolveState = ResolveItemDataView(ObjectId, ItemData);
if (ResolveState != EInventoryItemDataResolveState::Loaded)
{
    UE_LOG(..., Warning, TEXT("Item data unavailable..."));
    return 0;   // <- no RequestLoad, no BroadcastError, no retry
}
```

`HandlePickupSource` ([InventoryInteractionHandler.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp)) treats `Added == 0` as "nothing added" and skips `Consume`. Item stays on the ground. Player has no idea anything happened.

`ResolveItemDataView` ([ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) ~lines 829-874) already distinguishes `Missing` / `Loading` / `Loaded` / `InvalidProvider` / `InvalidData`. Only `Loaded` should short-circuit-succeed. `Missing` and `Loading` should defer, not fail.

## Goal

Pickups must never silently fail because an asset is still loading.

**Core safety invariant (applies to the entire initiative):**
**A pickup source may be consumed at most once, and only after an authoritative add succeeds.**

**Correctness boundary (must-have):**
- The **inventory boundary** — `Internal_AddItem` + `InventoryInteractionHandler` — must never produce a silent zero-add when the asset is simply not resident yet. Missing/Loading must defer and retry; hard failures must broadcast.

**Performance boundary (should-have, separate concern):**
- Warmup assets declared by the experience descriptor should actually be loaded before the common-case first-frame pickup, so the deferred path stays the exceptional case, not the default.

**Explicit split**
- Phase 6 is marked non-critical in the loading subsystem — it does **not** gate interactivity. That means **Slice 2 is the correctness fix**. Slice 1 is an optimization that keeps the deferred path rare; it is not a substitute for Slice 2.

## Principles

1. **Correctness lives at the inventory boundary, not in the loading pipeline.** No amount of pre-warmup can cover DLC, modded content, or future descriptors. Defer-and-retry is the invariant.
2. **Boundary direction is strict.** ProjectLoading does not depend on ProjectInventory or on `ObjectDefinitionCache`. Any expansion of an `ObjectCatalog` into concrete `ObjectDefinition` primary asset ids happens in ProjectCore loader code, before the phase executor runs. The executor only loads ids it is given.
3. **Reuse the cache primitives.** `ObjectDefinitionCache::RequestLoad` already deduplicates, shares the `FStreamableHandle`, and fires batched callbacks. No new pending-operation subsystem.
4. **UI coupling via delegate only.** ProjectInventory broadcasts `OnInventoryError`. UI binds it to `UProjectToastSubsystem`. ProjectInventory does not depend on ProjectUI. Cross-plugin boundaries SOT: `docs/agents/architecture.md`.
5. **No blocking gate.** Do not pause gameplay waiting for a warmup flag.
6. **Data-driven warmup set.** Experience descriptor's `LoadAssets.WarmupAssets` is the SOT for "should be hot". No Crowbar hardcodes, no catalog-path hardcodes in C++.
7. **Narrow public API.** Pending-pickup bookkeeping lives in the interaction handler. The inventory component returns a narrow internal result. Do not expose a public async pickup API on the component unless a second caller needs it.

## Implementation order

1. **Slice 2** — defer-and-retry at the inventory boundary (correctness).
2. **Slice 3** — loud failure on terminal states (UX contract; lands together with Slice 2).
3. **Tests** — unit + integration (incl. anti-spam and contention cases) must be green before Slice 1 is started.
4. **Slice 1** — warmup pipeline optimization, after correctness and tests are in.

This order ensures correctness lands first and is verified; the optimization slice then only changes the timing profile, not the correctness properties the tests pin down.

## Implementation slices

### Slice 2 — Defer-and-retry at the inventory boundary (correctness, must-land)

Listed first because it is the correctness fix. Ships independently of Slice 1.

**Files**
- [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) — `Internal_AddItem` returns an internal struct, not a bare `int32`. `TryAddItem` keeps its existing `int32` public signature (backward-compatible; a deferred result maps to `0` for legacy callers). No new public async method added.
- [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Interaction/InventoryInteractionHandler.cpp) — **owns** the pending-pickup map. Holds intent until load resolves. Drives `Consume` only on successful authoritative add. **Lifetime is anchored to the owning `UProjectInventoryComponent`** (handler is per-component, cleared on `UninitializeComponent` / `EndPlay`). **No player-scoped subsystem in this slice** — do not introduce `ULocalPlayerSubsystem` or `UGameInstanceSubsystem` surface for pending-pickup state.
- [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp) — no API change; reuse `RequestLoad(FOnObjectDefinitionLoaded)`.

**Internal result shape (private to the inventory module)**

```
struct FInventoryAddOutcome
{
    int32 AddedQuantity = 0;    // quantity actually accepted
    bool bDeferred = false;     // waiting on async load
    EInventoryAddFailReason Fail = EInventoryAddFailReason::None;
};
```

`TryAddItem` public wrapper returns `AddedQuantity` as today. Interaction handler consumes the full struct via an internal overload; external Blueprint callers see no new surface.

**Deferred retry contract — deterministic**

- **Attempts:** exactly one async load + one re-entry into the add path. Not a retry loop. `ObjectDefinitionCache::RequestLoad` dedup means N concurrent intents for the same id share one handle and one callback fan-out.
- **Callback payload:** the cache invokes the callback with the resolved `UObject*` or `nullptr`. `nullptr` is terminal hard failure (treated as `Failed`, never re-queued).
- **Pickup source lifetime:** intent holds `TWeakObjectPtr<UObject>` for pickup source and `TWeakObjectPtr<APawn>` for instigator. On callback, if either is null → drop intent silently. This is expected, not an error (player moved, actor destroyed, different pickup consumed first).
- **World teardown:** on the owning component's `UninitializeComponent` / `EndPlay`, flush and discard the pending map; do not broadcast errors during teardown.
- **Handle failure or cancellation:** `RequestLoad` returning nullptr-handle is surfaced as a synchronous `Failed` outcome at submit time (no intent queued). If `FStreamableHandle` later cancels, callback fires with `nullptr` — terminal `Failed`, toast shown.
- **Server authority:** deferred retries re-enter through the existing `Server_AddItem` RPC path. Client-side intent is bookkeeping only; the authoritative add happens on the server. Second player racing the same pickup: whichever server-side `Consume` wins first destroys the pickup source; the loser's deferred re-entry observes a null `TWeakObjectPtr<UObject>` and drops silently.
- **Anti-spam:** pending map key = `(PickupSource, ObjectId)`. Duplicate submissions before callback merge into the existing intent — the first queued intent is kept as-is. **Quantity on merge:** keep the first queued quantity. If a later submission presents a different quantity, `checkSlow` / `ensure` in development builds (flags a design drift), keep the first value in shipping. This is safer than "take latest" if a pickup source ever stops being strictly single-quantity.
- **Terminal states (always clear intent):** `Added` (success), `Failed` (callback nullptr, invalid provider/data after load, quantity rejected), or weak-ptr-null on callback entry. Every intent exits the map in one of these states.

**Acceptance**
- Unit: mock `ObjectDefinitionCache` returning `Missing`, `Loading`, `Loaded`, `InvalidProvider`, `InvalidData`; verify `RequestLoad` invoked exactly once per unique id, correct outcome per state, no leaks in the pending map.
- Integration (cold-boot): pickup of a test `ObjectDefinition` deliberately **omitted from `WarmupAssets`** succeeds after async load resolves, on the first gameplay frames.
- Integration (terminal failure): simulate `RequestLoad` callback with `nullptr` → toast shown, intent cleared, pickup actor **not** consumed.
- No silent `return 0` paths remain in `Internal_AddItem`.

### Slice 1 — Honor declared warmup (optimization, independent)

Optional perf optimization so the deferred path is rare in normal play. **Not** a correctness substitute for Slice 2.

**Boundary fix vs. previous draft**

Previous draft had Phase 6 chain `ObjectDefinitionCache::Warmup(Catalog->Objects)`. That is wrong direction — ProjectLoading would depend on ProjectInventory. Corrected shape:

1. `InitialExperienceLoader` (ProjectCore) — when an `ObjectCatalog` is declared in `LoadAssets.WarmupAssets`, load that catalog synchronously for the request build, then expand its `Objects` into concrete `ObjectDefinition` primary asset ids, and append those ids to `OutRequest.WarmupAssetIds`.
2. `WarmupPhaseExecutor` (ProjectLoading) — remains generic. It batch-loads whatever ids it is handed via `UAssetManager::LoadPrimaryAssets`. It does not know about catalogs, object definitions, or inventory.
3. `ObjectDefinitionCache::Warmup` (ProjectInventory) — still runs from the inventory feature init, but on a second boot its calls become no-ops because the ids are already resident (the cache already short-circuits `IsLoaded`).

**Files**
- [Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/Loaders/InitialExperienceLoader.cpp](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Private/Loaders/InitialExperienceLoader.cpp) — add catalog expansion step before returning the request.
- [Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Data/ObjectCatalog.h](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Data/ObjectCatalog.h) — already ProjectCore, no new dependency created.
- [Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/WarmupPhaseExecutor.cpp](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/WarmupPhaseExecutor.cpp) — batch-load `Request.WarmupAssetIds` via `LoadPrimaryAssets`. Stays a generic id consumer.
- Experience descriptor data — add `ObjectCatalog` primary asset id to `LoadAssets.WarmupAssets`.

**Acceptance**
- Phase 6 logs include a count of warmup primary assets loaded.
- On a normal boot for an experience that declares the catalog in `LoadAssets.WarmupAssets`, the common-case first pickup resolves without deferral — i.e. `ObjectDefinitionCache::IsLoaded(ObjectDefinition:Crowbar)` is `true` before the common-case first pickup attempt on a normal boot. This is an optimization target, not a gating contract (Phase 6 stays non-critical; Slice 2 remains the correctness backstop).
- Phase 6 stays under its existing 30s budget. Phase 6 remains non-critical; failure to load a warmup id is logged but does not abort the phase (Slice 2 catches any gap).
- `WarmupPhaseExecutor.cpp` has zero includes from `Plugins/Features/ProjectInventory/**` and no references to `ObjectDefinitionCache`.

### Slice 3 — Loud failure (UX contract, lands with Slice 2)

**Files**
- [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp) — `BroadcastError` on every terminal failure branch in the add path.
- UI binding (separate, non-ProjectInventory module) — subscribe to `OnInventoryError` and forward to [Plugins/UI/ProjectUI/Source/ProjectUI/Public/Subsystems/ProjectToastSubsystem.h](../../Plugins/UI/ProjectUI/Source/ProjectUI/Public/Subsystems/ProjectToastSubsystem.h). No ProjectInventory→ProjectUI dependency.

**Behavior**
- Hard failure (invalid provider, invalid data, `RequestLoad` callback nullptr, final quantity rejected) → `OnInventoryError` → toast, every time.
- Deferred → **no default toast**. Log + trace only. Fast loads would make a toast noisy and annoying.
- Optional follow-up (not required for correctness): a delayed info toast if the deferred state persists past a threshold (e.g. 500 ms). Gate behind a cvar or project setting, default off. Out of scope for the first landing.

**Acceptance**
- `InvalidProvider`, `InvalidData`, final-miss, and quantity rejection all produce a visible toast.
- Happy-path deferred (load resolves quickly) produces zero toasts and zero user-visible text.
- No silent `return 0` paths remain in `Internal_AddItem`.

## Verification

### Unit (`Plugins/Test/ProjectInventoryTests`)
- Mock `ObjectDefinitionCache` over all `ResolveItemDataView` states; verify outcomes per state.
- Single-intent lifecycle: submit → callback → cleared.
- Merge-on-duplicate: 10 submits for the same `(PickupSource, ObjectId)` while loading → one intent, one `RequestLoad`, one grant, one consume.
- Terminal `nullptr` callback → `Failed` + `OnInventoryError` fires; intent cleared.
- Weak-ptr-null on callback (pickup source destroyed pre-callback) → silent drop; no broadcast.

### Integration (`Plugins/Test/ProjectIntegrationTests`)
Mirror `InventoryLootPlacesIntegrationTest.cpp` style.

- **Cold-boot deferred success:** ObjectDefinition omitted from `WarmupAssets`, pickup attempted on first gameplay frame, eventually lands in inventory, pickup actor consumed exactly once.
- **Spam contention (single player):** trigger pickup 10× while load is in flight → exactly one grant on the server, exactly one `Consume` call on the pickup source, exactly one `OnItemAdded` event.
- **Two-player race:** two inventory components both submit intent for the same pickup source before callback → exactly one `Consume`, exactly one grant; the losing client sees weak-ptr-null on callback and drops silently, no error toast.
- **Pickup destroyed pre-callback:** destroy the pickup actor while load is in flight → callback resolves, weak ptr is null, intent cleared silently, no inventory change.
- **World teardown during callback:** end PIE while a `RequestLoad` is outstanding → no crash, no error broadcast during teardown.
- **Terminal load failure:** force `RequestLoad` callback to `nullptr` → toast shown, pickup actor not consumed, intent cleared.

### Smoke
- `scripts/ue/test/smoke/boot_test.bat` must still pass.

### Manual
- Restart editor, run immediately to a Crowbar pickup in the first gameplay frames. With both slices landed: pickup is instant (Slice 1 warmed it) or silently-deferred-and-succeeds (Slice 2). Never silent failure.

## Non-goals

- Do **not** force `ObjectCatalog` into `CriticalAssetIds` (Phase 3) as a shortcut. That hides Layer A and Layer C.
- Do **not** block gameplay waiting for a warmup flag.
- Do **not** let `WarmupPhaseExecutor` know about `ObjectDefinitionCache`, `ObjectCatalog`, or anything in ProjectInventory. Phase 6 only loads ids handed to it.
- Do **not** couple ProjectInventory to ProjectUI. Use `OnInventoryError`.
- Do **not** add a public async pickup API on the component in this iteration. Pending-pickup bookkeeping lives in the interaction handler.
- Do **not** emit a default info toast on deferred pickups. Log and trace only.
- Do **not** build a retry loop with attempt counters. One async load + one re-entry; `nullptr` callback is terminal.
- Do **not** change `ObjectDefinitionCache` public API. It already deduplicates and batches.
- Do **not** touch the inventory UI decouple work on the current branch ([inventory_ui_nearby_decouple.md](inventory_ui_nearby_decouple.md)) — complete and orthogonal.

## Refs

- [docs/agents/architecture.md](../../docs/agents/architecture.md) — cross-plugin boundaries, DIP.
- `Plugins/Features/ProjectInventory/docs/design_vision.md` — inventory behavior SOT.
- [Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/ProjectLoadingSubsystem.cpp](../../Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/ProjectLoadingSubsystem.cpp) — phase orchestration; Phase 6 is non-critical and does not gate interactivity.
- [Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp](../../Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Services/ObjectDefinitionCache.cpp) — `RequestLoad` / `Warmup` internals.

## Out of scope for this session

This document **is** the deliverable for this session. No code changes in this session. Implementation happens in a dedicated session on a `bot/*` branch; Slice 2 + Slice 3 land together (correctness), Slice 1 lands independently (optimization).
