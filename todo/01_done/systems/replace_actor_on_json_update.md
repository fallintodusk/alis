# Auto-Update Actors on Definition Change

## Goal
JSON change -> fully working pipeline in editor and game without manual intervention.

## Problem
When JSON definition changes, placed actors in the level keep old property values.
User must manually delete and re-place actors to see changes.

---

## Key Insight: Reapply vs Replace

**Full Replace (destroy + respawn) breaks:**
- Actor references from other actors
- Sequencer bindings
- Attachments, per-instance tweaks
- Selection state, folder assignments
- World partition bookkeeping

**Solution: Two-tier approach**
1. **Reapply (default)** - Update component properties in-place
2. **Replace (fallback)** - Only when structure changes (meshes/components added/removed)

---

## UE 5.7 Hooks (Research)

- `FCoreUObjectDelegates::OnObjectPropertyChanged` - listen to asset changes
- `AActor::RerunConstructionScripts()` - force reconstruction if needed
- `FScopedTransaction` - undo support for batch operations

---

## Simplified Architecture (KISS)

### Core Functions (no interfaces needed for Phase 1)

```cpp
// Find actors by definition ID
TArray<AActor*> FindActorsByDefinitionId(UWorld* World, FPrimaryAssetId DefId);

// In-place update (default path)
void ApplyDefinitionToActor(AActor* Actor, UObjectDefinition* Def);

// Full replace (fallback for structural changes)
AActor* ReplaceActorFromDefinition(AActor* OldActor, UObjectDefinition* Def, UActorFactory* Factory);

// Detect if structure changed (needs replace vs reapply)
bool IsStructuralChange(AActor* Actor, UObjectDefinition* NewDef);
```

---

## Data Flow

```
[JSON File Changed]
        |
        v
[Generator regenerates UObjectDefinition]
        |
        v
[OnDefinitionRegenerated(Def)]
        |
        v
[FindActorsByDefinitionId(DefId)]
        |
        v
[For each actor:]
        |
        +--[IsStructuralChange?]--NO--> [ApplyDefinitionToActor()] --> Done
        |
        +--YES--> [ReplaceActorFromDefinition()] --> Done
```

---

## Implementation Plan

### Phase 1: Store Definition Reference on Actors (Metadata Only)

**SOLID: Actor is pure runtime container. DefinitionActorSyncSubsystem owns all definition logic.**

**AInteractableActor.h:**
```cpp
// Metadata only - subsystem uses this to find actors
UPROPERTY(VisibleAnywhere, Category = "Definition")
FPrimaryAssetId ObjectDefinitionId;

// Set by factory, reserved for future optimization
UPROPERTY()
uint32 DefinitionStructureHash = 0;

// NO definition methods on actor - subsystem owns all update logic
```

**ProjectObjectActorFactory::SpawnActor() - store mesh ID on components:**
```cpp
Actor->ObjectDefinitionId = Def->GetPrimaryAssetId();
Actor->DefinitionStructureHash = ComputeStructureHash(Def);

// CRITICAL: Tag mesh components with their definition mesh ID for reapply matching
for (const FObjectMeshEntry& Entry : Def->Meshes)
{
    UStaticMeshComponent* MeshComp = /* created mesh */;
    MeshComp->ComponentTags.Add(FName(*FString::Printf(TEXT("DefMeshId=%s"), *Entry.Id.ToString())));
}
```

> **Why tags?** Matching by asset path fails when: (1) two meshes use same asset, (2) mesh asset changes in JSON. Tags make "Mesh asset path change -> Reapply" actually work.

### Phase 2: Structure Hash (for change detection)

