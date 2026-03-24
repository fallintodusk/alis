# Grandpa Behind Door - Scenario Implementation Plan

## Context

Player finds a locked door. Grandpa behind door hears knocking, asks for water through dialogue. If player has water, they can give it -- water consumed, door unlocks and opens. After that, door works as a normal hinged door.

**Goal:** Max decoupling, SOLID, KISS. Door is a blackbox. Grandpa is the brain.

---

## Architecture

```
Door Actor (InteractableActor) -- just a door
  [100] Lockable  -- blocks when locked, fires OnAccessDenied
  [  0] Hinged    -- door rotation

Grandpa Actor (placed near door)
  DialogueComponent       -- DLG_GrandPa_Door tree
  ActorWatcherComponent    -- binds to door's Lockable.OnAccessDenied

Flow:
  1. Player presses E on locked door
  2. Lockable denies -> broadcasts OnAccessDenied(LockTag, Instigator)
     (delegate ALREADY EXISTS in LockableComponent)
  3. ActorWatcher receives event -> tells DialogueComponent to start conversation
     DialogueComponent gets ActionTarget = door actor (for lock/motion actions)
  4. Dialogue tree: grandpa asks for water
  5. Player gives water -> actions fire:
     inventory.consume:WaterBottle -> InventoryComponent on player
     lock.unlock                   -> LockableComponent on door
     motion.open                   -> SpringRotatorComponent on door
  6. Next interact: door is unlocked -> Lockable passes -> Hinged runs -> normal door
```

**Key discovery:** `OnAccessDenied` delegate already exists on LockableComponent (broadcast when locked + interaction attempted). No new event needed. ActorWatcher subscribes via `AddDynamic` -- first C++ consumer of this delegate, but the infrastructure (`BlueprintAssignable` delegates on capability components) was designed for exactly this. Not reinventing -- activating existing hooks.

**Inventory condition approach:** Dialogue checks player inventory via `IInventoryReadOnly` interface (lives in ProjectCore). This queries actual inventory state directly -- no intermediate tags needed. Better than tagging the player on pickup because:
- Source of truth is inventory itself (item could be consumed/dropped)
- No tag lifecycle to manage
- IInventoryReadOnly already in ProjectCore, no new dependency for ProjectDialogue

**Chaining still needed:** Lockable + Hinged are both on "door" mesh. Current dispatch picks only the best (Lockable). After unlock, Lockable returns true but Hinged never runs. Step 1 fixes this for all doors.

---

## Steps

- [x] Step 1: Enable interaction chaining in InteractableActor.cpp (Lockable+Hinged)
- [x] Step 2: DialogueComponent - add ActionTarget for cross-actor action dispatch
- [x] Step 3: LockableComponent - add IProjectActionReceiver for lock.unlock
- [x] Step 4: InventoryComponent - add IProjectActionReceiver for inventory.consume
- [x] Step 5: SpringMotionComponentBase - add IProjectActionReceiver for motion.open (base class, benefits all motion components)
- [x] Step 6: ActorWatcherComponent - binds to door's OnAccessDenied, relays events
- [x] Step 7: Data files (DLG_GrandPa_Door.json, Door_GrandPa.json, schema update)
- [x] Build and verify (passed, 28/28 actions)
- [x] Post-review fixes (5 issues):
  - [x] Fix action dispatch ordering (action-first, then components -- preserves JSON order)
  - [x] Fix actor-scope fallback (single best only, no chaining -- preserves legacy)
  - [x] Add authority guards to HandleAction (lock, motion -- no-op on client with warning)
  - [x] ActorWatcher rebind (5s heartbeat detects stream-out/stream-in, auto-rebinds)
  - [x] Add ContainsItem to IInventoryReadOnly (zero-allocation check, used in dialogue condition)
- [x] Build and verify post-review fixes (passed)
- [x] Data-driven actor wiring (tag-based lookup):
  - [x] Add `actorTags` array to object.schema.json (optional string array on root)
  - [x] InteractableActor/ObjectSpawnUtility: apply actorTags from definition to spawned actor's Tags
  - [x] ActorWatcherComponent: add `WatchedActorTag` FName property, resolve via actor tag in TryBindToWatchedActor
  - [x] Door_GrandPa.json: add `"actorTags": ["Scenario.GrandpaDoor"]`
  - [x] Create Grandpa NPC JSON (GrandPa.json) with ActorWatcher cap: `"WatchedActorTag": "Scenario.GrandpaDoor"`
