# ProjectObjectCapabilities

Reusable capability components for world objects (access control, state).

## Purpose

- **COMPONENTS ONLY** - this plugin contains UActorComponent classes, NOT actors
- Provides composable capabilities that any actor can attach
- Not tied to specific actor types - generic building blocks
- Pairs with ProjectInteraction (player side) for complete interaction flow
- **World interactions only** - item identity data lives in ObjectDefinition.Item, not here

**Architecture:** See [Layer Contract](../../Resources/ProjectObject/docs/layer_contract.md) for the 3-layer separation (Capabilities / Item Data / GAS Profiles).

## Components vs Actors (IMPORTANT)

```
ProjectObjectCapabilities (this)     ProjectObject (Resources/)
------------------------------       --------------------------
COMPONENTS ONLY                      ACTORS that compose components

ULockableComponent          --->     AOpenableActor (has LockComponent)
UHealthComponent (future)   --->     ADestructibleActor (future)
UPowerableComponent (future)--->     APoweredDevice (future)
```

**Rule:** If you're creating a reusable capability (lock, health, power), put it HERE.
If you're creating a placeable world object, put it in ProjectObject.

## Architecture Position

```
Gameplay/
├── ProjectInteraction/         <- Player side (E key, traces)
└── ProjectObjectCapabilities/  <- Object side (lock, state)
    └── Access/
        └── ULockableComponent
```

## Components

### ULockableComponent

Access control component using FGameplayTag for key matching.

**Properties:**
| Property | Type | Description |
|----------|------|-------------|
| `LockTag` | FGameplayTag | Required key tag (empty = unlocked) |
| `bConsumeKeyOnUnlock` | bool | Whether key is consumed on use |
| `bIsUnlocked` | bool | Runtime state (replicated) |

LockTag requirement:
- `LockTag` must be a registered gameplay tag.
- Project-owned tags should be native tags from `ProjectCore` (`ProjectGameplayTags.h/.cpp`).
- `Config/DefaultGameplayTags.ini` should be used for engine/plugin/imported tags that are not native declarations.
- Invalid tag strings are now logged as errors during definition spawn/apply and the lock behaves as unlocked.

**API:**
```cpp
// Query
bool IsLocked() const;           // Has LockTag and not yet unlocked
FGameplayTag GetLockTag() const; // Get required key tag
bool ShouldConsumeKey() const;   // Whether to consume key

// Mutation (server-only)
void Unlock();  // Mark as unlocked
void Lock();    // Reset to locked state
```

**Delegates:**
```cpp
FOnAccessDenied OnAccessDenied;  // Broadcast when locked + interaction attempted
FOnUnlocked OnUnlocked;          // Broadcast when unlocked
FOnLocked OnLocked;              // Broadcast when locked
```

**IProjectActionReceiver** (authority-only, no-ops on client):
- `lock.unlock` -> calls Unlock()
- `lock.lock` -> calls Lock()

Interaction passthrough:
- When already unlocked, interaction forwards `motion.toggle` to same-scope motion receivers (door keeps normal open/close behavior in single-selection interaction mode).

Mesh scope persistence contract:
- Lockable mesh scope binding must persist across editor world -> PIE duplication.
- If mesh scope is missing at runtime, mesh-scoped motion capabilities can win focus/interaction (`Open`) before lock checks.

**Usage:**
```cpp
// In actor constructor
LockComponent = CreateDefaultSubobject<ULockableComponent>(TEXT("LockComponent"));

// In interaction handler
if (auto* Lock = Target->FindComponentByClass<ULockableComponent>())
{
    if (Lock->IsLocked())
    {
        if (!HasKeyFor(Lock->GetLockTag()))
            return; // No access
        Lock->Unlock();
    }
}
```

### UActorWatcherComponent

Universal watched-actor event capability.
Resolves a target actor by explicit reference and/or query filters, binds to a trigger source,
then emits normalized events to listeners on the owner (via `IActorWatchEventListener`) and
through a Blueprint multicast delegate.

**Properties:**
| Property | Type | Description |
|----------|------|-------------|
| `WatchedActor` | TSoftObjectPtr\<AActor\> | Explicit actor to watch |
| `WatchedActorTag` | FName | Optional actor tag filter for actor resolution |
| `WatchedActorNameContains` | FString | Optional actor-name substring filter |
| `Trigger` | EActorWatchTrigger | Source trigger type (`LockAccessDenied`) |
| `RequiredEventName` | FName | Optional exact event name filter |
| `RequiredEventTag` | FGameplayTag | Optional exact gameplay-tag filter |
| `RequiredEventTextContains` | FString | Optional case-insensitive event text substring filter |

**Delegates:**
```cpp
FOnActorWatchEvent OnWatchEvent;  // Normalized event payload (name/tag/text/source/instigator)
```

**Use case:** NPC actor watches nearby locked door and reacts to denied access without
hard-coupling to lockable internals in dialogue code.

```cpp
class UMyNpcLogicComponent : public UActorComponent, public IActorWatchEventListener
{
    void HandleActorWatchEvent(const FActorWatchEvent& Event) override
    {
        if (Event.EventName == FName(TEXT("lock.access_denied")))
        {
            // React (start dialogue, quest, bark, etc.)
        }
    }
};
```

`ActorWatcher` is a capability ID (`CapabilityComponent:ActorWatcher`) and can be spawned from object definitions.

Runtime contract for door -> NPC dialogue:
- `ULockableComponent` only reports deny (`OnAccessDenied`) and does not open UI.
- `UActorWatcherComponent` only emits normalized events and notifies listeners.
- Dialogue/UI behavior is owned by listener components (for example `UProjectDialogueComponent`).

Quick log checks:
- Lock path: `LogProjectObjectCapabilities ... Lockable ... Access denied`
- Watch path: `LogProjectObjectCapabilities ... ActorWatcher ... Event emitted (Name=lock.access_denied, ...)`
- If both exist, capability side is healthy and debugging should continue in ProjectDialogue/ProjectDialogueUI.

## Future Components

Planned capability components:
- `UPowerableComponent` - Requires power to function
- `UHealthComponent` - Can be damaged/destroyed
- `UOwnershipComponent` - Owner-only access

## Dependencies

- ProjectCore (foundation)
- GameplayTags (for FGameplayTag)

## Consumers

- ProjectObject (Resources) - Door, window, chest actors
- ProjectInventory (Features) - Interaction handler checks lock

## References

- [ProjectMotionSystem](../../Systems/ProjectMotionSystem/README.md) - USpringRotatorComponent (motion)
- [ProjectObject](../../Resources/ProjectObject/README.md) - Uses these components
- [ProjectInteraction](../ProjectInteraction/README.md) - Player-side interaction

## Legacy Paths

Canonical ID registry for this refactor:
- [todo/done/generalize_placeable_actor_parent.md](../../../todo/done/generalize_placeable_actor_parent.md)

Code marker format:
- `// LEGACY_OBJECT_PARENT_GENERALIZATION(L###): <reason>. Remove when <condition>.`

| Legacy ID | Location | Why It Exists | Remove Trigger |
|-----------|----------|---------------|----------------|
| _(none active)_ | n/a | Capability hierarchy resolution now uses strict interface-only mesh targeting | n/a |
