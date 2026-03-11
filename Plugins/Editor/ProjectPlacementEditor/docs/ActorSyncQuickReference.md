# Actor Sync Quick Reference

Quick lookup for common actor sync tasks and troubleshooting.

---

## Quick Start

### Enable Actor Sync for Your Actor Class

1. **Implement IDefinitionApplicable interface:**
```cpp
// YourActor.h
#include "IDefinitionApplicable.h"

UCLASS()
class AYourActor : public AProjectWorldActor, public IDefinitionApplicable
{
    GENERATED_BODY()

public:
    virtual bool ApplyDefinition_Implementation(UPrimaryDataAsset* Definition) override;
};
```

2. **Implement ApplyDefinition method:**
```cpp
// YourActor.cpp
bool AYourActor::ApplyDefinition_Implementation(UPrimaryDataAsset* Definition)
{
    // Cast to your definition type
    UYourDefinition* YourDef = Cast<UYourDefinition>(Definition);
    if (!YourDef)
    {
        return false; // Wrong definition type
    }

    // Structure hash check (safety)
    if (!AppliedStructureHash.IsEmpty() && AppliedStructureHash != YourDef->DefinitionStructureHash)
    {
        return false; // Structure mismatch - needs Replace
    }

    // Content hash check (idempotency)
    if (!AppliedContentHash.IsEmpty() && AppliedContentHash == YourDef->DefinitionContentHash)
    {
        return true; // Already up to date - no work needed
    }

    // Mark for undo
    Modify();

    // Update properties
    YourProperty1 = YourDef->Property1;
    YourProperty2 = YourDef->Property2;

    // Update meshes (if applicable)
    // Update capabilities (if applicable - currently TODO for InteractableActor)

    // Store hashes
    AppliedStructureHash = YourDef->DefinitionStructureHash;
    AppliedContentHash = YourDef->DefinitionContentHash;

    return true;
}
```

3. **Set ObjectDefinitionId when placing actor:**
```cpp
// In factory or placement code
Actor->ObjectDefinitionId = Definition->GetPrimaryAssetId();
```

---

## Common Tasks

### Force Update All Actors from Definition

**Editor:**
1. Modify definition asset
2. Save definition (Ctrl+S)
3. Wait 5 seconds (or click "Apply Now")
4. All actors update automatically

**Commandlet:**
```bash
UnrealEditor-Cmd.exe Alis.uproject -run=WorldPartitionResaveActorsBuilder -AllowCommandletRendering
```

---

### Manually Update Single Actor

**Option 1: Trigger Mode 0 (definition save)**
1. Open definition asset
2. Make any change (add space, remove space)
3. Save (Ctrl+S)
4. Click "Apply Now"

**Option 2: Delete and re-place**
1. Delete actor
2. Place new instance from Content Browser

---

### Check if Actor Will Update

**Prerequisites:**
- Actor has valid `ObjectDefinitionId`
- Actor implements `IDefinitionApplicable`
- Definition asset exists and is loaded

**Log check:**
```cpp
// Enable verbose logging
LogDefinitionActorSync
LogProjectWorldActor
```

**Manual check:**
```cpp
// In actor's ApplyDefinition implementation
UE_LOG(LogYourActor, Log, TEXT("ApplyDefinition called for: %s"), *GetActorLabel());
UE_LOG(LogYourActor, Log, TEXT("  Structure Hash: %s -> %s"), *AppliedStructureHash, *YourDef->DefinitionStructureHash);
UE_LOG(LogYourActor, Log, TEXT("  Content Hash: %s -> %s"), *AppliedContentHash, *YourDef->DefinitionContentHash);
```

---

## Troubleshooting Checklist

### Actor Not Updating (Mode 0 - Manual)

- [ ] Definition saved in editor? (Ctrl+S triggers update)
- [ ] Countdown appeared? (check notification in bottom-right)
- [ ] Actor visible in level? (not in unloaded World Partition cell)
- [ ] Actor has `ObjectDefinitionId` set? (check Details panel)
- [ ] Actor implements `IDefinitionApplicable`? (check class declaration)
- [ ] `ApplyDefinition` returning true? (check log: `[ActorSync] Applied` vs `[ActorSync] Replacing`)

---

### Actor Not Updating (Mode 1 - World Partition Load)

- [ ] World Partition enabled? (Project Settings -> World Partition)
- [ ] Actor in external package? (not in persistent level)
- [ ] Level hooks subscribed? (log: `Subscribed to ULevel::OnLoadedActorAddedToLevelPostEvent`)
- [ ] Content hash different? (log: `[Mode1] Actor X needs update: old -> new`)
- [ ] Apply succeeded? (log: `[Mode1] Applied: X` vs warning notification)

---

### Actor Not Updating (Mode 2 - Commandlet)

- [ ] Commandlet run? (`WorldPartitionResaveActorsBuilder`)
- [ ] PreSave guards passed? (ObjectDefinitionId valid, not procedural save, implements interface)
- [ ] Definition loaded? (check: `AssetManager.GetPrimaryAssetPath(ObjectDefinitionId)`)
- [ ] Apply succeeded? (log: `[Mode2] Applied` vs `[Mode2] Failed`)

