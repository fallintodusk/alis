# City17: Damage Zones + Sniper Bleeding + Death Respawn

## Problem

City17 needs three connected gameplay loops:
1. **Damage boxes** - volumes that hurt the player on overlap (generic reusable actor)
2. **Sniper zone** - enter area -> light wound + bleeding -> use bandage to stop
3. **Death respawn** - Condition=0 -> show "I will try again" -> reload map fresh

## What Already Exists

| Layer | Asset | Location |
|-------|-------|----------|
| Damage application | `ApplyMagnitudes()` / `ApplySingleMagnitude()` | `ProjectGAS/Public/ProjectGASLibrary.h` |
| Health attribute | `HealthAttributeSet::Condition` (0-100) | `ProjectGAS/Public/Attributes/HealthAttributeSet.h` |
| Bleeding attribute | `StatusAttributeSet::Bleeding` (0-1 intensity) | `ProjectGAS/Public/Attributes/StatusAttributeSet.h` |
| Bleeding drain | `ApplyBleedingDrain()` - drains Condition each vitals tick | `ProjectVitals/Private/ProjectVitalsComponent.cpp:582` |
| Bleeding config | tiers: light(0.02), moderate(0.1), severe(0.5), critical(1.0) | `ProjectVitals/Data/vitals_config.json` |
| Item use flow | `Internal_UseItem()` -> reads magnitudes -> `ApplyMagnitudes()` | `ProjectInventory/Private/Components/ProjectInventoryComponent.cpp:2928` |
| Medkit item def | Exists, says "Stops bleeding", but **NO magnitudes field** | `ProjectObject/Content/.../Medkit/Medkit.json` |
| Mind toast system | `IMindService` -> `FMindThoughtView` -> `W_MindThoughtToast` (top-right) | `ProjectCore/Public/Interfaces/IMindService.h` |
| Capability system | `CapabilityRegistry` + JSON-driven components | `ProjectObjectCapabilities/` |
| Map loading | `ILoadingService::BuildLoadRequestForExperience()` + `StartLoad()` | `ProjectCore/Public/Services/ILoadingService.h` |
| Death tags | `Status.Death.Dying`, `Status.Death.Dead` | `ProjectCore/Public/ProjectGameplayTags.h` |
| ASC attribute change | `GetGameplayAttributeValueChangeDelegate()` | UE AbilitySystemComponent API |

---

## New Classes by Plugin

| Plugin | Class | Purpose |
|--------|-------|---------|
| **ProjectObjectCapabilities** | `UEnvironmentEffectComponent` (NEW capability) | Overlap-based GAS magnitude applicator |
| **ProjectSinglePlay** | (addition to existing `ASinglePlayerGameMode`) | Death handler: ASC binding + reload |

**Data-only changes:**
- New `Bandage.json` in `ProjectObject/Content/HumanMade/Consumables/Vital/Health/Medical/Bandage/`

---

## Part 1: Environment Effect Capability

**Goal:** Data-driven environmental hazard. New capability in existing ProjectObjectCapabilities.

### Plugin: ProjectObjectCapabilities (EXISTING)

```
Plugins/Gameplay/ProjectObjectCapabilities/
  Source/ProjectObjectCapabilities/
    Public/Environmental/
      EnvironmentEffectComponent.h     (NEW)
    Private/Environmental/
      EnvironmentEffectComponent.cpp   (NEW)
```

Auto-registers via `GetPrimaryAssetId()` returning `CapabilityComponent:EnvironmentEffect`.

### Trigger shape requirement

The capability is effect logic, NOT a spatial trigger. It requires a real `UShapeComponent` on the owning actor. No mesh collision fallback in v1.

```cpp
// BeginPlay - iterate owner's components, match by name
UShapeComponent* Trigger = nullptr;
for (UShapeComponent* Shape : TInlineComponentArray<UShapeComponent*>(GetOwner()))
{
    if (Shape->GetFName() == TriggerComponentName)
    {
        Trigger = Shape;
        break;
    }
}
if (!Trigger)
{
    UE_LOG(LogEnvironmentEffect, Warning,
        TEXT("EnvironmentEffect '%s': no UShapeComponent named '%s' found. Deactivating."),
        *GetName(), *TriggerComponentName.ToString());
    Deactivate();  // not just SetComponentTickEnabled -- component is timer/delegate driven
    return;
}
bIsEffectEnabled = true;
```