```cpp
// Hash only structural elements, not property values
// SORT before hashing so reordering doesn't trigger unnecessary replace
uint32 ComputeStructureHash(UObjectDefinition* Def)
{
    uint32 Hash = 0;

    // Include spawn class if definition specifies one (future-proof)
    // If definitions can change actor class, this triggers replace
    if (Def->SpawnClass)
    {
        Hash = HashCombine(Hash, GetTypeHash(Def->SpawnClass->GetFName()));
    }

    // Collect and sort mesh IDs (order doesn't matter for structure)
    TArray<FName> MeshIds;
    for (const FObjectMeshEntry& Mesh : Def->Meshes)
    {
        MeshIds.Add(Mesh.Id);
    }
    MeshIds.Sort(FNameLexicalLess());
    for (const FName& Id : MeshIds)
    {
        Hash = HashCombine(Hash, GetTypeHash(Id));
    }

    // Collect and sort capability signatures (type + sorted scopes)
    TArray<FString> CapSignatures;
    for (const FObjectCapabilityEntry& Cap : Def->Capabilities)
    {
        TArray<FName> SortedScopes = Cap.Scope;
        SortedScopes.Sort(FNameLexicalLess());
        FString Sig = Cap.Type.ToString();
        for (const FName& S : SortedScopes)
        {
            Sig += TEXT(":") + S.ToString();
        }
        CapSignatures.Add(Sig);
    }
    CapSignatures.Sort();
    for (const FString& Sig : CapSignatures)
    {
        Hash = HashCombine(Hash, GetTypeHash(Sig));
    }

    return Hash;
}
```