---

### Structure Mismatch Warnings

**Symptom:**
```
LogProjectWorldActor: Warning: [Mode2] Failed to apply definition to: 'Actor_X' (structure mismatch or error)
```

**Cause:**
Definition structure changed (added/removed UPROPERTY, changed types).

**Fix:**
- **Mode 0:** Auto Replace (respawn actor) - no action needed
- **Mode 1/2:** Manual fix required:
  1. Open Message Log (Window -> Developer Tools -> Message Log)
  2. Click actor link to select
  3. Delete actor
  4. Place new instance from Content Browser

**Prevention:**
- Keep definition structures stable
- Use optional properties (add new fields with defaults)
- Use Blueprint properties instead of C++ UPROPERTY

---

### Content Hash Not Updating

**Symptom:**
Actor properties don't match definition, but no update triggered.

**Cause:**
`DefinitionContentHash` not computed or not changed.

**Fix:**
1. Check definition generator computed hash:
   ```cpp
   UE_LOG(LogTemp, Log, TEXT("Definition Content Hash: %s"), *YourDef->DefinitionContentHash);
   ```
2. Verify definition saved after hash update
3. Manually trigger regeneration (if needed)

---

### Editor Crashes on Startup

**Symptom:**
Crash in `UEditorEngine::GetEditorWorldContext` during subsystem init.

**Cause:**
Trying to access world context before it's ready.

**Fix:**
Already fixed in current implementation - we use `FWorldDelegates::OnPostWorldCreation` to defer level hook setup.

If crash persists:
1. Check `UDefinitionActorSyncSubsystem::Initialize()` doesn't access `GEditor->GetEditorWorldContext().World()` directly
2. Verify `OnWorldCreated()` filters for `EWorldType::Editor`

---

## Performance Tips

### Optimize Mode 0 (Manual Updates)

**Problem:** Countdown blocks designer workflow.

**Solutions:**
- Reduce countdown time (change `CountdownSeconds` constant)
- Add "Skip Countdown" user setting
- Batch multiple definition saves into single update

---

### Optimize Mode 1 (World Partition Load)

**Problem:** Frame hitches when loading actors.

**Solutions:**
- Use async definition loading (`StreamableManager.RequestAsyncLoad()`)
- Defer updates to next frame (accumulate batch)
- Early return on content hash match (already implemented)

---

### Optimize Mode 2 (Commandlet)

**Problem:** Commandlet takes too long.

**Solutions:**
- Run commandlet with `-NoShaderCompile` flag
- Filter actors by definition type (skip unrelated actors)
- Parallelize with multiple commandlet instances (World Partition cells)

---

## Log Analysis

### Enable Relevant Log Categories

```ini
# DefaultEngine.ini or console command
[Core.Log]
LogDefinitionActorSync=Verbose
LogProjectWorldActor=Verbose
```

Console commands:
```
Log LogDefinitionActorSync Verbose
Log LogProjectWorldActor Verbose
```

---

### Key Log Messages

**Subsystem initialization:**
```
LogDefinitionActorSync: Subscribed to FDefinitionEvents::OnDefinitionRegenerated
LogDefinitionActorSync: Subscribed to FWorldDelegates::OnPostWorldCreation (Mode 1)
LogDefinitionActorSync: Subscribed to ULevel::OnLoadedActorAddedToLevelPostEvent (Mode 1) for world: Kazan_MainCity
```

**Mode 0 countdown:**
```
LogDefinitionActorSync: [ActorSync] Found 12 actors to update - showing countdown
LogDefinitionActorSync: [ActorSync] Executing update for 12 actors
LogDefinitionActorSync: [ActorSync] Applied: Door_Instance_01
LogDefinitionActorSync: [ActorSync] Replacing: Door_Instance_02
LogDefinitionActorSync: [ActorSync] === Update complete ===
LogDefinitionActorSync:   Reapplied: 10 actors
LogDefinitionActorSync:   Replaced: 2 actors
LogDefinitionActorSync:   Time: 45.23ms
```

**Mode 1 passive:**
```
LogDefinitionActorSync: [Mode1] Actor Door_Instance_03 needs update: 1a2b3c4d -> 5e6f7g8h
LogDefinitionActorSync: [Mode1] Applied: Door_Instance_03
```

**Mode 2 batch:**
```
LogProjectWorldActor: [Mode2] Applied definition to: Door_Instance_04
LogProjectWorldActor: Warning: [Mode2] Failed to apply definition to: Chest_Instance_01 (structure mismatch or error)
```

---

## API Reference

### IDefinitionApplicable Interface

```cpp
UINTERFACE(BlueprintType)
class PROJECTWORLD_API UDefinitionApplicable : public UInterface
{
    GENERATED_BODY()
};

class PROJECTWORLD_API IDefinitionApplicable
{
    GENERATED_BODY()

public:
    /**
     * Apply definition to actor (update properties in-place).
     *
     * @param Definition The definition to apply (cast to your definition type)
     * @return true if applied successfully, false if structure mismatch or error
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Definition")
    bool ApplyDefinition(UPrimaryDataAsset* Definition);
};
```