Guard overlap handlers with early return on `!bIsEffectEnabled`. Hazard zones must be explicit shape triggers, no mesh collision fallback.

### GAS bridge -- ProjectCore data contract + ProjectGAS runtime call

Same pattern as ProjectInventory:
1. Magnitudes stored as `TMap<FGameplayTag, float>` -- ProjectCore type (`FItemDataView::Magnitudes`)
2. SetByCaller tags defined in `ProjectCore/ProjectGameplayTags.h`
3. At runtime, convert to `TArray<FAttributeMagnitude>` and call `UProjectGASLibrary::ApplyMagnitudes()`
4. Add `ProjectGAS` to `ProjectObjectCapabilities.Build.cs` PrivateDependencyModuleNames

See `ProjectInventory/Private/Components/ProjectInventoryComponent.cpp:2964-2972` for the exact bridge code.

### Log category

Use `LogEnvironmentEffect` (or `LogProjectObjectCapabilities`), NOT `LogTemp`.

### API

```cpp
UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
FName TriggerComponentName = TEXT("Trigger");

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
TMap<FGameplayTag, float> EntryMagnitudes;       // fire once on begin overlap

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
TMap<FGameplayTag, float> PeriodicMagnitudes;    // fire every TickInterval while inside

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
TMap<FGameplayTag, float> ExitMagnitudes;        // fire once on end overlap

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
float TickInterval = 0.0f;

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
int32 MaxApplications = 0;                       // 0 = unlimited

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
bool bPersistentApplicationLimit = false;        // true = track across re-entries

UPROPERTY(EditAnywhere, Category="EnvironmentEffect")
bool bAffectPawnsOnly = true;

UPROPERTY(EditAnywhere, Category="Audio")
TSoftObjectPtr<USoundBase> EntrySound;

UPROPERTY(EditAnywhere, Category="Audio")
bool bPlaySoundAs2D = false;                     // true = PlaySound2D, false = PlaySoundAtLocation
```

Dropped from v1:
- `bApplyEntryOncePerOverlap` -- entry on BeginOverlap is naturally once per overlap session, ActiveActors dedupes. No concrete use case for re-triggering within a single overlap.
- `ExitSound` -- no current use case, add when needed.

**Why three maps:** Entry vs Periodic are different concerns:

| Zone | Entry | Periodic | Exit |
|------|-------|----------|------|
| Sniper | Condition -10, Bleeding +0.1 | none | none |
| Fire | none | Condition -5 (every 1.0s) | none |
| Gas cloud | Radiation +0.3 | Condition -1 (every 1.0s) | none |
| Damage box | Condition -25 | none | none |

### Application lifetime: per-session vs persistent

| Mode | Use case | Behavior |
|------|----------|----------|
| `bPersistentApplicationLimit = false` | Fire, gas, traps | MaxApplications resets each overlap session |
| `bPersistentApplicationLimit = true` | Sniper | MaxApplications tracked across re-entries, never resets |

```cpp
// Active overlap state (cleared on EndOverlap)
struct FEnvironmentEffectState
{
    int32 ApplicationCount = 0;
    FTimerHandle TimerHandle;
};
TMap<TWeakObjectPtr<AActor>, FEnvironmentEffectState> ActiveActors;

// Persistent tracking (survives EndOverlap, cleared only on EndPlay)
TSet<TWeakObjectPtr<AActor>> PermanentAppliedActors;
```

### Persistent limit recorded at apply time

Record persistent limit immediately after successful application, NOT only on EndOverlap. If the actor dies/teleports/level unloads before EndOverlap fires, the state must already be persisted.

```cpp
// After successful entry application:
if (bPersistentApplicationLimit && MaxApplications > 0
    && State.ApplicationCount >= MaxApplications)
{
    PermanentAppliedActors.Add(OtherActor);
}
```

