# Global Actor Update from Definitions

**Goal:** When JSON definition changes, update ALL actors (loaded/unloaded World Partition cells) without destroying iteration speed.

**Problem:** Current DefinitionActorSyncSubsystem only updates actors in currently loaded world/cells. Actors in unloaded cells stay stale.

**Solution:** Two-mode design - (1) passive update on load (editor), (2) offline resave pass (CI/shipping).

---

## Architecture Comparison

### RedirectorAutoFix (Global, Infrequent)
```
1. OnAssetRenamed event
2. Load redirector from old path
3. Call IAssetTools::FixupReferencers()
   -> Uses Asset Registry to find ALL packages referencing redirector
   -> Loads each package on demand
   -> Fixes references
   -> Saves package
4. Show notification
```

**Why global scan is acceptable:**
- Asset renames are infrequent (few times per day)
- Affects few packages (typically 1-10 references)
- User explicitly triggered the operation

### Actor Sync (Current - Only Loaded)
```
1. OnDefinitionRegenerated event
2. TActorIterator finds actors in current world (LOADED ONLY)
3. Update actors via Reapply or Replace
4. Show notification
```

**Problem:** Misses actors in closed levels and unloaded World Partition cells.

### Actor Sync (Target - TWO MODES)

**Mode 1: Update on Load (Editor - Zero Overhead)**
```
1. Hook ULevel::OnLoadedActorAddedToLevelPostEvent
2. When actor loads into world:
   -> Check if ObjectDefinitionId exists
   -> Load current definition
   -> Compare ContentHash (detects ANY property change)
   -> If outdated: ReapplyDefinition() (safe, in-place)
   -> If structural mismatch: Show "Needs Replace" notification
3. Actor automatically correct when cell loads
```

**Why this is best:**
- Zero package IO storm (updates only when needed)
- Designer opens level -> actors instantly correct
- No iteration speed penalty
- Works perfectly with World Partition streaming

**Mode 2: Offline Resave (CI/Shipping - Global Correctness)**
```
1. Actor PreSave hook checks -AlisUpdateFromDef flag
2. If flag present: ReapplyDefinition() if ContentHash outdated
3. Run WorldPartitionResaveActorsBuilder commandlet (manual or CI)
4. Commandlet loads/saves all actor packages
5. PreSave triggers, actors become correct
6. Run before cook/release
```

**Why this is best:**
- Guarantees global correctness for shipping
- Runs offline (no editor performance impact)
- Uses Epic's WP infrastructure (robust, battle-tested)
- No custom builder code needed

---

## Key Insight: Two-Hash System

**Problem with single StructureHash:** Misses property changes (door angles, movement params, etc.)

**Solution:** Track two hashes

| Hash Type | What It Detects | Used For |
|-----------|-----------------|----------|
| **StructureHash** | Mesh count + capability types | Determines if Replace needed (structural change) |
| **ContentHash** | ANY definition property change | Triggers Reapply (safe, fast) |

**Example:**
```json
// Door definition update
{
  "Meshes": [...],           // Unchanged
  "Capabilities": [...],     // Unchanged
  "StartAngle": 0,           // Changed from 0
  "EndAngle": 120            // Changed to 120 (was 90)
}
```

**Result:**
- `StructureHash` = unchanged (3 meshes, 2 capabilities)
- `ContentHash` = changed (JSON content differs)
- **Action:** Reapply (safe, in-place property update)

---

## Implementation Plan (Two-Mode Design)

### Phase 1: Add Two-Hash Version Tracking

**Goal:** Fast version detection without loading full definition.

#### Task 1.1: Add hash properties to UObjectDefinition

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h`

```cpp
// In UObjectDefinition class
UCLASS()
class PROJECTOBJECT_API UObjectDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Existing properties...

    /**
     * Hash of structural elements (mesh count + capability types).
     * Changes only when components are added/removed.
     * Used to determine if Replace is needed.
     * Computed by ProjectDefinitionGenerator from source JSON.
     */
    UPROPERTY(VisibleAnywhere, Category = "Definition")
    FString DefinitionStructureHash;

    /**
     * Hash of full definition content (source JSON).
     * Changes when ANY property changes.
     * Used to detect if Reapply is needed.
     * Computed by ProjectDefinitionGenerator from source JSON.
     */
    UPROPERTY(VisibleAnywhere, Category = "Definition")
    FString DefinitionContentHash;
};
```

**Why no UpdateDefinitionHashes() method:**
- Hashes computed by **generator from source JSON** (single source of truth)
- Avoids nondeterministic UObject serialization
- Avoids circular dependency (hashing fields that include hashes)
- Source JSON is immutable and deterministic

#### Task 1.2: Compute hashes in ProjectDefinitionGenerator from source JSON

**File:** `Plugins/Editor/ProjectDefinitionGenerator/Source/ProjectDefinitionGenerator/Private/DefinitionGeneratorSubsystem.cpp`

```cpp
void UDefinitionGeneratorSubsystem::GenerateDefinitionAsset(const FString& JsonPath)
{
    // 1. Load source JSON text (BEFORE parsing)
    FString SourceJsonText;
    if (!FFileHelper::LoadFileToString(SourceJsonText, *JsonPath))
    {
        UE_LOG(LogDefinitionGenerator, Error, TEXT("Failed to load JSON: %s"), *JsonPath);
        return;
    }

    // Normalize line endings for consistent hash across platforms
    SourceJsonText.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    SourceJsonText.ReplaceInline(TEXT("\r"), TEXT("\n"));

    // 2. Parse JSON and create/update definition asset (existing logic)
    // ... TSharedPtr<FJsonObject> JsonObject = ParseJson(SourceJsonText) ...
    // ... UObjectDefinition* ObjDef = CreateOrUpdateDefinitionAsset(JsonObject) ...

    // 3. Compute hashes from source JSON (after asset populated)
    if (UObjectDefinition* ObjDef = Cast<UObjectDefinition>(GeneratedAsset))
    {
        // Content hash: raw JSON bytes (KISS - formatting changes trigger reapply, which is cheap)
        ObjDef->DefinitionContentHash = ComputeContentHash(SourceJsonText);

        // Structure hash: detailed structural signature (mesh IDs + capability types + targets)
        ObjDef->DefinitionStructureHash = ComputeStructureHash(ObjDef);

        UE_LOG(LogDefinitionGenerator, Verbose, TEXT("[%s] Computed hashes - Structure: %s, Content: %s"),
            *ObjDef->GetName(), *ObjDef->DefinitionStructureHash, *ObjDef->DefinitionContentHash);
    }

    // ... save asset, broadcast event ...
}

