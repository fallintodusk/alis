# Actor Sync Architecture

## Overview

The Actor Sync system provides automatic synchronization of placed world actors when their underlying data definitions are modified. It implements a three-mode update strategy to handle different workflows: manual editor updates, passive World Partition streaming, and batch commandlet processing.

**Key Components:**
- `UDefinitionActorSyncSubsystem` - Orchestrates updates, manages timing and notifications
- `IDefinitionApplicable` interface - Allows actors to handle their specific definition types polymorphically
- `AProjectWorldActor::PreSave` - Mode 2 batch update hook
- Content hash validation - Idempotency (skip redundant updates)
- Structure hash validation - Safety (prevent incompatible updates)

---

## Three-Mode Update Strategy

### Mode 0: Manual Editor Updates (Interactive)
**When:** Definition regenerated in editor (user saves definition asset)
**How:** `FDefinitionEvents::OnDefinitionRegenerated` delegate
**Behavior:**
- Shows 5-second countdown notification
- "Apply Now" button (skip countdown)
- "Cancel" button (abort update)
- Finds all actors referencing the updated definition
- Attempts Apply (in-place update via IDefinitionApplicable)
- Falls back to Replace (respawn actor) if Apply fails
- Undo support via FScopedTransaction

**Use case:** Designer modifies a door definition, all door actors update after 5-second countdown.