> **Why sort?** If order is not meaningful (mesh order, capability order), sorting prevents unnecessary full-replace when JSON entries are just reordered.
>
> **Why include spawn class?** If definitions later support custom actor classes, changing it must trigger replace (can't reclass an actor in-place).

### Phase 3: In-Place Reapply (the main path)

**SOLID: Subsystem owns all definition logic. Actor is just a container.**

**DefinitionActorSyncSubsystem::ReapplyDefinition():**
```cpp
void UDefinitionActorSyncSubsystem::ReapplyDefinition(AInteractableActor* Actor, UObjectDefinition* Def)
{
    if (!Actor || !Def) return;

    Actor->Modify(); // For undo

    // Build mesh map by finding components via their DefMeshId tag (NOT asset path!)
    TMap<FName, UStaticMeshComponent*> MeshMap;
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp))
        {
            for (const FName& Tag : Mesh->ComponentTags)
            {
                FString TagStr = Tag.ToString();
                if (TagStr.StartsWith(TEXT("DefMeshId=")))
                {
                    FName MeshId = FName(*TagStr.Mid(10));
                    MeshMap.Add(MeshId, Mesh);
                    break;
                }
            }
        }
    }

    // Update mesh assets AND transforms
    for (const FObjectMeshEntry& Entry : Def->Meshes)
    {
        if (UStaticMeshComponent** Found = MeshMap.Find(Entry.Id))
        {
            UStaticMeshComponent* Mesh = *Found;
            Mesh->Modify();

            UStaticMesh* NewMesh = Entry.Asset.LoadSynchronous();
            if (NewMesh && Mesh->GetStaticMesh() != NewMesh)
            {
                Mesh->SetStaticMesh(NewMesh);
            }
            Mesh->SetRelativeTransform(Entry.Transform.ToTransform());
        }
    }

    // Update capability component properties
    for (const FObjectCapabilityEntry& CapEntry : Def->Capabilities)
    {
        UClass* CapClass = FCapabilityRegistry::GetCapabilityClass(CapEntry.Type);
        if (!CapClass) continue;

        UActorComponent* Comp = Actor->FindComponentByClass(CapClass);
        if (!Comp) continue;

        Comp->Modify();

        // Update TargetMesh reference if scoped (0 or 1 non-actor scope)
        FName NonActorScopeId = NAME_None;
        for (const FName& ScopeId : CapEntry.Scope)
        {
            if (ScopeId != NAME_CapabilityScope_Actor)
            {
                NonActorScopeId = ScopeId;
                break; // Use first non-actor scope
            }
        }

        if (NonActorScopeId != NAME_None)
        {
            if (UStaticMeshComponent** Found = MeshMap.Find(NonActorScopeId))
            {
                SetPropertyByName(Comp, TEXT("TargetMesh"), *Found);
            }
        }

        // Apply property values
        for (const auto& Prop : CapEntry.Properties)
        {
            SetPropertyByName(Comp, Prop.Key, Prop.Value);
        }
    }

    Actor->RefreshInteractableComponents();
    Actor->MarkPackageDirty();
}
```

> **SOLID design:**
> - Actor is pure runtime container - holds components, dispatches interaction
> - Subsystem owns ALL definition logic: CanReapplyDefinition + ReapplyDefinition + ReplaceActorFromDefinition
> - Actor keeps metadata (ObjectDefinitionId) for subsystem to find matching actors

### Phase 4: Subsystem Integration

**SOLID: DefinitionActorSyncSubsystem subscribes to definition events and handles all updates.**

**DefinitionActorSyncSubsystem.cpp:**
```cpp
void UDefinitionActorSyncSubsystem::ExecutePendingActorUpdate()
{
    // ... find actors by ObjectDefinitionId ...

    FScopedTransaction Transaction(FText::Format(
        NSLOCTEXT("DefinitionActorSync", "UpdateActors", "Update {0} actors from definition"),
        FText::AsNumber(AffectedActors.Num())));

    int32 ReappliedCount = 0;
    int32 ReplacedCount = 0;

    for (AInteractableActor* Actor : AffectedActors)
    {
        if (CanReapplyDefinition(Actor, Def))
        {
            // Structure matches - safe to update properties in place
            ReapplyDefinition(Actor, Def);
            ReappliedCount++;
        }
        else
        {
            // Structure changed - need full replace
            ReplaceActorFromDefinition(Actor, Def);
            ReplacedCount++;
        }
    }
}

// CanReapplyDefinition: compares actor mesh IDs + capability types with definition
// If structure matches, Reapply is safe. Otherwise, Replace.
bool UDefinitionActorSyncSubsystem::CanReapplyDefinition(AInteractableActor* Actor, UObjectDefinition* Def) const
{
    // Compare mesh IDs (via DefMeshId tags) and capability types
    // Returns true if structure matches, false if needs replace
}
```

> **Key points:**
> - Subsystem subscribes to `FDefinitionEvents::OnDefinitionRegenerated()`
> - Shows 5-second countdown notification before updating (Apply Now / Cancel)
> - CanReapplyDefinition does on-the-fly structure comparison (not hash-based)
> - All three methods (CanReapply, Reapply, Replace) are on subsystem

### Phase 5: Full Replace (fallback)

```cpp
AActor* ReplaceActorFromDefinition(AActor* OldActor, UObjectDefinition* Def)
{
    // Cache factory once (avoid repeated scan)
    static TWeakObjectPtr<UProjectObjectActorFactory> CachedFactory;
    UProjectObjectActorFactory* Factory = CachedFactory.Get();
    if (!Factory)
    {
        for (UActorFactory* F : GEditor->ActorFactories)
        {
            Factory = Cast<UProjectObjectActorFactory>(F);
            if (Factory)
            {
                CachedFactory = Factory;
                break;
            }
        }
    }
    if (!Factory) return nullptr;

    // Capture state BEFORE deletion
    FTransform OldTransform = OldActor->GetActorTransform();
    ULevel* Level = OldActor->GetLevel();
    FString OldLabel = OldActor->GetActorLabel();
    FName OldFolderPath = OldActor->GetFolderPath(); // Use FName, not FFolder (cross-version safe)

    // Use EDITOR-AWARE deletion via UEditorActorSubsystem
    // This is already inside FScopedTransaction from caller
    OldActor->Modify();
    UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    bool bDeleted = ActorSubsystem ? ActorSubsystem->DestroyActor(OldActor) : false;

    if (!bDeleted)
    {
        // Fallback to raw destroy if editor delete fails
        OldActor->Destroy();
    }

    // Spawn new via factory
    FActorSpawnParameters Params;
    AActor* NewActor = Factory->SpawnActor(Def, Level, OldTransform, Params);

    if (NewActor)
    {
        NewActor->SetActorLabel(OldLabel);
        NewActor->SetFolderPath(OldFolderPath); // Restore folder
    }

    return NewActor;
}
```

> **Why editor-aware delete?** Raw `Destroy()` can cause issues with undo/redo and editor bookkeeping. `UEditorActorSubsystem::DestroyActor()` handles world partition, undo, and other editor state correctly.

### Phase 6: Optional - Force Viewport Update

If property changes don't visually update in viewport (e.g., motion preview), try:

```cpp
// After applying all properties to a component:
Comp->ReregisterComponent(); // Forces render/scene update

// OR add a component method for complex cases:
if (IDefinitionUpdatable* Updatable = Cast<IDefinitionUpdatable>(Comp))
{
    Updatable->OnDefinitionReapplied();
}
```

> Try minimal path first (just set properties). Only add ReregisterComponent if visuals don't update.

---

## What Changes Trigger What

| Change Type | Action | Preserves |
|-------------|--------|-----------|
| Property value (OpenPosition, Stiffness...) | Reapply | Everything |
| Mesh transform | Reapply | Everything |
| Mesh asset path | Reapply | Everything |
| New mesh added | Replace | Transform, label |
| Mesh removed | Replace | Transform, label |
| New capability added | Replace | Transform, label |
| Capability removed | Replace | Transform, label |

---

## Testing Checklist

- [ ] Place wardrobe, change JSON OpenPosition, verify in-place reapply
- [ ] Change mesh transform in JSON, verify mesh moves without actor replace
- [ ] Add new capability to JSON, verify full replace happens
- [ ] Undo after update, verify actors restored
- [ ] Multiple actors of same type, verify all updated
- [ ] Item pickup actors (ItemDefinition changes)

---

## Future Extensions (Phase 2+)

1. **Notification UI** - countdown + cancel for batch operations
2. **Diff preview** - show what will change before applying
3. **Cross-level** - update actors in all loaded sublevels
4. **Selective update** - let user pick which actors to update
5. **Generic matcher/replacer interfaces** - when needed for other use cases

---

## Observable Logging (Debug Chain)

### Log Category
```cpp
DEFINE_LOG_CATEGORY_STATIC(LogDefinitionUpdate, Log, All);
```

### Full Cycle Logging

```cpp
// 1. JSON Change Detection
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] JSON changed: %s"),
    *FDateTime::Now().ToString(), *JsonPath);

// 2. Definition Regeneration
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Regenerating definition: %s (hash: %u -> %u)"),
    *FDateTime::Now().ToString(), *DefId.ToString(), OldHash, NewHash);

// 3. Actor Search
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Searching for actors with DefId: %s"),
    *FDateTime::Now().ToString(), *DefId.ToString());

// 4. Found Actors
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Found %d actors to update:"),
    *FDateTime::Now().ToString(), AffectedActors.Num());
for (AActor* Actor : AffectedActors)
{
    UE_LOG(LogDefinitionUpdate, Log, TEXT("  - %s (hash: %u, needs replace: %s)"),
        *Actor->GetName(),
        Actor->DefinitionStructureHash,
        Actor->DefinitionStructureHash != NewHash ? TEXT("YES") : TEXT("NO"));
}

// 5. Reapply Start
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Reapplying to actor: %s"),
    *FDateTime::Now().ToString(), *Actor->GetName());

// 6. Property Update
UE_LOG(LogDefinitionUpdate, Verbose, TEXT("[%s]   Setting %s.%s = %s"),
    *FDateTime::Now().ToString(), *CompName, *PropName, *Value);

// 7. Reapply Complete
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Reapply complete for: %s (%.2fms)"),
    *FDateTime::Now().ToString(), *Actor->GetName(), ElapsedMs);

// 8. Replace Start (if structural change)
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Replacing actor: %s (structural change)"),
    *FDateTime::Now().ToString(), *Actor->GetName());

// 9. Replace Complete
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] Replace complete: %s -> %s"),
    *FDateTime::Now().ToString(), *OldName, *NewActor->GetName());

// 10. Cycle Complete
UE_LOG(LogDefinitionUpdate, Log, TEXT("[%s] === Update cycle complete ==="),
    *FDateTime::Now().ToString());
UE_LOG(LogDefinitionUpdate, Log, TEXT("  Definition: %s"), *DefId.ToString());
UE_LOG(LogDefinitionUpdate, Log, TEXT("  Reapplied: %d actors"), ReappliedCount);
UE_LOG(LogDefinitionUpdate, Log, TEXT("  Replaced: %d actors"), ReplacedCount);
UE_LOG(LogDefinitionUpdate, Log, TEXT("  Total time: %.2fms"), TotalElapsedMs);
```

### Example Log Output

```
[2026.01.13-03:45:12] JSON changed: .../WardrobeSlider2.json
[2026.01.13-03:45:12] Regenerating definition: Object:WardrobeSlider2 (hash: 0 -> 847291)
[2026.01.13-03:45:12] Searching for actors with DefId: Object:WardrobeSlider2
[2026.01.13-03:45:12] Found 3 actors to update:
  - WardrobeSlider2_0 (hash: 847291, needs replace: NO)
  - WardrobeSlider2_1 (hash: 847291, needs replace: NO)
  - WardrobeSlider2_2 (hash: 847291, needs replace: NO)
[2026.01.13-03:45:12] Reapplying to actor: WardrobeSlider2_0
[2026.01.13-03:45:12]   Setting SpringSliderComponent_0.ClosedPosition = (0,0,0)
[2026.01.13-03:45:12]   Setting SpringSliderComponent_0.OpenPosition = (-114,0,0)
[2026.01.13-03:45:12] Reapply complete for: WardrobeSlider2_0 (0.45ms)
[2026.01.13-03:45:12] Reapplying to actor: WardrobeSlider2_1
...
[2026.01.13-03:45:13] === Update cycle complete ===
  Definition: Object:WardrobeSlider2
  Reapplied: 3 actors
  Replaced: 0 actors
  Total time: 2.34ms
```

### Console Commands (for debugging)

```cpp
// Force update all actors of a definition
UFUNCTION(Exec)
static void ForceUpdateDefinition(FString DefIdString);

// List all actors with definition IDs
UFUNCTION(Exec)
static void ListDefinitionActors();

// Show definition structure hash
UFUNCTION(Exec)
static void ShowDefinitionHash(FString DefIdString);
```

### Visual Debug (optional)

```cpp
#if WITH_EDITOR
// Draw debug info on updated actors for 5 seconds
if (GEngine && bShowDebugInfo)
{
    DrawDebugString(World, Actor->GetActorLocation() + FVector(0, 0, 100),
        FString::Printf(TEXT("Updated: %s"), *DefId.ToString()),
        nullptr, FColor::Green, 5.0f);
}
#endif
```

---

## Validated Implementation Rules (Summary)

### SOLID Architecture
- **Actor is pure runtime container** - holds components, dispatches interaction, stores metadata only
- **DefinitionActorSyncSubsystem owns ALL definition logic:**
  - `CanReapplyDefinition()` - checks if structure matches
  - `ReapplyDefinition()` - in-place property update
  - `ReplaceActorFromDefinition()` - full replace fallback

### Implementation Rules
1. **Store MeshEntry.Id on spawned mesh components** via ComponentTags (`DefMeshId=<id>`)
2. **Reapply does:**
   - Find mesh comp by DefMeshId tag (NOT asset path)
   - Call `Comp->Modify()` before changes (undo granularity)
   - Set mesh asset + relative transform
   - Apply capability properties via SetPropertyByName
   - Call `Actor->RefreshInteractableComponents()`
3. **CanReapplyDefinition** compares mesh IDs + capability types on-the-fly (not hash-based)
4. **One component per capability type** - `FindComponentByClass()` returns first match
5. **Editor transaction** wraps the batch update
6. **Replace uses `UEditorActorSubsystem::DestroyActor()`** (not raw Destroy)
7. **TargetMesh is strict contract** - all scoped capabilities must have this property
8. **Scope rule:** 0 or 1 non-actor scope ID per capability
9. **Cache factory pointer** - don't scan `GEditor->ActorFactories` on every replace

---

## Optional Hardening (not required for Phase 1)

- Log + warn when a mesh component is missing `DefMeshId` tag
- Warn on duplicate mesh IDs on same actor
- Add `ReregisterComponent()` only if visuals don't update after property changes
- Consider `IDefinitionUpdatable` interface for components needing custom refresh hooks

---

## Priority: HIGH
## Complexity: Low-Medium (1-2 days for core, +1 day for polish)
## Dependencies: None