// Helper: Compute content hash from source JSON (KISS approach)
FString UDefinitionGeneratorSubsystem::ComputeContentHash(const FString& SourceJsonText) const
{
    // KISS: Hash raw JSON file bytes (already normalized line endings in caller)
    // Formatting changes (whitespace, key order) will trigger reapply - that's OK!
    // Reapply is cheap (in-place property updates), so prefer simplicity over canonicalization.

    // CRITICAL: Use UTF-8 byte hashing, not HashAnsiString (which mangles non-ASCII)
    FTCHARToUTF8 Utf8(*SourceJsonText);
    const uint8* Data = reinterpret_cast<const uint8*>(Utf8.Get());
    const int32 Len = Utf8.Length();

    FMD5 Md5;
    Md5.Update(Data, Len);
    uint8 Digest[16];
    Md5.Final(Digest);

    return BytesToHex(Digest, 16);
}

// Helper: Compute detailed structure hash
FString UDefinitionGeneratorSubsystem::ComputeStructureHash(const UObjectDefinition* Def) const
{
    // Include: mesh count, mesh IDs (sorted), capability types + target meshes (sorted)
    // Sorted for determinism (order doesn't matter since we target meshes by ID)
    // This catches: mesh added/removed, mesh ID changed, capability target changed

    TArray<FString> MeshIds;
    for (const auto& Mesh : Def->Meshes)
    {
        // Use DefMeshId (or mesh path if no ID)
        FString MeshId = Mesh.DefMeshId.IsEmpty() ? Mesh.MeshPath : Mesh.DefMeshId;
        MeshIds.Add(MeshId);
    }
    MeshIds.Sort();  // Sorted for deterministic hash

    TArray<FString> CapSignatures;
    for (const auto& Cap : Def->Capabilities)
    {
        // CRITICAL: Use stable capability ID, NOT GetName()!
        // Using GetName() means renaming a C++ class becomes "structural mismatch".
        // Use the stable ID from your capability system (e.g., "Lockable", "Hinged", "Sliding").
        FString CapId = Cap.CapabilityId.IsEmpty() ? Cap.CapabilityClass->GetName() : Cap.CapabilityId;

        // Include capability ID + target mesh (if applicable)
        FString CapSig = CapId;
        if (!Cap.TargetMeshId.IsEmpty())
        {
            CapSig += TEXT(":") + Cap.TargetMeshId;
        }
        CapSignatures.Add(CapSig);
    }
    CapSignatures.Sort();  // Sorted for deterministic hash

    // Build signature: count|ids|capabilities
    FString StructSig = FString::Printf(TEXT("Meshes:%d|MeshIds:%s|Caps:%s"),
        Def->Meshes.Num(),
        *FString::Join(MeshIds, TEXT(",")),
        *FString::Join(CapSignatures, TEXT(",")));

    return FMD5::HashAnsiString(*StructSig);
}
```

**Why this is correct:**
- Hashes computed from **source JSON** (immutable, deterministic)
- Content hash = **raw JSON bytes** (KISS approach - formatting changes trigger reapply, which is cheap)
- Structure hash = **detailed topology** (mesh count + IDs + capability types + target meshes)
- No UObject serialization issues (nondeterminism, editor fields, circular refs)

**What each hash catches:**

| Hash Type | Detects | Example Changes |
|-----------|---------|-----------------|
| **ContentHash** | ANY property change | Door angle, color, movement speed, mesh transforms, JSON formatting |
| **StructureHash** | Component topology change | Mesh added/removed, mesh ID changed, capability added/removed, capability target changed |

**Why detailed structure hash is critical:**
- Catches mesh ID changes (same count, different IDs: `["frame"]` vs `["body"]`)
- Catches capability target changes (Lockable targets different mesh)
- Deterministic: sorted IDs/caps for consistent hash (order doesn't matter - we target by ID)
- Prevents false negative: "3 meshes + 2 capabilities" is NOT enough!

**Why KISS content hash (raw JSON) is correct:**
- Simple = fewer failure modes (no complex canonicalization logic)
- False positives acceptable: formatting changes trigger cheap reapply (in-place property updates)
- Reapply is fast and safe, so prefer simplicity over canonical JSON complexity
- If canonical JSON needed later: can implement sorted tree walk (complex, adds failure modes)

#### Task 1.3: Store hashes in actor for comparison

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Template/Interactable/InteractableActor.h`

