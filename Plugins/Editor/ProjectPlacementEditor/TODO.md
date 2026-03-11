# ProjectPlacementEditor - TODO

Outstanding tasks for the Actor Auto-Update System.

---

## 1. Implement Capability Updates (High Priority)

**Status:** TODO in `AInteractableActor::ApplyDefinition_Implementation`

**Current state:**
- [OK] Mesh updates work (add/remove/modify static meshes)
- [X] Capability component updates do not work (e.g., adding Lockable to a door)

**Problem:**
Capability resolution logic exists only in `UProjectObjectActorFactory::CreateActor()` and is not reusable.

**Solution:**
1. Extract capability resolution into shared helper function:
   ```cpp
   // New helper in ProjectObjectActorFactory or separate utility
   static void ApplyCapabilitiesToActor(
       AInteractableActor* Actor,
       const TArray<FCapabilityEntry>& Capabilities);
   ```

2. Use in both:
   - `UProjectObjectActorFactory::CreateActor()` (initial spawn)
   - `AInteractableActor::ApplyDefinition_Implementation()` (updates)

3. Implementation requirements:
   - Match existing capability components by `CapabilityTag`
   - Add missing capabilities (spawn new components)
   - Remove obsolete capabilities (destroy old components)
   - Update properties on existing capabilities

**Complexity:**
Medium - Requires refactoring Factory code and handling component lifecycle.

**Workaround:**
- Mode 0: Automatic Replace fallback works [OK]
- Mode 1/2: Manual fix required (delete + re-place) [!]

**Files to modify:**
- [ProjectObjectActorFactory.h](Source/ProjectPlacementEditor/Public/ProjectObjectActorFactory.h)
- [ProjectObjectActorFactory.cpp](Source/ProjectPlacementEditor/Private/ProjectObjectActorFactory.cpp)
- [InteractableActor.cpp](../Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp)

---

## 2. Make Mode 1 Fully Generic (Medium Priority)

**Status:** Mode 1 uses ObjectDefinition-specific hash checking

**Current state:**
- [OK] ObjectDefinition actors: Fast-skip optimization (subsystem checks content hash)
- [!] Other definition actors (if added): Always call ApplyDefinition (actors do internal hash check, minimal overhead)

**Problem:**
```cpp
// DefinitionActorSyncSubsystem.cpp:595-602
UObjectDefinition* ObjDef = Cast<UObjectDefinition>(Def);
if (ObjDef && !ObjDef->DefinitionContentHash.IsEmpty())
{
    if (WorldActor->AppliedContentHash == ObjDef->DefinitionContentHash)
    {
        continue; // Skip update - only works for ObjectDefinition
    }
}
```

**Solution (pick one):**

### Option A: Remove subsystem hash check (simplest)
**Pros:** Simpler code, actors already do hash checking internally
**Cons:** Negligible perf overhead (~0.1ms per other definition actor on load)

```cpp
// Always call ApplyDefinition - actors handle hash check
if (ApplyDefinitionToActor(WorldActor, Def))
{
    UE_LOG(LogDefinitionActorSync, Log, TEXT("[Mode1] Applied: %s"), *WorldActor->GetActorLabel());
}
```

### Option B: Create shared definition base class (cleaner)
**Pros:** Type-safe, subsystem truly generic
**Cons:** More refactoring (affects all definition types)

```cpp
// New base class
UCLASS(Abstract)
class PROJECTWORLD_API UProjectDefinitionBase : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, Category = "Definition")
    FString DefinitionStructureHash;

    UPROPERTY(VisibleAnywhere, Category = "Definition")
    FString DefinitionContentHash;
};

// Then derive:
class UObjectDefinition : public UProjectDefinitionBase { ... };
// Future definition types derive from UProjectDefinitionBase.

// Subsystem can cast to base:
UProjectDefinitionBase* BaseDef = Cast<UProjectDefinitionBase>(Def);
if (BaseDef && WorldActor->AppliedContentHash == BaseDef->DefinitionContentHash)
{
    continue; // Generic fast-skip!
}
```

**Recommendation:** Option A (simpler, minimal impact)