**Implementation:** [DefinitionActorSyncSubsystem.cpp:205-274](../Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp#L205-L274)

---

### Mode 1: Passive World Partition Updates (Silent)
**When:** Actors loaded from external packages (World Partition streaming, level load)
**How:** `ULevel::OnLoadedActorAddedToLevelPostEvent` delegate
**Behavior:**
- Silent updates (no countdown, no UI)
- Content hash check (skip if already up to date)
- Apply definition via IDefinitionApplicable
- Show warning notification only on structural mismatch
- Clickable Message Log entry to navigate to problem actors

**Use case:** Designer opens a World Partition cell, stale actors auto-update on load.

**Implementation:** [DefinitionActorSyncSubsystem.cpp:575-621](../Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp#L575-L621)

**Current limitation:** Hash checking uses `Cast<UObjectDefinition>(Def)` - only works for ObjectDefinition types. ItemDefinition actors won't get Mode 1 fast-skip optimization (will call ApplyDefinition every load, but actors do internal hash check).

**Crash Fix (UE 5.7):**
Cannot access `GEditor->GetEditorWorldContext().World()` during subsystem `Initialize()` - context not fully set up yet. Solution: Subscribe to `FWorldDelegates::OnPostWorldCreation` to defer level hook setup until world is ready.

---

### Mode 2: Batch Commandlet Updates (Silent)
**When:** Actors saved via commandlet or manual save
**How:** `AProjectWorldActor::PreSave` override
**Behavior:**
- Silent updates during save (no UI)
- Idempotent via content hash check (actor's ApplyDefinition checks hash internally)
- Runs for all AProjectWorldActor subclasses
- Logs success/failure to UE_LOG and Message Log
- Skips procedural saves (e.g., during transactions)

**Use case:** Bulk resave commandlet (`WorldPartitionResaveActorsBuilder`) updates thousands of actors without manual intervention.

**Implementation:** [ProjectWorldActor.cpp:23-83](../../World/ProjectWorld/Source/ProjectWorld/Private/ProjectWorldActor.cpp#L23-L83)

**Commandlet usage:**
```bash
UnrealEditor-Cmd.exe Alis.uproject -run=WorldPartitionResaveActorsBuilder -AllowCommandletRendering
```

---

## Safety Features

### 1. Structure Hash Validation
**Purpose:** Prevent applying incompatible definition changes (e.g., door -> chest)

**How it works:**
- Each definition type has `DefinitionStructureHash` (computed from UPROPERTIES via reflection)
- Actor stores `AppliedStructureHash` from last successful apply
- Before applying, actor compares hashes:
  - **Match:** Apply in-place (safe)
  - **Mismatch:** Return false -> Replace or Manual Fix

**Where implemented:**
- [InteractableActor.cpp:164-173](../../../Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp#L164-L173)
- [ProjectPickupItemActor.cpp:285-294](../../../Resources/ProjectItems/Source/ProjectItems/Private/Pickup/ProjectPickupItemActor.cpp#L285-L294)

**Example:**
```cpp
// Door definition changes from FDoorData to FChestData (structure mismatch)
if (AppliedStructureHash != ObjDef->DefinitionStructureHash)
{
    // REJECT - cannot apply incompatible structure
    return false;
}
```

---

### 2. Content Hash Idempotency
**Purpose:** Skip redundant updates (definition unchanged)

**How it works:**
- Each definition has `DefinitionContentHash` (computed from property values)
- Actor stores `AppliedContentHash` from last successful apply
- Before applying, actor compares hashes:
  - **Match:** Return true immediately (no work needed)
  - **Mismatch:** Proceed with update

**Where implemented:**
- [InteractableActor.cpp:174-179](../../../Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp#L174-L179)
- [ProjectPickupItemActor.cpp:295-300](../../../Resources/ProjectItems/Source/ProjectItems/Private/Pickup/ProjectPickupItemActor.cpp#L295-L300)

**Example:**
```cpp
// Mode 2 PreSave runs on every save - content hash prevents redundant work
if (AppliedContentHash == ObjDef->DefinitionContentHash)
{
    // Already up to date - no update needed
    return true;
}
```

---

### 3. Interface-Based Polymorphism
**Purpose:** Each actor knows HOW to apply its specific definition type

**Architecture:**
- Subsystem knows WHEN to update (orchestration, timing, notifications)
- Actor knows HOW to apply (definition casting, property mapping, mesh updates)

**Interface:**
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
    UFUNCTION(BlueprintNativeEvent, Category = "Definition")
    bool ApplyDefinition(UPrimaryDataAsset* Definition);
};
```

**Actor implementations:**
- `AInteractableActor::ApplyDefinition_Implementation` - Casts to `UObjectDefinition`, updates meshes (capabilities TODO)
- `AProjectPickupItemActor::ApplyDefinition_Implementation` - Casts to `UItemDefinition`, sets mesh/properties

**Benefits:**
- **Type-safe at actor level:** Actor casts definition to expected type (fails gracefully if wrong type)
- **Extensible:** New actor types just implement IDefinitionApplicable
- **Partially decoupled:** Subsystem orchestration is generic, but Mode 1 hash checking is ObjectDefinition-specific (see limitations below)

**Current limitations:**
- Mode 1 (passive updates) uses `Cast<UObjectDefinition>(Def)` to access hash fields for early-return optimization
- To make subsystem fully generic, either:
  - Option A: Always call `ApplyDefinitionToActor()` (actors do hash check internally)
  - Option B: Create `UProjectDefinitionBase` class with standardized hash fields

---

## Update Flow Diagram

```
[Definition Regenerated] -> Mode 0: Manual Update
    |
    +-> Find Actors (TActorIterator<AProjectWorldActor>)
    |
    +-> Show Countdown (5 sec, Apply Now/Cancel)
    |
    +-> For Each Actor:
        |
        +-> Try Apply (IDefinitionApplicable::ApplyDefinition)
        |   |
        |   +-> Structure Hash Check
        |   |   |
        |   |   +-> MATCH -> Content Hash Check
        |   |   |            |
        |   |   |            +-> MATCH -> Return true (no work)
        |   |   |            |
        |   |   |            +-> MISMATCH -> Update Properties
        |   |   |
        |   |   +-> MISMATCH -> Return false (incompatible)
        |   |
        |   +-> SUCCESS -> Log "Applied"
        |   |
        |   +-> FAIL -> Try Replace (ObjectDefinition only)
        |               |
        |               +-> Spawn new actor
        |               |
        |               +-> Copy transform/label/folder
        |               |
        |               +-> Delete old actor

[Actor Loaded from WP] -> Mode 1: Passive Update
    |
    +-> Content Hash Check
    |   |
    |   +-> MATCH -> Skip (already up to date)
    |   |
    |   +-> MISMATCH -> Try Apply (IDefinitionApplicable)
    |                   |
    |                   +-> SUCCESS -> Log "Applied"
    |                   |
    |                   +-> FAIL -> Show Warning Notification

[Actor PreSave] -> Mode 2: Batch Update
    |
    +-> Check: ObjectDefinitionId valid?
    |
    +-> Check: Implements IDefinitionApplicable?
    |
    +-> Load Definition (UAssetManager)
    |
    +-> Try Apply (IDefinitionApplicable)
        |
        +-> Structure Hash Check (inside ApplyDefinition)
        |   |
        |   +-> MATCH -> Content Hash Check (idempotency)
        |   |            |
        |   |            +-> MATCH -> Return true (no work)
        |   |            |
        |   |            +-> MISMATCH -> Update Properties
        |   |
        |   +-> MISMATCH -> Return false (incompatible)
        |
        +-> SUCCESS -> Log "[Mode2] Applied"
        |
        +-> FAIL -> Log Warning to Message Log
```

---

## Implementation Details

### Subsystem Initialization (Fixed for UE 5.7)

**Problem:**
During `UDefinitionActorSyncSubsystem::Initialize()`, calling `GEditor->GetEditorWorldContext().World()` crashes because the editor world context is not fully initialized yet.

**Solution:**
Defer level hook setup using `FWorldDelegates::OnPostWorldCreation`:

```cpp
void UDefinitionActorSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Subscribe to world creation delegate (deferred level hook setup)
    OnWorldCreatedHandle = FWorldDelegates::OnPostWorldCreation.AddUObject(
        this, &UDefinitionActorSyncSubsystem::OnWorldCreated);
}

void UDefinitionActorSyncSubsystem::OnWorldCreated(UWorld* World)
{
    // Only care about editor worlds
    if (!World || World->WorldType != EWorldType::Editor)
    {
        return;
    }

    // Now safe to set up level hooks
    for (ULevel* Level : World->GetLevels())
    {
        if (Level)
        {
            OnLoadedActorAddedHandle = Level->OnLoadedActorAddedToLevelPostEvent.AddUObject(
                this, &UDefinitionActorSyncSubsystem::OnActorsLoadedIntoLevel);
        }
    }
}
```

**Key points:**
- Subscribe to `FWorldDelegates::OnPostWorldCreation` in `Initialize()`
- Set up level hooks in `OnWorldCreated()` callback
- Filter for `EWorldType::Editor` (ignore PIE, game, etc.)
- Clean up delegate handle in `Deinitialize()`

---

### Mode 2 PreSave Guard Conditions

The PreSave hook has multiple guards to prevent unwanted updates:

```cpp
void AProjectWorldActor::PreSave(FObjectPreSaveContext SaveContext)
{
    Super::PreSave(SaveContext);

    // Guard 1: Only update if actor has a definition link
    if (!ObjectDefinitionId.IsValid())
    {
        return;
    }

    // Guard 2: Skip procedural saves (transactions, auto-saves)
    if (SaveContext.IsProceduralSave())
    {
        return;
    }

    // Guard 3: Only update if actor implements IDefinitionApplicable
    if (!this->Implements<UDefinitionApplicable>())
    {
        return;
    }

    // Proceed with update...
}
```

**Why these guards matter:**
- **ObjectDefinitionId check:** Prevents processing non-data-driven actors
- **IsProceduralSave check:** Prevents updates during undo/redo, auto-saves, transactions
- **Interface check:** Type safety - only actors that can apply definitions get updated

---

### Apply vs Replace Strategy

**Apply (In-Place Update):**
- Actor implements `IDefinitionApplicable::ApplyDefinition`
- Updates properties without respawning
- Preserves actor GUID, references, Blueprint connections
- **Preferred method** (cheaper, safer)

**Replace (Respawn):**
- Spawn new actor from definition
- Copy transform, label, folder path
- Delete old actor
- **Fallback method** when Apply fails (structural mismatch)

**Decision logic:**
```cpp
if (ApplyDefinitionToActor(Actor, Def))
{
    // SUCCESS: Applied in-place
    ReappliedCount++;
}
else
{
    // FAIL: Structure mismatch or interface not implemented
    if (DefId.PrimaryAssetType == FName(TEXT("ObjectDefinition")))
    {
        // Replace (ObjectDefinition only - factory exists)
        AActor* NewActor = ReplaceActorFromDefinition(Actor, Def);
        ReplacedCount++;
    }
    else
    {
        // No factory for this definition type - needs manual fix
        ManualFixNeeded.Add(Actor);
    }
}
```

---

## Logging and Diagnostics

### Log Categories

**LogDefinitionActorSync** (Subsystem):
- `[ActorSync]` - Mode 0 manual updates
- `[Mode1]` - Passive World Partition updates
- No Mode 2 tag (uses LogProjectWorldActor)

**LogProjectWorldActor** (Actor PreSave):
- `[Mode2]` - Batch commandlet updates

### Example Log Output

**Mode 0 (Manual):**
```
LogDefinitionActorSync: [ActorSync] Found 12 actors to update - showing countdown
LogDefinitionActorSync: [ActorSync] Executing update for 12 actors
LogDefinitionActorSync: [ActorSync] Applied: Door_Instance_01
LogDefinitionActorSync: [ActorSync] Applied: Door_Instance_02
LogDefinitionActorSync: [ActorSync] Replacing: Door_Instance_03
LogDefinitionActorSync: [ActorSync] Replace complete: Door_Instance_03 -> Door_Instance_03_NEW
LogDefinitionActorSync: [ActorSync] === Update complete ===
LogDefinitionActorSync:   Definition: ObjectDefinition:Door_Basic
LogDefinitionActorSync:   Reapplied: 10 actors
LogDefinitionActorSync:   Replaced: 2 actors
LogDefinitionActorSync:   Manual fix needed: 0 actors
LogDefinitionActorSync:   Time: 45.23ms
```

**Mode 1 (Passive):**
```
LogDefinitionActorSync: Subscribed to ULevel::OnLoadedActorAddedToLevelPostEvent (Mode 1) for world: Kazan_MainCity
LogDefinitionActorSync: [Mode1] Actor Door_Instance_04 needs update: 1a2b3c4d -> 5e6f7g8h
LogDefinitionActorSync: [Mode1] Applied: Door_Instance_04
```

**Mode 2 (Batch):**
```
LogProjectWorldActor: [Mode2] Applied definition to: Door_Instance_05
LogProjectWorldActor: Warning: [Mode2] Failed to apply definition to: Chest_Instance_01 (structure mismatch or error)
```

### Message Log Integration

All warnings/errors are logged to the Message Log with clickable actor references:

```cpp
FMessageLog MessageLog("PIE");
TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
Message->AddToken(FTextToken::Create(/* message text */));
Message->AddToken(FUObjectToken::Create(Actor)->OnMessageTokenActivated(
    FOnMessageTokenActivated::CreateLambda([](const TSharedRef<IMessageToken>& Token)
    {
        // Select actor in viewport when clicked
        GEditor->SelectActor(TokenActor, true, true);
        GEditor->MoveViewportCamerasToActor(*TokenActor, false);
    })
));
MessageLog.AddMessage(Message);
MessageLog.Open(EMessageSeverity::Warning);
```

---

## Troubleshooting

### Problem: Actors not updating in World Partition

**Symptom:** Actors load from external packages but don't update.

**Diagnosis:**
1. Check log for Mode 1 subscription: `Subscribed to ULevel::OnLoadedActorAddedToLevelPostEvent`
2. Check actor has valid `ObjectDefinitionId`
3. Check content hash logged: `[Mode1] Actor X needs update: old_hash -> new_hash`

**Fix:**
- Ensure World Partition is enabled
- Verify actor stored in external package (not persistent level)
- Check definition asset has `DefinitionContentHash` set

---

### Problem: Commandlet not updating actors

**Symptom:** `WorldPartitionResaveActorsBuilder` runs but actors not updated.

**Diagnosis:**
1. Check PreSave guards (ObjectDefinitionId valid? IsProceduralSave false?)
2. Check actor implements `IDefinitionApplicable`
3. Check Mode 2 logs: `[Mode2] Applied` or `[Mode2] Failed`

**Fix:**
- Verify `ObjectDefinitionId` is set on actor
- Ensure actor class implements `IDefinitionApplicable` interface
- Check definition asset is registered with `UAssetManager`

---

### Problem: Structure mismatch warnings

**Symptom:** Warning: "Actor 'X' failed definition update (structural change?)"

**Cause:** Definition structure changed (added/removed properties, changed types).

**Fix:**
- **Mode 0:** System automatically tries Replace (respawn actor)
- **Mode 1/2:** Manual fix required (delete old actor, place new instance)

**How to prevent:**
- Use Blueprint properties instead of C++ UPROPERTY (more flexible)
- Keep definition structures stable (add optional fields instead of changing existing)

---

### Problem: Crash on editor startup (UE 5.7)

**Symptom:** Crash in `UEditorEngine::GetEditorWorldContext` during subsystem init.

**Cause:** Trying to access world context before it's fully initialized.

**Fix:** Already fixed - we now use `FWorldDelegates::OnPostWorldCreation` to defer level hook setup.

---

## Performance Considerations

### Mode 0 (Manual)
- **Actor search:** O(N) where N = total actors in level (uses TActorIterator)
- **Apply time:** ~5ms per actor (depends on definition complexity)
- **UI blocking:** None (countdown runs on timer, async)

**Optimization:** Filter by PrimaryAssetType before iterating (if only ObjectDefinition changed, skip items)

---

### Mode 1 (Passive)
- **Per-actor cost:** ~2ms (load definition, hash check, apply)
- **Streaming impact:** Minimal (only processes newly loaded actors)
- **Content hash check:** Early return if up to date (0.1ms)

**Optimization:** Batch updates if many actors load simultaneously (defer until frame end)

---

### Mode 2 (Batch)
- **Per-actor cost:** ~3ms (PreSave overhead + apply)
- **Commandlet scaling:** Linear with actor count
- **Idempotency:** Content hash check prevents redundant work

**Optimization:** Commandlet runs headless (no UI), can process thousands of actors per minute

---

## Known Limitations

The system has three known limitations that don't block core functionality:

1. **Capability updates not implemented** - Mesh updates work, capability add/remove requires Replace or manual fix
2. **Mode 1 ObjectDefinition-specific** - ItemDefinition actors don't get subsystem-level fast-skip optimization (minimal overhead)
3. **ItemDefinition Replace not implemented** - Structural changes require manual fix (ObjectDefinition has auto-replace)

**See [TODO.md](../TODO.md) for detailed explanations, workarounds, and implementation plans.**

---

## Future Enhancements

### Potential Improvements

1. **Async definition loading (Mode 1):**
   - Currently uses `TryLoad()` (synchronous)
   - Could use `StreamableManager.RequestAsyncLoad()` to avoid frame hitches

2. **Batch notification (Mode 1):**
   - Currently shows notification per actor
   - Could batch multiple updates into single notification ("5 actors updated silently")

3. **Partial updates:**
   - Currently updates all properties on mismatch
   - Could diff property values and only update changed fields

4. **Undo support (Mode 1/2):**
   - Currently no undo for passive/batch modes
   - Could wrap in FScopedTransaction (editor only)

5. **Factory registry:**
   - Currently hardcoded check for `ObjectDefinition`
   - Could register factories by definition type (support ItemDefinition replace)

---

## Related Documentation

- [TODO](../TODO.md) - Outstanding tasks and implementation plans
- [Quick Reference](./ActorSyncQuickReference.md) - Common tasks and troubleshooting
- [IDefinitionApplicable Interface](../../World/ProjectWorld/Public/IDefinitionApplicable.h)
- [UDefinitionGeneratorSubsystem](../../Editor/ProjectDefinitionGenerator/docs/DefinitionGenerator.md)
- [UProjectObjectActorFactory](../Source/ProjectPlacementEditor/Public/ProjectObjectActorFactory.h)

---

## Summary

The Actor Sync system provides multi-mode synchronization of world actors with their data definitions:

- **Mode 0:** Interactive updates with user confirmation (countdown + Apply/Cancel)
- **Mode 1:** Silent updates when actors load from World Partition
- **Mode 2:** Batch updates during commandlet saves

**Safety features:**
- Structure hash prevents incompatible updates
- Content hash prevents redundant updates (idempotency)
- Interface-based polymorphism (type-safe at actor level)

**Performance:**
- Idempotent (skips unchanged actors via hash checks)
- Scales to thousands of actors (commandlet batch processing)
- Mode 1 fast-skip for ObjectDefinition actors (ItemDefinition actors check internally)

**Developer experience:**
- Automatic updates for mesh/property changes (no manual re-placement)
- Clear diagnostics (logs + Message Log with clickable actor links)
- Replace fallback for ObjectDefinition structural changes
- Manual fix required for ItemDefinition structural changes

**Known limitations:**
- Capability updates not implemented (meshes work, capabilities TODO)
- Mode 1 hash checking is ObjectDefinition-specific (ItemDefinition works but no fast-skip)
- ItemDefinition Replace not implemented (manual fix required for structural changes)

See [TODO.md](../TODO.md) for detailed explanations, workarounds, and implementation plans.