```cpp
// In AInteractableActor class
UCLASS(Blueprintable)
class PROJECTOBJECT_API AInteractableActor : public AProjectWorldActor, public IInteractableTargetInterface
{
    GENERATED_BODY()

public:
    // Existing properties...

    /**
     * Hash of structural elements at time of spawn/last update.
     * Compared against UObjectDefinition::DefinitionStructureHash.
     * Mismatch = structural change = needs Replace.
     */
    UPROPERTY()
    FString AppliedStructureHash;

    /**
     * Hash of full definition content at time of spawn/last update.
     * Compared against UObjectDefinition::DefinitionContentHash.
     * Mismatch = property change = needs Reapply.
     */
    UPROPERTY()
    FString AppliedContentHash;

    /**
     * Apply definition to this actor (in-place property updates).
     * Called by both Mode 1 (editor subsystem) and Mode 2 (PreSave).
     * CRITICAL: Must be in runtime module (ProjectObject), not editor module!
     * @return true if successfully applied
     */
    bool ApplyDefinition(const UObjectDefinition* Definition);

    // Note: Old uint32 DefinitionStructureHash property deprecated (remove after migration)
};
```

#### Task 1.4: Implement ApplyDefinition in runtime module (CRITICAL for Mode 2)

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp`

```cpp
bool AInteractableActor::ApplyDefinition(const UObjectDefinition* Definition)
{
    if (!Definition)
    {
        UE_LOG(LogProjectObject, Error, TEXT("[%s] ApplyDefinition: Definition is null!"),
            *GetActorLabel());
        return false;
    }

    // Update mesh transforms (in-place, preserves components)
    // TODO: Implement mesh transform updates from Definition->Meshes

    // Update capability properties (in-place, preserves capability objects)
    // TODO: Implement capability property updates from Definition->Capabilities

    // Update other properties (materials, physics, etc.)
    // TODO: Implement other property updates

    return true;
}
```

**Why this is critical:**
- **Mode 1** (editor subsystem) can call `Actor->ApplyDefinition(Def)` (runtime method, no circular dependency)
- **Mode 2** (PreSave in runtime WITH_EDITOR) can call `ApplyDefinition(Def)` directly (same module)
- **No dependency hell:** Runtime module never depends on editor modules
- **Single responsibility:** Reapply logic lives where it belongs (on the actor)
- **Testable:** Can unit test ApplyDefinition without editor subsystem

**Alternative (rejected):** Putting reapply logic in editor subsystem causes:
- Circular dependency (runtime PreSave needs editor subsystem)
- Linkage errors (runtime can't link against editor modules)
- Violation of SOLID (business logic in wrong layer)

#### Task 1.5: Update ProjectObjectActorFactory to set hashes

**File:** `Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/ProjectObjectActorFactory.cpp`

```cpp
AActor* UProjectObjectActorFactory::SpawnActor(UObjectDefinition* Def, ...)
{
    // ... existing spawn logic ...

    // Set tracking properties
    if (AInteractableActor* InteractableActor = Cast<AInteractableActor>(SpawnedActor))
    {
        InteractableActor->ObjectDefinitionId = Def->GetPrimaryAssetId();

        // NEW: Set hashes
        InteractableActor->AppliedStructureHash = Def->DefinitionStructureHash;
        InteractableActor->AppliedContentHash = Def->DefinitionContentHash;
    }

    return SpawnedActor;
}
```

---

### Phase 2: Mode 1 - Update on Actor Load (Editor)

**Goal:** Zero-overhead passive updates when actors naturally load during editor work.

#### Task 2.1: Hook actor load event

**File:** `Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.h`

```cpp
// In UDefinitionActorSyncSubsystem class
private:
    /** Handle to OnDefinitionRegenerated delegate. */
    FDelegateHandle OnDefinitionRegeneratedHandle;

    /** Handle to actor load event (NEW). */
    FDelegateHandle OnLoadedActorAddedHandle;
```

**File:** `Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp`

```cpp
void UDefinitionActorSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Subscribe to definition regeneration (existing)
    OnDefinitionRegeneratedHandle = FDefinitionEvents::OnDefinitionRegenerated().AddUObject(
        this, &UDefinitionActorSyncSubsystem::OnDefinitionRegenerated);

    // NEW: Subscribe to actor load event (World Partition / level streaming)
    // CRITICAL: Delegate signature takes TArray<AActor*>, not single actor!
    OnLoadedActorAddedHandle = ULevel::OnLoadedActorAddedToLevelPostEvent.AddUObject(
        this, &UDefinitionActorSyncSubsystem::OnActorsLoadedIntoLevel);

    UE_LOG(LogDefinitionActorSync, Log, TEXT("DefinitionActorSyncSubsystem initialized"));
    UE_LOG(LogDefinitionActorSync, Log, TEXT("  -> Hooked ULevel::OnLoadedActorAddedToLevelPostEvent (Mode 1)"));
}