- [ ] Regenerate assets (generator processes updated JSON -> UObjectDefinition)
- [ ] In-editor test: verify full flow (locked -> dialogue -> give water -> unlock -> normal door)

---

## Step 1: Enable Interaction Chaining (InteractableActor)

**File:** `Plugins/Resources/ProjectObject/Source/ProjectObject/Private/Template/Interactable/InteractableActor.cpp`

**Problem:** Lines 171-201 select single best capability for hit mesh. Lockable(100) + Hinged(0) on same mesh -- only Lockable runs. After unlock, Lockable returns true but Hinged never gets called.

**Change:** Collect ALL matching capabilities for mesh, sort by priority desc, chain them.

```
// Pseudocode:
Caps = CollectMatchingCapsForHitMesh();
SortByPriorityDesc(Caps);

// Single-cap fast path (preserves old behavior exactly)
if (Caps.Num() <= 1)
    return ExecuteSingle(Caps[0]);

// Multi-cap chain
UE_LOG(LogInteractableActor, Log,
    TEXT("[%s] Chaining %d capabilities on mesh [%s]"),
    *GetName(), Caps.Num(), *HitMeshName);

for (Cap : Caps)
    if (!OnComponentInteract(Cap, Instigator))
        return false;  // blocked (e.g. Lockable denied)
return true;
```

**Safety:** Single-cap path = old code. Multi-cap chain only fires for new content.

---

## Step 2: DialogueComponent - ActionTarget for Cross-Actor Dispatch

**Files:**
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Public/Components/ProjectDialogueComponent.h`
- `Plugins/Features/ProjectDialogue/Source/ProjectDialogue/Private/Components/ProjectDialogueComponent.cpp`

### 2a: ActionTarget property

```cpp
// Store optional external target for actions (e.g. grandpa's dialogue targets the door)
TWeakObjectPtr<AActor> ActionTarget;
```

### 2b: StartConversation with target

```cpp
// New overload or parameter
void StartConversationWithInstigator(AActor* Instigator, AActor* InActionTarget = nullptr);
```

Sets `ActionTarget` before starting the conversation. If null, falls back to owner (existing behavior).

### 2c: Action routing in DispatchActions

Existing `DispatchActions()` (lines 306-359) already iterates components via `GetComponents()` + `ImplementsInterface(UProjectActionReceiver)`. This component-level dispatch is correct and reaches `ULockableComponent`, `UProjectInventoryComponent`, etc. No need to check the actor itself.

Refactor to support ActionTarget + Instigator:

```cpp
// Extract existing component iteration loop into helper
void DispatchToActor(AActor* Actor, const FString& Context, const TArray<FString>& Actions);