---

### AProjectWorldActor Properties

```cpp
// Stored on actor
UPROPERTY(VisibleAnywhere, Category = "Definition")
FPrimaryAssetId ObjectDefinitionId; // Link to definition

UPROPERTY(VisibleAnywhere, Category = "Definition")
FString AppliedStructureHash; // Last applied structure hash (safety)

UPROPERTY(VisibleAnywhere, Category = "Definition")
FString AppliedContentHash; // Last applied content hash (idempotency)
```

---

### UDefinitionActorSyncSubsystem Methods

```cpp
// Public (exposed to subsystem users)
void OnDefinitionRegenerated(const FString& TypeName, UObject* RegeneratedAsset);

// Internal (subsystem implementation)
void OnWorldCreated(UWorld* World);
void OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors);
UPrimaryDataAsset* LoadDefinition(const FPrimaryAssetId& DefinitionId);
bool ApplyDefinitionToActor(AProjectWorldActor* Actor, UPrimaryDataAsset* Def);
AActor* ReplaceActorFromDefinition(AActor* OldActor, UPrimaryDataAsset* Def);
```

---

## Testing Checklist

### Manual Testing (Mode 0)

1. [ ] Place actor from definition
2. [ ] Modify definition (change property value)
3. [ ] Save definition (Ctrl+S)
4. [ ] Verify countdown appears (5 seconds)
5. [ ] Verify "Apply Now" button works (instant update)
6. [ ] Verify "Cancel" button works (aborts update)
7. [ ] Verify actor properties updated
8. [ ] Verify undo works (Ctrl+Z)

---

### Manual Testing (Mode 1)

1. [ ] Enable World Partition
2. [ ] Place actor in external package (WP cell)
3. [ ] Unload cell (navigate away)
4. [ ] Modify definition
5. [ ] Save definition
6. [ ] Load cell (navigate back)
7. [ ] Verify actor updated silently (no countdown)
8. [ ] Check log: `[Mode1] Applied: X`

---

### Manual Testing (Mode 2)

1. [ ] Place actor
2. [ ] Modify definition
3. [ ] Save definition
4. [ ] Run commandlet:
   ```bash
   UnrealEditor-Cmd.exe Alis.uproject -run=WorldPartitionResaveActorsBuilder
   ```
5. [ ] Check log: `[Mode2] Applied definition to: X`
6. [ ] Open level in editor
7. [ ] Verify actor properties updated

---

### Automated Testing (Unit Tests)

```cpp
// Example unit test structure
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FActorSyncApplyTest,
    "Alis.ActorSync.Apply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FActorSyncApplyTest::RunTest(const FString& Parameters)
{
    // Setup: Create definition and actor
    UYourDefinition* Def = NewObject<UYourDefinition>();
    Def->Property1 = 123;

    AYourActor* Actor = GetWorld()->SpawnActor<AYourActor>();
    Actor->ObjectDefinitionId = Def->GetPrimaryAssetId();

    // Execute: Apply definition
    bool bSuccess = Actor->ApplyDefinition(Def);

    // Verify: Properties updated
    TestTrue("ApplyDefinition succeeded", bSuccess);
    TestEqual("Property1 updated", Actor->YourProperty1, 123);

    return true;
}
```

---

## Known Limitations

The system has three known limitations:

1. **Capability updates not implemented** - Mesh updates work, capability changes require Replace/manual fix
2. **Mode 1 ObjectDefinition-specific** - ItemDefinition actors don't get fast-skip (~0.1ms overhead per actor)
3. **ItemDefinition Replace not implemented** - Manual fix required for structural changes

**For detailed explanations, workarounds, and implementation plans, see [TODO.md](../TODO.md).**

**Quick workarounds:**
- Capability changes: Mode 0 auto Replace, Mode 1/2 manual fix
- ItemDefinition structural changes: Delete + re-place from Content Browser

---

## Related Files

### Documentation
- [ActorSyncArchitecture.md](./ActorSyncArchitecture.md) - Full architecture documentation
- [TODO.md](../TODO.md) - Outstanding tasks, known limitations, implementation plans

### Source Files
- [DefinitionActorSyncSubsystem.h](../Source/ProjectPlacementEditor/Public/DefinitionActorSyncSubsystem.h)
- [DefinitionActorSyncSubsystem.cpp](../Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp)
- [ProjectWorldActor.h](../../World/ProjectWorld/Source/ProjectWorld/Public/ProjectWorldActor.h)
- [ProjectWorldActor.cpp](../../World/ProjectWorld/Source/ProjectWorld/Private/ProjectWorldActor.cpp)
- [IDefinitionApplicable.h](../../World/ProjectWorld/Source/ProjectWorld/Public/IDefinitionApplicable.h)
- [InteractableActor.cpp](../../Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp)
- [ProjectPickupItemActor.cpp](../../Resources/ProjectItems/Source/ProjectItems/Private/Pickup/ProjectPickupItemActor.cpp)