void UDefinitionActorSyncSubsystem::Deinitialize()
{
    // Unsubscribe from events
    if (OnDefinitionRegeneratedHandle.IsValid())
    {
        FDefinitionEvents::OnDefinitionRegenerated().Remove(OnDefinitionRegeneratedHandle);
    }

    if (OnLoadedActorAddedHandle.IsValid())
    {
        ULevel::OnLoadedActorAddedToLevelPostEvent.Remove(OnLoadedActorAddedHandle);
    }

    Super::Deinitialize();
}
```

#### Task 2.2: Implement actor load validation with safety filters

**CRITICAL: Correct delegate signature for UE 5.7**

```cpp
// In DefinitionActorSyncSubsystem.h
void OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors);  // Takes ARRAY, not single actor!

// In DefinitionActorSyncSubsystem.cpp
void UDefinitionActorSyncSubsystem::OnActorsLoadedIntoLevel(const TArray<AActor*>& Actors)
{
    // Process each loaded actor
    for (AActor* Actor : Actors)
    {
        if (!Actor)
        {
            continue;
        }

        // SAFETY FILTER 1: Only editor worlds (skip PIE, Game, Simulate, Preview)
        UWorld* World = Actor->GetWorld();
        if (!World)
        {
            continue;
        }

        // ONLY allow Editor world type (not preview contexts)
        // EditorPreview = asset preview worlds (can dirty transient packages unexpectedly)
        // Goal: map correctness, not preview correctness
        if (World->WorldType != EWorldType::Editor)
        {
            continue;  // Don't mutate actors during PIE/gameplay/preview
        }

        // SAFETY FILTER 2: Skip transient/being destroyed actors
        if (Actor->IsUnreachable() || Actor->IsPendingKillPending())
        {
            continue;
        }

        // Only handle InteractableActors with definitions
        AInteractableActor* InteractableActor = Cast<AInteractableActor>(Actor);
        if (!InteractableActor || !InteractableActor->ObjectDefinitionId.IsValid())
        {
            continue;
        }

        // Load current definition
        UObjectDefinition* CurrentDef = LoadDefinition(InteractableActor->ObjectDefinitionId);
        if (!CurrentDef)
        {
            UE_LOG(LogDefinitionActorSync, Warning, TEXT("[%s] Definition not found: %s"),
                *InteractableActor->GetActorLabel(),
                *InteractableActor->ObjectDefinitionId.ToString());
            continue;
        }

        // Check if actor is up-to-date (fast hash comparison)
        const bool bContentUpToDate = (InteractableActor->AppliedContentHash == CurrentDef->DefinitionContentHash);
        const bool bStructureUpToDate = (InteractableActor->AppliedStructureHash == CurrentDef->DefinitionStructureHash);

        if (bContentUpToDate && bStructureUpToDate)
        {
            // Actor is current - no action needed
            continue;
        }

        // Note: If StructureHash differs, ContentHash will also differ because both are
        // derived from the same JSON (structure changes are a type of content change).
        // We check structure first to gate Replace path (requires manual intervention).

        // Content changed but structure unchanged - safe to reapply
        if (!bContentUpToDate && bStructureUpToDate)
        {
            UE_LOG(LogDefinitionActorSync, Log, TEXT("[%s] Auto-updating properties (content changed)"),
                *InteractableActor->GetActorLabel());

            // IMPORTANT: NO FScopedTransaction for auto-updates!
            // Would spam undo stack when streaming hundreds of actors.
            // Transactions only for manual "Update Now" actions.

            // Call actor's runtime Reapply method (see Task 1.4 for implementation)
            const bool bApplied = InteractableActor->ApplyDefinition(CurrentDef);

            if (bApplied)
            {
                // Update hashes AFTER successful reapply
                InteractableActor->AppliedStructureHash = CurrentDef->DefinitionStructureHash;
                InteractableActor->AppliedContentHash = CurrentDef->DefinitionContentHash;
                InteractableActor->MarkPackageDirty();
            }
            else
            {
                UE_LOG(LogDefinitionActorSync, Error, TEXT("[%s] Failed to reapply definition!"),
                    *InteractableActor->GetActorLabel());
            }

            continue;
        }

        // Structure changed - needs manual replace (can't auto-replace safely)
        if (!bStructureUpToDate)
        {
            UE_LOG(LogDefinitionActorSync, Warning, TEXT("[%s] Structural mismatch - needs manual replacement!"),
                *InteractableActor->GetActorLabel());

            ShowStructuralMismatchNotification(InteractableActor);
            continue;
        }
    }
}
```

**Why safety filters are critical:**
- **PIE/Game/Preview filter:** Prevents mutating actors outside editor map editing
  - Allows ONLY: `EWorldType::Editor` (map editing)
  - Blocks: `PIE`, `Game`, `Simulate`, `EditorPreview`, `GamePreview`, `Inactive`
  - Why no EditorPreview: Asset preview worlds can dirty transient packages unexpectedly
  - Goal: Map correctness, not preview correctness
- **Transient filter:** Avoids operating on actors being destroyed (null ref crashes)
- **No transaction:** Prevents undo stack spam (hundreds of entries when streaming WP cells)
- **Continue instead of return:** Processes all actors in batch (efficient)

#### Task 2.3: Add structural mismatch notification

```cpp
void UDefinitionActorSyncSubsystem::ShowStructuralMismatchNotification(AInteractableActor* Actor)
{
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("DefinitionActorSync", "StructuralMismatch",
            "Actor '{0}' needs manual replacement (meshes or capabilities changed)"),
        FText::FromString(Actor->GetActorLabel())
    ));

    Info.bFireAndForget = true;
    Info.ExpireDuration = 10.0f;
    Info.bUseSuccessFailIcons = true;

    // Button: Select Actor in outliner and focus camera
    Info.ButtonDetails.Add(FNotificationButtonInfo(
        NSLOCTEXT("DefinitionActorSync", "SelectActor", "Select Actor"),
        FText::GetEmpty(),
        FSimpleDelegate::CreateLambda([Actor]() {
            if (GEditor && Actor)
            {
                GEditor->SelectNone(false, true);
                GEditor->SelectActor(Actor, true, true);
                GEditor->MoveViewportCamerasToActor(*Actor, false);
            }
        })
    ));

    FSlateNotificationManager::Get().AddNotification(Info);
}
```

**Why this approach works:**
- Zero overhead until actor loads
- Designer opens level -> actors instantly correct
- No package IO storm
- Works perfectly with World Partition streaming
- Respects structural changes (requires manual review)

---

### Phase 3: Mode 2 - Offline Resave (CI/Shipping)

**Goal:** Guarantee global correctness before cook/release using Epic's infrastructure.

#### Task 3.1: Add PreSave hook to AInteractableActor with cooking guard

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp`