void DispatchActions(const FString& Context, const TArray<FString>& Actions)
{
    // Actions go to action target (door) or owner (grandpa) as fallback
    AActor* Target = ActionTarget.IsValid() ? ActionTarget.Get() : GetOwner();
    DispatchToActor(Target, Context, Actions);

    // Also dispatch to instigator (player) for inventory.consume etc.
    if (AActor* InstigatorActor = CurrentInstigator.Get())
    {
        if (InstigatorActor != Target)  // avoid double dispatch
        {
            DispatchToActor(InstigatorActor, Context, Actions);
        }
    }
}
```

`DispatchToActor` reuses the exact existing pattern: `GetComponents()` -> check `ImplementsInterface(UProjectActionReceiver)` -> `Cast<IProjectActionReceiver>` -> `HandleAction()`. Already proven working in AudioCapabilityComponent dispatch.

### 2d: Inventory Condition Support

Extend `CheckCondition()` to support `inventory:<ObjectId>` prefix. Queries real inventory state via `IInventoryReadOnly` interface (ProjectCore) -- same pattern as `AudioCapabilityComponent.cpp:67-80` ImplementsInterface cast.

```cpp
if (ConditionStr.StartsWith("inventory:"))
{
    FString ItemId = ConditionStr.Mid(10);
    AActor* InstigatorActor = Instigator.Get();
    if (!InstigatorActor) return false;

    // IInventoryReadOnly is on UProjectInventoryComponent, NOT on the actor.
    // Must search components -- same pattern as DispatchActions (lines 306-359).
    TInlineComponentArray<UActorComponent*> Components;
    InstigatorActor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(UInventoryReadOnly::StaticClass()))
        {
            const IInventoryReadOnly* Inventory = Cast<IInventoryReadOnly>(Comp);
            if (!Inventory) continue;
            // Iterate GetEntriesView(), match entry where ItemId == ObjectId.PrimaryAssetName
            return bFound;
        }
    }
    return false;
}
```

**Why IInventoryReadOnly, not tags:**
- Checks actual inventory (source of truth). Item could be consumed, dropped, traded.
- No tag to grant/revoke on pickup/drop.
- Interface is in ProjectCore -- zero new dependencies.
- Must search components (not actor) because `IInventoryReadOnly` is implemented by `UProjectInventoryComponent`, not `AProjectCharacter`. Verified in `ProjectInventoryComponent.h:56`.

---

## Step 3: LockableComponent Action Receiver

**Files:**
- `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Access/LockableComponent.h`
- `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/Access/LockableComponent.cpp`

Add `IProjectActionReceiver` interface:
```cpp
class ULockableComponent : public UActorComponent,
    public IInteractableComponentTargetInterface,
    public IProjectActionReceiver
```

```cpp
void HandleAction(const FString& Context, const FString& Action) override
{
    if (Action == "lock.unlock") { Unlock(); }
    else if (Action == "lock.lock") { Lock(); }
}
```

**No delegate changes needed** -- `OnAccessDenied` already exists and broadcasts on denied interaction.

---

## Step 4: Inventory Action Receiver

**File:** `Plugins/Features/ProjectInventory/Source/ProjectInventory/Public/Components/ProjectInventoryComponent.h` (and .cpp)

Add `IProjectActionReceiver` interface:
```cpp
class UProjectInventoryComponent : public UActorComponent,
    public IInventoryReadOnly, public IInventoryCommands,
    public IProjectActionReceiver
```

```cpp
void HandleAction(const FString& Context, const FString& Action) override
{
    if (!Action.StartsWith("inventory.consume:")) return;
    FString ItemIdStr = Action.Mid(18);
    FPrimaryAssetId ItemId(FPrimaryAssetType("ObjectDefinition"), FName(*ItemIdStr));
    // Find entry by ItemId in Entries, call Internal_RemoveItem(entry.InstanceId, 1)
}
```

---

## Step 5: SpringMotionComponentBase Action Receiver

**Files:** (implemented on BASE class, benefits all motion components)
- `Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Public/Components/SpringMotionComponentBase.h`
- `Plugins/Systems/ProjectMotionSystem/Source/ProjectMotionSystem/Private/Components/SpringMotionComponentBase.cpp`

Add `IProjectActionReceiver`:
```cpp
void HandleAction(const FString& Context, const FString& Action) override
{
    if (Action == "motion.open") { Open(); }
    else if (Action == "motion.close") { Close(); }
    else if (Action == "motion.toggle") { Toggle(); }
}
```

Dialogue uses `motion.open` (deterministic). `motion.toggle` kept for general use.

---

## Step 6: ActorWatcherComponent

**New files:**
- `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Public/Access/ActorWatcherComponent.h`
- `Plugins/Gameplay/ProjectObjectCapabilities/Source/ProjectObjectCapabilities/Private/Access/ActorWatcherComponent.cpp`

Lives in ProjectObjectCapabilities (has access to LockableComponent). Bridges door events to any reaction -- here, dialogue.

```cpp
UCLASS(ClassGroup=(ProjectCapabilities), meta=(BlueprintSpawnableComponent))
class PROJECTOBJECTCAPABILITIES_API UActorWatcherComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Set in level editor: actor this watcher observes
    UPROPERTY(EditInstanceOnly, Category = "ActorWatcher")
    TSoftObjectPtr<AActor> WatchedActor;

    // Fired when watcher receives matching event from watched actor
    UPROPERTY(BlueprintAssignable, Category = "ActorWatcher")
    FOnActorWatchEvent OnWatchEvent;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void TryBindToWatchedActor();

    UFUNCTION()
    void HandleLockAccessDenied(FGameplayTag RequiredKeyTag, AActor* Instigator);

    FTimerHandle RetryHandle;
    bool bBound = false;
};
```

```cpp
void UActorWatcherComponent::BeginPlay()
{
    Super::BeginPlay();
    TryBindToWatchedActor();
}