EndOverlap still cleans up ActiveActors, but the critical persist happens at apply time.

### Behavior rules

- BeginPlay: find `UShapeComponent` by `TriggerComponentName`, bind overlap delegates, validate config. If no trigger found, log warning and disable.
- OnBeginOverlap:
  - Early return if `!bIsEffectEnabled`
  - If `bPersistentApplicationLimit` and actor in `PermanentAppliedActors` -> skip
  - Validate actor, dedupe in ActiveActors
  - Apply entry magnitudes, increment ApplicationCount
  - Persist limit if reached (immediately, not deferred to EndOverlap)
  - Start timer only if `TickInterval > 0` AND `PeriodicMagnitudes` not empty
- Periodic timer tick:
  - Apply periodic magnitudes, increment ApplicationCount
  - If `MaxApplications > 0` and count reached -> stop timer, persist if `bPersistentApplicationLimit`
- OnEndOverlap:
  - Stop timer
  - Apply exit magnitudes if configured (do NOT mix with `bPersistentApplicationLimit=true` zones)
  - Remove from ActiveActors
- EndPlay: clear ALL timers and both tracking maps
- Use `FTimerHandle` per actor -- NOT `Tick()`, not frame-based

### Config validation (BeginPlay warnings)

```cpp
if (TickInterval <= 0.0f && !PeriodicMagnitudes.IsEmpty())
    UE_LOG(LogEnvironmentEffect, Warning,
        TEXT("EnvironmentEffect '%s': PeriodicMagnitudes but TickInterval <= 0"), *GetName());
if (MaxApplications < 0)
    UE_LOG(LogEnvironmentEffect, Warning,
        TEXT("EnvironmentEffect '%s': invalid MaxApplications < 0"), *GetName());
```

### ExitMagnitudes caveat

Exit magnitudes are fragile for reversible status effects (overlapping zones subtract blindly, not source-aware). Fine for simple cases now. Real fix later: tracked effect handles.

---

## Part 2: Sniper Zone (Wound + Bleeding)

**Goal:** Enter trigger area -> one-shot light damage + bleeding + bullet impact sound.

Object with EnvironmentEffect capability + a trigger volume. Placed in editor per-instance.

### Level-placed instance config

Actor with `UBoxComponent` named "Trigger" + `UEnvironmentEffectComponent`:

```
TriggerComponentName = "Trigger"
MaxApplications = 1
bPersistentApplicationLimit = true    // once ever, not once per overlap session
EntrySound = sniper_shot
bPlaySoundAs2D = true                 // immediate hit feedback, not spatial from zone center

EntryMagnitudes:
  SetByCaller.Condition = -10.0       // light wound, 10% HP
  SetByCaller.Bleeding = 0.1          // moderate bleeding tier

PeriodicMagnitudes: empty
ExitMagnitudes: empty
```

**Result:** Player enters zone, hears sniper shot (2D), takes 10 HP damage + starts bleeding. Stepping out and back in does NOT re-trigger.

### Bleeding Cure Item -- Bandage (NEW, not Medkit)

Create `Bandage.json` in `ProjectObject/Content/HumanMade/Consumables/Vital/Health/Medical/Bandage/`:

```json
{
  "id": "Bandage",
  "meshes": [
    {
      "id": "body",
      "asset": "/ProjectObject/HumanMade/Consumables/Vital/Health/Medical/Bandage/SM_Bandage"
    }
  ],
  "capabilities": [
    {
      "type": "Pickup",
      "scope": ["actor"],
      "properties": {
        "InitialQuantity": "1",
        "PickupTime": "0"
      }
    }
  ],
  "sections": {
    "item": {
      "displayName": "Bandage",
      "description": "Stops bleeding. Apply to wound to stop blood loss.",
      "iconCode": "",
      "weight": 0.02,
      "volume": 0.01,
      "maxStack": 5,
      "tags": ["Item.Type.Consumable"],
      "canBeDropped": true,
      "canBeTraded": true,
      "isQuestItem": false,
      "magnitudes": {
        "SetByCaller.Bleeding": -1.0
      },
      "consumeOnUse": true
    }
  }
}
```