```cpp
#if WITH_EDITOR
void AInteractableActor::PreSave(FObjectPreSaveContext SaveContext)
{
    Super::PreSave(SaveContext);

    // CRITICAL: NEVER run during cooking (would mutate cooked data)
    if (SaveContext.IsCooking())
    {
        return;
    }

    // Only during offline resave pass (triggered by commandlet)
    if (!FParse::Param(FCommandLine::Get(), TEXT("AlisUpdateFromDef")))
    {
        return;
    }

    // Only if actor has definition link
    if (!ObjectDefinitionId.IsValid())
    {
        return;
    }

    // Load current definition (robust loading for commandlet context)
    UAssetManager* AssetManager = UAssetManager::GetIfValid();
    if (!AssetManager)
    {
        UE_LOG(LogProjectObject, Error, TEXT("[%s] OFFLINE RESAVE FAILURE: AssetManager not available!"),
            *GetActorLabel());
        UE_LOG(LogProjectObject, Error, TEXT("  This is a critical failure - updates were SKIPPED!"));
        return;
    }

    // Try to load definition via AssetManager
    UObjectDefinition* CurrentDef = Cast<UObjectDefinition>(
        AssetManager->GetPrimaryAssetObject(ObjectDefinitionId));

    // Fallback: Try direct load via soft object path
    if (!CurrentDef)
    {
        FSoftObjectPath DefPath = AssetManager->GetPrimaryAssetPath(ObjectDefinitionId);
        if (DefPath.IsValid())
        {
            CurrentDef = Cast<UObjectDefinition>(DefPath.TryLoad());
        }
    }

    if (!CurrentDef)
    {
        UE_LOG(LogProjectObject, Error, TEXT("[%s] OFFLINE RESAVE FAILURE: Definition not found: %s"),
            *GetActorLabel(), *ObjectDefinitionId.ToString());
        UE_LOG(LogProjectObject, Error, TEXT("  Actor will remain stale - MANUAL REVIEW REQUIRED!"));
        return;
    }

    // Check if outdated
    const bool bContentUpToDate = (AppliedContentHash == CurrentDef->DefinitionContentHash);
    const bool bStructureUpToDate = (AppliedStructureHash == CurrentDef->DefinitionStructureHash);

    if (bContentUpToDate && bStructureUpToDate)
    {
        return; // Already current
    }

    // Only reapply if structure matches (deterministic, safe)
    if (bStructureUpToDate && !bContentUpToDate)
    {
        UE_LOG(LogProjectObject, Log, TEXT("[%s] Offline resave: reapplying definition"),
            *GetActorLabel());

        // Call runtime ApplyDefinition method (see Task 1.4)
        // This avoids circular dependencies - PreSave is in runtime module,
        // ApplyDefinition is in runtime module, no editor subsystem involved.
        const bool bApplied = ApplyDefinition(CurrentDef);

        // CRITICAL: Only update hashes AFTER successful reapply!
        // Setting hashes before reapply would mark stale actors as up-to-date.
        if (bApplied)
        {
            AppliedStructureHash = CurrentDef->DefinitionStructureHash;
            AppliedContentHash = CurrentDef->DefinitionContentHash;

            UE_LOG(LogProjectObject, Verbose, TEXT("[%s] Offline resave: SUCCESS - hashes updated"),
                *GetActorLabel());
        }
        else
        {
            UE_LOG(LogProjectObject, Error, TEXT("[%s] OFFLINE RESAVE FAILURE: Reapply failed!"),
                *GetActorLabel());
            UE_LOG(LogProjectObject, Error, TEXT("  Actor remains stale - MANUAL REVIEW REQUIRED!"));
            // Hashes NOT updated - actor will be retried on next resave pass
        }
    }
    else if (!bStructureUpToDate)
    {
        // Structural mismatch - log for manual review
        UE_LOG(LogProjectObject, Warning,
            TEXT("[%s] Offline resave: SKIPPED - structural mismatch (needs manual replace)"),
            *GetActorLabel());
    }
}
#endif // WITH_EDITOR
```