void UActorWatcherComponent::TryBindToWatchedActor()
{
    AActor* Door = WatchedActor.Get();
    if (!Door)
    {
        // World Partition: door may not be loaded yet. Retry until available.
        GetWorld()->GetTimerManager().SetTimer(RetryHandle, this,
            &ThisClass::TryBindToWatchedActor, 0.5f, false);
        return;
    }

    if (auto* Lockable = Door->FindComponentByClass<ULockableComponent>())
    {
        Lockable->OnAccessDenied.AddDynamic(this, &ThisClass::HandleLockAccessDenied);
        bBound = true;
    }
}

void UActorWatcherComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(RetryHandle);

    if (bBound)
    {
        if (AActor* Door = WatchedActor.Get())
        {
            if (auto* Lockable = Door->FindComponentByClass<ULockableComponent>())
            {
                Lockable->OnAccessDenied.RemoveDynamic(this, &ThisClass::HandleLockAccessDenied);
            }
        }
    }
    Super::EndPlay(Reason);
}

void UActorWatcherComponent::HandleLockAccessDenied(FGameplayTag RequiredKeyTag, AActor* Instigator)
{
    OnWatchEvent.Broadcast(MakeLockDeniedWatchEvent(RequiredKeyTag, Instigator));
}
```

**Streaming safety:** World Partition confirmed (16 cells in KazanMain). Door and grandpa can load at different times. `TryBindToWatchedActor()` retries every 0.5s until door is available. `EndPlay` cleans up timer and delegate.

**Grandpa wires it up:** In Grandpa actor's BeginPlay (or via a small GrandpaDoorScenarioComponent):
```cpp
ActorWatcher->OnWatchEvent.AddDynamic(this, &ThisClass::OnDoorKnocked);