- Bleeding-only cure, not a medkit. Medkit stays as-is for bigger heal later.
- No C++ changes -- existing `Internal_UseItem()` -> `ApplyMagnitudes()` handles it.

### Pre-implementation verification

- [ ] `object.schema.json` includes `magnitudes` field in item section
- [ ] `object.schema.json` includes `consumeOnUse` field in item section
- [ ] Runtime parser for `FItemSection` reads both fields
- [ ] `ApplyMagnitudes()` accepts `SetByCaller.*` tag namespace from JSON
- [ ] Bandage mesh asset exists or needs placeholder

---

## Part 3: Death -> "I will try again" -> Map Reload

**Goal:** When Condition hits 0, fade to black, show text, reload map fresh.

### Death Detection -- ASC Attribute Change Delegate

Bind to Condition attribute value change on the player's ASC. Detect transition `Old > 0 && New <= 0`.

```cpp
ASC->GetGameplayAttributeValueChangeDelegate(
    UHealthAttributeSet::GetConditionAttribute()
).AddUObject(this, &ThisClass::HandleConditionChanged);
```

```cpp
void ThisClass::HandleConditionChanged(const FOnAttributeChangeData& Data)
{
    if (!bDeathSequenceStarted && Data.OldValue > 0.0f && Data.NewValue <= 0.0f)
    {
        bDeathSequenceStarted = true;
        StartDeathSequence();
    }
}
```

### ASC delegate cleanup

Store delegate handle per player context (not one global). Unbind explicitly when:
- Player controller changes pawn
- Game mode restarts level
- Object owning the delegate is destroyed

Prevents duplicate death handling after respawn/reload.

```cpp
// Per-player death tracking (not global, even in single-player)
struct FPlayerDeathContext
{
    FDelegateHandle ConditionChangedHandle;
    bool bDeathSequenceStarted = false;
};
TMap<TWeakObjectPtr<APlayerController>, FPlayerDeathContext> PlayerDeathContexts;

// Bind (in HandleStartingNewPlayer)
auto& Ctx = PlayerDeathContexts.FindOrAdd(PlayerController);
Ctx.ConditionChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
    UHealthAttributeSet::GetConditionAttribute()
).AddUObject(this, &ThisClass::HandleConditionChanged);

// Cleanup
ASC->GetGameplayAttributeValueChangeDelegate(
    UHealthAttributeSet::GetConditionAttribute()
).Remove(Ctx.ConditionChangedHandle);
```

### Death Flow -- split authority vs local

**Authority side (GameMode):**
- Binds to player ASC Condition attribute change in HandleStartingNewPlayer
- Detects death, notifies PlayerController
- Starts restart timer
- Reloads map via `ILoadingService`

**Local side (PlayerController):**
- Disables input
- Camera fade to black
- Emits "I will try again" thought

### Death Flow

1. **GameMode** `HandleConditionChanged()` detects `Old > 0 && New <= 0`
2. Guard: `bDeathSequenceStarted = true`
3. **GameMode** notifies PlayerController: `OnPlayerDeath()`
4. **PlayerController** (local):
   a. Disable input
   b. `PlayerCameraManager->StartCameraFade(0, 1, 1.0f, FLinearColor::Black, false, true)`
   c. Emit thought via existing Mind toast: "I will try again" (`Channel = Toast`, TTL = 1.0f)
5. **GameMode** starts 1.5s timer (fade completes at 1.0s, 0.5s black before reload)
6. **GameMode** reloads current experience/map via `ILoadingService`

Ragdoll is optional, off by default for v1. Not every pawn setup tolerates instant `SetSimulatePhysics(true)` cleanly (no physics asset, capsule mismatch, camera issues). Fade + input disable + reload is sufficient for City17 first pass.

### Death UI -- first pass uses existing toast

Use the existing `EMindThoughtChannel::Toast` (top-right) for "I will try again". Zero UI changes.

**Phase 2 (later, not this task):** Add `EMindThoughtChannel::Announcement` + center-screen HUD slot + announcement widget. Only when center-screen messages are needed elsewhere.

### Map Reload