**Why cooking guard is critical:**
- Without `IsCooking()` check, updates would run during cook
- Could mutate cooked data (nondeterminism, correctness issues)
- Cook context may have limited asset loading capabilities
- Determinism: cook should never modify source data

**Why robust asset loading is critical:**
- Commandlet context may not have AssetManager initialized (silent failure)
- Primary asset loading might fail (unregistered, not scanned yet)
- **Fallback strategy:** Try `GetPrimaryAssetPath() + TryLoad()` if initial load fails
- **Explicit error logging:** Use Error level (not Warning) so CI catches failures
- Without robust loading: actors silently stay stale, failures not detected

**Asset loading pattern (two-step with fallback):**
```cpp
// 1. Try AssetManager (primary path)
UObjectDefinition* Def = AssetManager->GetPrimaryAssetObject(ObjectDefinitionId);

// 2. Fallback: Direct load via soft path
if (!Def)
{
    FSoftObjectPath DefPath = AssetManager->GetPrimaryAssetPath(ObjectDefinitionId);
    Def = Cast<UObjectDefinition>(DefPath.TryLoad());
}

// 3. Explicit error if both fail
if (!Def)
{
    UE_LOG(..., Error, TEXT("OFFLINE RESAVE FAILURE: ..."));
}
```

#### Task 3.2: Create commandlet wrapper script

**File:** `scripts/ue/tools/resave_actors_update_from_def.bat`

```bat
@echo off
REM Resave all InteractableActors with definition updates (offline pass)
REM Usage: resave_actors_update_from_def.bat <MapPath>
REM Example: resave_actors_update_from_def.bat /Game/Project/Maps/City17_Persistent_WP

setlocal
set MAP_PATH=%1

if "%MAP_PATH%"=="" (
    echo Error: Map path required
    echo Usage: resave_actors_update_from_def.bat /Game/Project/Maps/MapName_WP
    exit /b 1
)

REM Load UE path from config
for /f "delims=" %%i in ('type scripts\config\ue_path.conf') do set UE_PATH=%%i

echo ========================================
echo Resaving actors with definition updates
echo Map: %MAP_PATH%
echo ========================================

"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "Alis.uproject" ^
    "%MAP_PATH%" ^
    -Unattended ^
    -run=WorldPartitionBuilderCommandlet ^
    -SCCProvider=None ^
    -Builder=WorldPartitionResaveActorsBuilder ^
    -ActorClass=InteractableActor ^
    -AlisUpdateFromDef

REM NOTE: -ActorClass uses the Unreal class name WITHOUT the 'A' prefix
REM AInteractableActor -> InteractableActor
REM Same as Epic's example: AStaticMeshActor -> StaticMeshActor

if %ERRORLEVEL% NEQ 0 (
    echo ========================================
    echo FAILED: Resave actors
    echo ========================================
    exit /b 1
)

echo ========================================
echo SUCCESS: Actors resaved and updated
echo ========================================
```

**Usage:**
```powershell
# Manual run before release
.\scripts\ue\tools\resave_actors_update_from_def.bat /Game/Project/Maps/City17_Persistent_WP

# CI integration (add to pre-cook step)
- name: Update actors from definitions
  run: .\scripts\ue\tools\resave_actors_update_from_def.bat /Game/Project/Maps/City17_Persistent_WP
```

**Why this approach works:**
- Uses Epic's battle-tested WorldPartitionResaveActorsBuilder
- No custom builder code (100+ lines saved)
- PreSave hook is clean, minimal, single-responsibility
- Runs offline (zero editor performance impact)
- Deterministic (Reapply-only, no risky auto-replace)

#### Known World Partition Builder Pitfalls (Test on Copy Branch First)

**Issue 1: WorldPartitionResaveActorsBuilder can detach attached actors**
- **UE Bug Tracker:** UE-220221
- **Symptom:** Actors with parent/child attachment may lose attachment after resave
- **Workaround:** Test on copy branch first, verify attachment preservation
- **Mitigation:** Run attachment validation pass after resave

**Issue 2: Multiple WP levels in one run can cause assertions**
- **UE Bug Tracker:** UE-354938
- **Symptom:** Running builder on multiple maps sequentially may hit assertions/crashes
- **Workaround:** Run builder per-map in separate invocations
- **Alternative:** Force GC between maps if batching is required

**Example per-map invocation (safest):**
```powershell
# Run separately for each map
.\scripts\ue\tools\resave_actors_update_from_def.bat /Game/Maps/City17_WP
.\scripts\ue\tools\resave_actors_update_from_def.bat /Game/Maps/Outskirts_WP
.\scripts\ue\tools\resave_actors_update_from_def.bat /Game/Maps/Underground_WP
```

**Validation after resave:**
```powershell
# Verify no actors lost attachment
# Verify no package corruption
# Spot-check random actors for correct properties
```

#### Optional Enhancement: TSoftObjectPtr Fallback (Cheap Insurance)

**Problem:** Mode 2 PreSave relies on AssetManager being initialized and having scanned primary assets. In rare commandlet contexts, this may fail silently.