**Files to modify (Option A):**
- [DefinitionActorSyncSubsystem.cpp](Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Remove ObjectDefinition cast

**Files to modify (Option B):**
- [ProjectDefinitionBase.h](../World/ProjectWorld/Source/ProjectWorld/Public/Data/ProjectDefinitionBase.h) - New file
- [ObjectDefinition.h](../Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) - Change base class
- [DefinitionActorSyncSubsystem.cpp](Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Use base class

---

## 3. Implement Specialized Definition Replace (Low Priority)

**Status:** N/A unless new definition types are added

**Current state:**
- [OK] ObjectDefinition actors: Automatic Replace via `UProjectObjectActorFactory`
- [X] Other definition actors: Manual fix required (delete + re-place)

**Problem:**
Mode 0 Replace only works for ObjectDefinition types:
```cpp
// DefinitionActorSyncSubsystem.cpp:473-491
if (DefId.PrimaryAssetType == FName(TEXT("ObjectDefinition")))
{
    AActor* NewActor = ReplaceActorFromDefinition(Actor, Def); // Works
}
else
{
    // Other definition types - no factory exists
    ManualFixNeeded.Add(Actor);
}
```

**Solution:**
1. Create a dedicated factory per definition type (mirror of `UProjectObjectActorFactory`)
2. Implement Replace support:
   ```cpp
   AActor* UProjectPickupActorFactory::ReplaceActor(
       AActor* OldActor,
       UPrimaryDataAsset* Definition);
   ```

3. Register factory in subsystem:
   ```cpp
   // DefinitionActorSyncSubsystem.h
   TWeakObjectPtr<UProjectPickupActorFactory> CachedPickupFactory;
   ```

4. Update Replace logic to handle other definition types:
   ```cpp
   if (DefId.PrimaryAssetType == FName(TEXT("SomeDefinitionType")))
   {
       AActor* NewActor = ReplaceActorFromDefinition(Actor, Def);
   }
   ```

**Complexity:**
Low - Copy pattern from ObjectFactory for specialized actor types

**Workaround:**
Manual fix is acceptable for rare structural changes to specialized actors.

**Files to create:**
- [ProjectPickupActorFactory.h](Source/ProjectPlacementEditor/Public/ProjectPickupActorFactory.h)
- [ProjectPickupActorFactory.cpp](Source/ProjectPlacementEditor/Private/ProjectPickupActorFactory.cpp)

**Files to modify:**
- [DefinitionActorSyncSubsystem.h](Source/ProjectPlacementEditor/Public/DefinitionActorSyncSubsystem.h) - Add cached factory
- [DefinitionActorSyncSubsystem.cpp](Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Add Replace branch

---

## 4. Add Async Definition Loading (Performance Optimization)

**Status:** Mode 1 uses synchronous loading

**Current state:**
```cpp
// DefinitionActorSyncSubsystem.cpp:642
UObject* LoadedAsset = AssetPath.TryLoad(); // Synchronous - blocks frame
```

**Problem:**
Loading definitions synchronously can cause frame hitches when many actors load (World Partition streaming).

**Solution:**
Use `FStreamableManager::RequestAsyncLoad()`:
```cpp
FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();

TArray<FSoftObjectPath> AssetsToLoad;
AssetsToLoad.Add(AssetPath);

StreamableManager.RequestAsyncLoad(
    AssetsToLoad,
    FStreamableDelegate::CreateUObject(this, &UDefinitionActorSyncSubsystem::OnDefinitionLoaded, WorldActor)
);
```

**Complexity:**
Medium - Requires callback handling, actor weak pointers, deferred update queue

**Priority:**
Low - Only matters for large World Partition worlds with many actors loading simultaneously

**Files to modify:**
- [DefinitionActorSyncSubsystem.h](Source/ProjectPlacementEditor/Public/DefinitionActorSyncSubsystem.h) - Add async callback
- [DefinitionActorSyncSubsystem.cpp](Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Replace TryLoad with RequestAsyncLoad

---

## 5. Add Batch Notification for Mode 1 (UX Enhancement)

**Status:** Shows per-actor notifications (can be spammy)

**Current behavior:**
Each actor with structural mismatch shows separate warning notification.

**Suggested improvement:**
Batch multiple updates into single notification:
```
"5 actors updated silently from World Partition load"
"3 actors need manual fix (structural changes)"
```

**Complexity:**
Low - Accumulate updates in array, show single notification at end of frame

**Priority:**
Low - Current per-actor notifications work, just verbose

**Files to modify:**
- [DefinitionActorSyncSubsystem.cpp](Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Batch Mode 1 notifications

---

## Priority Summary

1. **High:** Implement capability updates (#1) - Most impactful for designers
2. **Medium:** Make Mode 1 fully generic (#2) - Architectural improvement (recommend Option A)
3. **Low:** Specialized Replace (#3) - Nice-to-have, manual fix acceptable
4. **Low:** Async loading (#4) - Only needed for very large worlds
5. **Low:** Batch notifications (#5) - UX polish

---

## Testing Checklist (After Implementation)

### Capability Updates (#1)
- [ ] Add capability to existing actor (e.g., door -> lockable door)
- [ ] Remove capability from actor
- [ ] Modify capability properties (e.g., change lock type)
- [ ] Verify Mode 0 Apply works (no Replace fallback)
- [ ] Verify Mode 1 silent update works
- [ ] Verify Mode 2 batch update works

### Mode 1 Generic (#2)
- [ ] Other definition actors get fast-skip optimization (if Option B)
- [ ] No ObjectDefinition-specific code in subsystem (if Option B)
- [ ] Performance test: Load 100 non-ObjectDefinition actors (< 10ms overhead if Option A)

### Specialized Replace (#3)
- [ ] Structural change to non-ObjectDefinition triggers Replace (not manual fix)
- [ ] Replace preserves transform/label/folder
- [ ] Replace works in Mode 0 (countdown notification)

### Async Loading (#4)
- [ ] No frame hitches when loading 50+ actors simultaneously
- [ ] Definitions load in background
- [ ] Actors update when definition finishes loading

### Batch Notifications (#5)
- [ ] Single notification for multiple Mode 1 updates
- [ ] Notification shows count (e.g., "5 actors updated")
- [ ] Clickable list in Message Log

---

## Notes

- All TODOs are non-blocking - current system works with documented workarounds
- Capability updates (#1) has highest ROI for designer workflow
- Mode 1 generic (#2) is architectural debt - recommend addressing with Option A
- Other items are polish/performance - defer until needed