Use standard loading pipeline. Pull current experience name from authoritative runtime state, do NOT hardcode.

```cpp
auto LoadingService = FProjectServiceLocator::Resolve<ILoadingService>();
if (LoadingService)
{
    FLoadRequest Request;
    FText Error;
    LoadingService->BuildLoadRequestForExperience(CurrentExperienceName, Request, Error);
    LoadingService->StartLoad(Request);
}
```

**Note:** Verify `ILoadingService` exposes current active experience name. If not, add accessor.

---

## Implementation Order

1. **UEnvironmentEffectComponent** in ProjectObjectCapabilities -- ~120-150 lines
   - Requires named `UShapeComponent` trigger (strict, no mesh collision fallback)
   - Entry/Periodic/Exit magnitude maps, per-actor timers, optional entry sound
   - ProjectCore data contract + ProjectGAS runtime call
   - Proper log category (LogEnvironmentEffect)
2. **Verify schema** for `magnitudes` + `consumeOnUse` in `object.schema.json`
3. **Bandage.json** (NEW item data) -- bleeding-only cure, consumable
4. **Death handler** addition to SinglePlayerGameMode + PlayerController
   - ASC Condition attribute change binding + stored delegate handle for cleanup
   - Authority/local split
   - Fade to black + existing toast (no new UI)
5. **Place hazard actors in City17 map** (editor: actor with BoxCollision + EnvironmentEffect component)
6. **Place bandage pickup** near sniper zone
7. **Mark** `todo/00_current/implement_poison_gas_cloud.md` as superseded by EnvironmentEffect capability
   - Delete only after capability is merged and used

## Decisions

- Volume placement: EnvironmentEffect capability in existing ProjectObjectCapabilities (no new plugin)
- Trigger shape: strict `UShapeComponent` lookup by name iteration, no mesh collision fallback. Deactivate component (not just disable tick) if missing
- GAS bridge: ProjectCore data contract (`TMap<FGameplayTag, float>`) + ProjectGAS runtime call (same pattern as inventory)
- Log category: `LogEnvironmentEffect`, not `LogTemp`
- Persistent limit: recorded at apply time, not deferred to EndOverlap
- Sniper audio: `sniper_shot` via `EntrySound` + `bPlaySoundAs2D=true`
- Sniper retrigger: `bPersistentApplicationLimit=true` -- once ever, not once per overlap session
- Death presentation: fade to black (1.0s) + existing Mind toast, ragdoll optional/off by default
- Death UI phase 2 (later): center-screen Announcement channel when needed elsewhere
- Death flow split: GameMode = authority (detect + reload), PlayerController = local (fade + toast + input)
- ASC delegate: per-player context (not global), store handle, unbind on pawn change / level restart / destroy
- Periodic timer: also increments ApplicationCount and obeys MaxApplications
- Timing: fade 1.0s, reload at 1.5s (0.5s hold on black)
- Bleeding cure: Medkit.json temporarily (has magnitudes + consumeOnUse). TODO: create separate Bandage item with own mesh when available
- Bandage use: no animation, instant GAS application via existing item-use flow
- Poison gas todo: mark superseded, delete only after capability is merged
- Death detection: ASC Condition attribute change delegate, NOT vitals tick
- Volume API: three separate maps (Entry/Periodic/Exit), NOT mixed
- Dropped from v1: `bApplyEntryOncePerOverlap` (redundant), `ExitSound` (no use case)

## Verification

- Place actor with BoxCollision("Trigger") + EnvironmentEffect component in City17, walk into it, confirm HP drops
- Place sniper zone (MaxApplications=1, persistent), walk in, confirm HP drop + bleeding + 2D sound
- Step out and back in -- confirm sniper does NOT re-trigger
- Pick up bandage, use it, confirm bleeding stops
- Let bleeding drain to Condition=0, confirm fade to black + toast, map reloads after 1.5s
- After reload, confirm fresh spawn (full HP, no bleeding, at PlayerStart)
- Place periodic fire zone, stand inside, confirm damage ticks at interval
- Verify EnvironmentEffect logs warning and disables if no UShapeComponent found