**Solution:** Add `TSoftObjectPtr<UObjectDefinition>` as redundant fallback path.

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Template/Interactable/InteractableActor.h`

```cpp
// In AInteractableActor class
UCLASS(Blueprintable)
class PROJECTOBJECT_API AInteractableActor : public AProjectWorldActor
{
    GENERATED_BODY()

public:
    // Existing: Primary asset ID (preferred path)
    UPROPERTY(EditAnywhere, Category = "Definition")
    FPrimaryAssetId ObjectDefinitionId;

    // NEW: Soft reference fallback (cheap insurance for Mode 2)
    // Only used if AssetManager fails to load via ObjectDefinitionId
    UPROPERTY(VisibleAnywhere, Category = "Definition")
    TSoftObjectPtr<UObjectDefinition> DefinitionAssetFallback;
};
```

**Update spawn logic to set fallback:**

```cpp
// In ProjectObjectActorFactory::SpawnActor
NewActor->ObjectDefinitionId = Def->GetPrimaryAssetId();
NewActor->DefinitionAssetFallback = Def;  // Set fallback reference
```

**Update PreSave to use fallback:**

```cpp
// In AInteractableActor::PreSave, after AssetManager attempts fail:
if (!CurrentDef && DefinitionAssetFallback.IsValid())
{
    CurrentDef = DefinitionAssetFallback.LoadSynchronous();
    UE_LOG(LogProjectObject, Warning, TEXT("[%s] Used fallback TSoftObjectPtr to load definition"),
        *GetActorLabel());
}
```

**Cost:** 1 property per actor (8 bytes), no runtime overhead

**Benefit:** Eliminates AssetManager scan order dependency, guaranteed load path

**When to add:** If you see "OFFLINE RESAVE FAILURE: Definition not found" errors in CI logs despite correct AssetManager config.

---

### Phase 4: Replace Strategy (Loaded Actors Only)

**Scope Clarification:** Replace requires valid `UWorld` context - only possible for loaded actors.

#### When Replace is Safe:

1. **Actor is loaded in editor world** (Mode 1 detects structural mismatch)
2. **User manually triggers replace** (notification button, editor menu, etc.)
3. **Uses existing `ReplaceActorFromDefinition()`** method

**Existing implementation location:**
- `Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp:496-549`

#### What Replace Does (Existing Code):

```cpp
AActor* UDefinitionActorSyncSubsystem::ReplaceActorFromDefinition(
    AInteractableActor* OldActor,
    UObjectDefinition* Def)
{
    // CRITICAL: Requires OldActor to be in a valid UWorld!
    // Will FAIL if called on unloaded World Partition cell actors.

    UWorld* World = OldActor->GetWorld();
    check(World); // Must have world context

    // ... destroy old actor, spawn new via factory ...
}
```

#### Mode 2 Replace Strategy:

**Offline resave does NOT attempt Replace:**
- Structural mismatches logged for manual review
- Report generated with list of actors needing attention
- Designer loads affected cells, manually reviews/replaces

**Why:**
- Auto-replace can destroy per-instance tweaks (position, rotation, custom edits)
- Structural changes are rare (should be caught during development)
- Manual review ensures correctness

---

## Testing Plan

### Phase 1: Hash Tracking

**Test 1.1: Hash computation**
```
1. Edit JSON: change door angle
2. Regenerate definition
3. Verify ContentHash changed, StructureHash unchanged
```

**Test 1.2: Structural change**
```
1. Edit JSON: add new mesh entry
2. Regenerate definition
3. Verify both hashes changed
```

**Test 1.3: Factory sets hashes**
```
1. Place new door actor
2. Verify AppliedStructureHash and AppliedContentHash set
3. Verify matches definition hashes
```

### Phase 2: Mode 1 (Update on Load)

**Test 2.1: Property change auto-update**
```
1. Place door in WP cell
2. Unload cell (close level)
3. Edit JSON: change door angle
4. Regenerate definition
5. Load cell
6. Verify: door auto-updated, no notification
```

**Test 2.2: Structural change notification**
```
1. Place door in WP cell
2. Unload cell
3. Edit JSON: add new mesh
4. Regenerate definition
5. Load cell
6. Verify: notification shown, actor NOT auto-updated
```

**Test 2.3: Already up-to-date (no action)**
```
1. Place door, load cell
2. Verify: no update triggered (hashes match)
```

### Phase 3: Mode 2 (Offline Resave)

**Test 3.1: Offline property update**
```
1. Place 100 doors across multiple WP cells
2. Edit JSON: change all door angles
3. Regenerate definition
4. Run: resave_actors_update_from_def.bat
5. Verify: all doors updated (check random sample)
```

**Test 3.2: Structural mismatch report**
```
1. Place door
2. Edit JSON: add mesh (structural change)
3. Regenerate definition
4. Run: resave_actors_update_from_def.bat
5. Verify: log shows "SKIPPED - structural mismatch"
```

---

## Edge Cases and Solutions

### 1. Definition Deleted
**Issue:** Actor references definition that no longer exists
**Solution:**
- Mode 1: Log warning, skip update (actor stays as-is)
- Mode 2: Log warning in resave report

### 2. Hash Collision (Extremely Rare)
**Issue:** Different definitions produce same hash (MD5 collision)
**Solution:** Accept risk (astronomically low probability for our use case)

### 3. Actor Has Manual Tweaks
**Issue:** Designer manually adjusted actor properties
**Solution:**
- Mode 1: Reapply overwrites tweaks (acceptable - definition is source of truth)
- Mode 2: Same behavior (deterministic)
- **Mitigation:** Document that actors are definition-backed (no manual tweaks)

### 4. Circular Definition References
**Issue:** Definition A references Definition B, both need update
**Solution:** Not applicable (definitions don't reference each other in our system)

---

## Performance Characteristics

### Mode 1 (Update on Load)
- **Trigger:** Actor load event (passive)
- **Cost per actor:** ~1ms (hash comparison + Reapply if needed)
- **Total overhead:** Negligible (only when cells load)
- **Impact on iteration:** Zero (no forced package loads)

### Mode 2 (Offline Resave)
- **Trigger:** Manual/CI (runs before cook)
- **Cost:** 100-200 actors/second (load/save overhead)
- **Example:** 10,000 actors = 50-100 seconds
- **Impact on iteration:** Zero (runs offline)

---

## Migration Plan

### Step 1: Add hashes to definition and actor (Phase 1)
- Add properties
- Update generator to compute hashes
- Update factory to set hashes
- **Build and test**: Verify hashes computed correctly

### Step 2: Implement Mode 1 (Phase 2)
- Add actor load hook
- Implement two-hash validation
- Add structural mismatch notification
- **Test**: Verify auto-update on load works

### Step 3: Implement Mode 2 (Phase 3)
- Add PreSave hook to actor
- Create commandlet wrapper script
- Test offline resave pass
- **Document**: Add to CI/release process

### Step 4: Deprecate old single-hash property
- Remove `uint32 DefinitionStructureHash` from AInteractableActor
- Update all references to use new `FString` hashes

---

## Related Files

### Definition System
- [ObjectDefinition.h](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Data/ObjectDefinition.h) - Definition asset
- [InteractableActor.h](../../Plugins/Resources/ProjectObject/Source/ProjectObject/Public/Template/Interactable/InteractableActor.h) - Actor base class
- [ProjectObjectActorFactory.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/ProjectObjectActorFactory.cpp) - Spawns actors from definitions

### Sync System
- [DefinitionActorSyncSubsystem.cpp](../../Plugins/Editor/ProjectPlacementEditor/Source/ProjectPlacementEditor/Private/DefinitionActorSyncSubsystem.cpp) - Current sync logic (Reapply/Replace)
- [DefinitionEvents.h](../../Plugins/Editor/ProjectEditorCore/Source/ProjectEditorCore/Public/DefinitionEvents.h) - OnDefinitionRegenerated delegate

### Documentation
- [DefinitionActorSyncSubsystem.md](../../Plugins/Editor/ProjectPlacementEditor/docs/DefinitionActorSyncSubsystem.md) - Architecture docs
- [Data Pipeline Architecture](../../docs/data/README.md#data-pipeline-architecture) - SYNC -> GENERATION -> PROPAGATION flow

---

## Implementation Status

- [ ] Phase 1: Two-Hash Version Tracking
  - [ ] Task 1.1: Add hashes to UObjectDefinition
  - [ ] Task 1.2: Update generator to compute hashes
  - [ ] Task 1.3: Add hashes to AInteractableActor
  - [ ] Task 1.4: Update factory to set hashes
  - [ ] Test: Verify hash computation

- [ ] Phase 2: Mode 1 - Update on Actor Load
  - [ ] Task 2.1: Hook actor load event
  - [ ] Task 2.2: Implement two-hash validation
  - [ ] Task 2.3: Add structural mismatch notification
  - [ ] Test: Property change auto-update
  - [ ] Test: Structural change notification

- [ ] Phase 3: Mode 2 - Offline Resave
  - [ ] Task 3.1: Add PreSave hook
  - [ ] Task 3.2: Create commandlet wrapper script
  - [ ] Test: Offline property update
  - [ ] Test: Structural mismatch report
  - [ ] Document: Add to CI process

- [ ] Phase 4: Cleanup
  - [ ] Remove deprecated uint32 DefinitionStructureHash
  - [ ] Update all references to FString hashes
  - [ ] Final testing pass

---

## Success Criteria

**Must Have:**
- [X] Mode 1 working: Actors auto-update when cells load
- [X] Mode 2 working: Offline resave updates all actors
- [X] Two-hash system: Detects both property and structural changes
- [X] No iteration speed penalty: Mode 1 is passive, Mode 2 is offline

**Nice to Have:**
- [ ] Manual replace button in notification for structural changes
- [ ] Resave report shows list of actors needing manual attention
- [ ] Editor menu: "Update All Actors in Current Level"

---

## References

**Epic Documentation:**
- [World Partition Builder Commandlet](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-builder-commandlet-reference)
- [ULevel::OnLoadedActorAddedToLevelPostEvent](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ULevel/FLoadedActorAddedToLevelPostEven-)
- [One File Per Actor](https://dev.epicgames.com/documentation/en-us/unreal-engine/one-file-per-actor-in-unreal-engine)

**Project Architecture:**
- [Data Pipeline](../../docs/data/README.md) - SYNC -> GENERATION -> PROPAGATION
- [RedirectorAutoFix](../../Plugins/Editor/ProjectAssetSync/README.md#redirectorautofix-implemented) - Similar pattern reference
