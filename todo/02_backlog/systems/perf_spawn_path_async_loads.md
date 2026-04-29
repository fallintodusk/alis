# Perf: Convert Spawn-Path Synchronous Loads to Async

## Problem

The object spawn path uses `LoadSynchronous` / `RequestSyncLoad` / `TryLoad` for several heavy assets, which can stall the game thread for seconds per spawn ([UE docs](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FStreamableManager/RequestSyncLoad)). Audit done 2026-04-22 during the City17 outdoor-lag investigation; conversions deferred at the time because the perf symptoms were resolved by other fixes (Phase 3 preload via `bSerializeDependencies=True`, PSO disk cache, Mutable budget bump). Spawn-path sync loads are not currently user-visible but remain technical debt that can resurface on new maps, denser NPC populations, or heavier content.

## Expected

Each hot-priority sync load below replaced with `FStreamableManager::RequestAsyncLoad` plus a completion-delegate gate at the spawn/apply site. Same lifecycle pattern already used by `UObjectDefinitionCache` residency.

## Audit (priority order)

| # | File:Line | Load target | Why hot |
|---|---|---|---|
| 1 | `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp:873` | groom binding asset | Each groom-enabled NPC stalls a frame on spawn |
| 2 | `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp:823` | mesh asset | Primary actor spawn path; per-actor stall |
| 3 | `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp:1088`, `:1327` | material override / post-process material | Per material slot, per trigger volume PP material |
| 4 | `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/InteractableActor.cpp:372`, `:415`, `:435` | mesh / anim class / material override | `ApplyDefinition_Implementation`; call frequency unknown - profile first |
| 5 | `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp:3411` | ability set (`TryLoad`) | Per equip action; cheap per call but HOT under rapid equip |

## Cold call sites (kept for reference; NOT lag contributors, do not convert blindly)

- `ObjectSpawnUtility.cpp:347` (`GetLootProfile` fallback)
- `ObjectSpawnUtility.cpp:1751` (`SpawnFromDefinition` public overload fallback)
- `Plugins/Systems/ProjectLoading/Source/ProjectLoading/Private/Experience/InitialExperienceLoader.cpp:85` (warmup `RequestSyncLoad` at startup)
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/ProjectUIThemeManager.cpp:79` (theme change)
- `Plugins/UI/ProjectUI/Source/ProjectUI/Private/Subsystems/LoadingScreenSubsystem.cpp:86` (level-load widget bootstrap)
- `Plugins/Systems/ProjectLoading/Source/ProjectLoadingMoviePlayer/Private/Subsystems/ProjectLoadingMoviePlayerSubsystem.cpp:388` (movie player init)

## Reopen criteria

Promote from backlog to active only if any of these surface:

- A new map shows visible spawn-path stutter at outdoor entry.
- NPC density grows (multiple MetaHuman spawns at once) and groom binding sync load multiplies into a measurable stall.
- A Shipping log captures `LoadSynchronous` / `WaitForLoading` taking >50 ms on the game thread during gameplay.

## Engineering pattern

Per priority-1 site, sketch (do not paste verbatim - each site has its own context):

```cpp
// Replace:
UGroomBindingAsset* GroomBinding = GroomBindingPath.LoadSynchronous();
ApplyGroomBinding(SpawnedActor, GroomBinding);

// With:
TArray<FSoftObjectPath> Paths{ GroomBindingPath.ToSoftObjectPath() };
TWeakObjectPtr<AActor> WeakActor(SpawnedActor);
UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
    Paths,
    FStreamableDelegate::CreateLambda([WeakActor, GroomBindingPath]()
    {
        if (AActor* Actor = WeakActor.Get())
        {
            UGroomBindingAsset* Loaded = GroomBindingPath.Get();
            ApplyGroomBinding(Actor, Loaded);
        }
    })
);
```

Each conversion needs:
- TWeakObjectPtr capture (spawn target may be destroyed before the load completes).
- A test that toggles the conversion off (sabotage check, per `docs/agents/canonical.md` testing strategy).
- Verification that downstream code handling the asset tolerates "applied later" vs "applied during spawn" timing.

## Size

- Priority 1 (groom binding): ~2-4 hr including sabotage test.
- Priority 1-3 together (spawn-path 3 sites): half-day.
- Priority 4-5: profile call frequency first; defer if low-frequency.

## Forbidden

Do NOT convert all five sites in one batch. Each conversion changes timing semantics; bundle introduces hard-to-bisect regressions. One site per PR with its own sabotage test.

## Files

- `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/ObjectSpawnUtility.cpp` (priorities 1, 2, 3)
- `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Spawning/InteractableActor.cpp` (priority 4)
- `Plugins/Features/ProjectInventory/Source/ProjectInventory/Private/Components/ProjectInventoryComponent.cpp` (priority 5)

## References

- Origin: `todo/01_done/...` archived perf_outdoor_lag_shipping.md (sync-load audit section, 2026-04-22).
- UE docs: [`FStreamableManager::RequestSyncLoad`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FStreamableManager/RequestSyncLoad) (canonical "stall warning"), [`RequestAsyncLoad`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FStreamableManager/RequestAsyncLoad) (replacement).
- Project pattern: `UObjectDefinitionCache` residency flow (existing async load + completion gate; mirror this).
- Testing policy: `docs/agents/canonical.md` (each site needs a sabotage test).