void OnDoorKnocked(const FActorWatchEvent& Event)
{
    DialogueComponent->StartConversationWithInstigator(Event.Instigator.Get(), Event.SourceActor.Get());
}
```

**Design choice:** ActorWatcherComponent is generic (relays door denied events). The scenario-specific logic (start dialogue) lives in Grandpa's own component or actor class, not in the watcher.

---

## Step 7: Data Files

### 7a: Dialogue Tree

**New file:** `Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Door.json`

Single dialogue tree with branches based on game state. Option conditions control which paths the player sees. Conditions use `inventory:ObjectId` (checks real inventory via IInventoryReadOnly).

**Branching logic:**
- Player has water -> "Give water" option visible -> consume, unlock, open
- Player has no water -> only "I'll look for some" visible -> ends, player comes back later
- Door already unlocked -> OnAccessDenied never fires -> dialogue never starts -> normal door

```json
{
  "$schema": "../../../../../Features/ProjectDialogue/Data/Schemas/dialogue.schema.json",
  "id": "DLG_GrandPa_Door",
  "startNode": "greeting",
  "nodes": {
    "greeting": {
      "speaker": "Grandpa",
      "text": "Who's there? Please... I need water. My throat is so dry...",
      "options": [
        {
          "text": "Here, take this water.",
          "next": "give_water",
          "condition": "inventory:WaterBottle"
        },
        {
          "text": "What happened to you?",
          "next": "backstory"
        },
        {
          "text": "I'll look for some.",
          "next": "come_back"
        }
      ]
    },
    "backstory": {
      "speaker": "Grandpa",
      "text": "The door jammed when the quake hit. I've been stuck for days. Please, just some water...",
      "options": [
        {
          "text": "Here, take this water.",
          "next": "give_water",
          "condition": "inventory:WaterBottle"
        },
        {
          "text": "I'll find water and come back.",
          "next": "come_back"
        }
      ]
    },
    "come_back": {
      "speaker": "Grandpa",
      "text": "Please hurry... I don't know how much longer I can wait.",
      "next": "$end"
    },
    "give_water": {
      "speaker": "Grandpa",
      "text": "Oh, bless you! Let me open this door...",
      "actions": [
        "inventory.consume:WaterBottle",
        "lock.unlock",
        "motion.open"
      ],
      "next": "thanks"
    },
    "thanks": {
      "speaker": "Grandpa",
      "text": "Thank you, survivor. You saved an old man's life.",
      "next": "$end"
    }
  }
}
```

**Replay behavior:** If player leaves without giving water ("I'll look for some"), next knock on the door triggers the same dialogue from `greeting`. Grandpa repeats his plea. Once water is given and door unlocks, `OnAccessDenied` stops firing -- dialogue never starts again. Door becomes a normal hinged door.

**Post-unlock grandpa interaction:** After door opens, grandpa is a regular actor with a DialogueComponent. Player can walk up and press E on grandpa directly -- standard dialogue interaction, no ActorWatcher involved. Could be the same DLG_GrandPa_Door tree with a different `startNode`, or a separate `DLG_GrandPa_Free.json` for post-rescue conversation. This is a separate concern from the door scenario and needs no new code -- just data.

### 7b: Door Object Definition (just a door)

**New file:** `Plugins/Resources/ProjectObject/Content/HumanMade/Building/Fenestration/Door/Scenario/Door_GrandPa.json`

```json
{
  "$schema": "../../../../../../Data/Schemas/object.schema.json",
  "id": "Door_GrandPa",
  "meshes": [
    { "id": "frame", "asset": "<EXISTING_FRAME_MESH>" },
    { "id": "door", "asset": "<EXISTING_DOOR_MESH>" }
  ],
  "capabilities": [
    {
      "type": "Lockable",
      "scope": ["door"],
      "properties": {
        "LockTag": "Scenario.GrandpaDoor"
      }
    },
    {
      "type": "Hinged",
      "scope": ["door"],
      "properties": {
        "OpenAngle": "-85"
      }
    }
  ]
}
```

No Dialogue capability on the door.

### 7c: Grandpa Actor Placement

Grandpa actor placed in level near the door. Components:
- `UProjectDialogueComponent` with `DialogueTreeAsset = DLG_GrandPa_Door`
- `UActorWatcherComponent` with `WatchedActor = Door_GrandPa instance ref`
- Scenario glue component that wires `OnWatchEvent -> StartConversation`

Actor class: TBD (could be a generic NPC actor or a BP subclass). Door mesh reference set per-instance in editor.

### 7d: Dialogue Schema Update

**File:** `Plugins/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json`

Document new condition/action formats:
- Conditions: `"inventory:ObjectId"` (has item)
- Actions: `"lock.unlock"`, `"lock.lock"`, `"inventory.consume:ObjectId"`, `"motion.open"`, `"motion.close"`

---

## Files Summary

| # | File | Change |
|---|------|--------|
| 1 | `InteractableActor.cpp` | Chain mesh-scope capabilities (single-cap fast path + multi-cap chain) |
| 2 | `ProjectDialogueComponent.h/.cpp` | ActionTarget, StartConversationWithInstigator, dual dispatch, inventory condition |
| 3 | `LockableComponent.h/.cpp` | Add IProjectActionReceiver for lock.unlock/lock.lock |
| 4 | `ProjectInventoryComponent.h/.cpp` | Add IProjectActionReceiver for inventory.consume |
| 5 | `SpringMotionComponentBase.h/.cpp` | Add IProjectActionReceiver for motion.open/close/toggle (base class) |
| 6 | **NEW** `ActorWatcherComponent.h/.cpp` | Generic: relays door OnAccessDenied to subscribers |
| 7 | **NEW** `DLG_GrandPa_Door.json` | Dialogue tree (grandpa asks for water) |
| 8 | **NEW** `Door_GrandPa.json` | Door object: Lockable + Hinged only |
| 9 | `dialogue.schema.json` | Document inventory condition and action formats |

---

## Reused Patterns

| Pattern | Source | Reuse |
|---------|--------|-------|
| IProjectActionReceiver | `ProjectCore/Interfaces/IProjectActionReceiver.h` | Added to Lockable, Inventory, Motion |
| ImplementsInterface cast | `AudioCapabilityComponent.cpp:67-80` | Same pattern for inventory condition check via IInventoryReadOnly |
| IInventoryReadOnly | `ProjectCore/Interfaces/IInventoryReadOnly.h` | Query real inventory state without ProjectInventory dependency |
| OnAccessDenied delegate | `LockableComponent.h:88-91` | Already exists (BlueprintAssignable). First C++ subscriber: ActorWatcher |
| AddDynamic subscription | Standard UE delegate pattern | ActorWatcher subscribes to existing capability delegate |
| FindComponentByClass | Standard UE pattern | ActorWatcher finding Lockable on door |
| Option conditions | `DLG_GrandPa_Test.json:26` | Existing: `"condition": "Quest.ElderTrust"`. Extended with `inventory:` prefix |

---

## What Changed vs Previous Plan

| Before | After | Why |
|--------|-------|-----|
| Dialogue capability on door | Dialogue on grandpa actor | Door doesn't talk. Grandpa does. |
| BypassCondition / bBypassWhenOwnerUnlocked | Not needed | Dialogue is on grandpa, not door. Door just has Lockable+Hinged. |
| TagActionReceiverComponent | Removed | No per-player state needed |
| Player setup step | Removed | No TagActionReceiver to add |
| New OnInteractDenied delegate | Reuse existing OnAccessDenied | Already broadcasts on LockableComponent |
| `motion.toggle` | `motion.open` | Deterministic, cannot close open door |

---

## Verified Against Codebase

| Concern | Status | Evidence |
|---------|--------|----------|
| World Partition streaming | **Fixed.** ActorWatcher retries bind until door loads | 16 WP cells in KazanMain confirmed |
| IInventoryReadOnly on actor | **Fixed.** Interface is on component, not actor. Search components. | `ProjectInventoryComponent.h:56` |
| Action dispatch reaches components | **Already correct.** Existing dispatch iterates `GetComponents()` | `ProjectDialogueComponent.cpp:306-359` |
| Capability return semantics | **Consistent.** All caps use true=pass, false=block. No "not mine" usage. | Lockable, Pickup verified |
| OnAccessDenied signature | **Confirmed.** `FOnAccessDenied(FGameplayTag, AActor*)` | `LockableComponent.h:88-91` |

## Known Gaps (out of scope)

**Dialogue action authority:** SelectOption() runs client-side. Actions dispatch without server verification. Low risk for single-player/coop. Needs Server RPC for competitive multiplayer.

**Grandpa visibility:** Grandpa is behind the door (invisible to player). Could be an invisible actor or have collision disabled. TBD based on gameplay needs.

---

## Verification

1. **Build:** `scripts/ue/standalone/build.ps1`
2. **Regenerate assets:** Editor processes DLG_GrandPa_Door.json and Door_GrandPa.json
3. **Place in level:** Door_GrandPa + Grandpa actor, wire ActorWatcher reference
4. **Test locked:** E on door -> grandpa dialogue starts -> "I'll be back" -> door stays locked
5. **Test no water:** No "Give water" option visible
6. **Test with water:** Give water -> consumed, door unlocks and opens
7. **Test after unlock:** E on door -> no dialogue (Lockable passes) -> door toggles normally
8. **Test existing doors:** ComplexDoor_Inner_Beige still works after chaining change
9. **Test multiplayer:** Second player -> door already unlocked -> works as normal door

---

## Resolved Details

- **Water item:** `WaterBottle` (ObjectId) at `Content/HumanMade/Consumables/Vital/Nutrition/Drink/WaterBottle/WaterBottle.json`
- **Player class:** `AProjectCharacter` at `Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Public/ProjectCharacter.h`
- **Door mesh:** TBD -- user will provide
- **OnAccessDenied:** Already exists in LockableComponent.h:88-91, broadcast at LockableComponent.cpp:57

