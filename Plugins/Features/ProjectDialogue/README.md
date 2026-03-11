# ProjectDialogue

Universal data-driven dialogue system for any actor (NPCs, objects, terminals).

## Architecture

- **Data-driven**: Dialogue trees defined in JSON, auto-generated to UAssets via ProjectDefinitionGenerator
- **Component-based**: `UProjectDialogueComponent` attached to any actor
- **KISS**: Tree-based navigation with semantic node IDs, GameplayTag conditions
- **Blackbox UI**: ProjectDialogueUI plugin (separate) consumes IDialogueService

## Data Flow

```
DLG_*.json (ProjectObject/Content/) -> Generator -> UDialogueTreeDefinition (same dir as JSON)
                                                          |
                                                  UProjectDialogueComponent
                                                          |
                                              FDialogueServiceImpl (ServiceLocator)
                                                          |
                                              UDialogueViewModel (MVVM)
                                                          |
                                                  W_DialoguePanel (Widget)
```

## Interaction

Press E on actor with dialogue component -> IInteractionService -> FDialogueInteractionHandler -> StartConversation

If dialogue is already active, re-interact only closes it when player interacts with
the same dialogue owner actor. Interactions on other actors are ignored while dialogue is active.

## Action Dispatch

Dialogue nodes can have `actions` (string array). On node entry, actions are dispatched
to `IProjectActionReceiver` components via `DispatchActions()`.

**Routing:**
- Actions go to **ActionTarget** (if set via `StartConversationWithInstigator`) or **owner** (default)
- When ActionTarget is external, actions are also dispatched to **owner** (owner-local actions remain available)
- Actions also go to **instigator** (player) for player-side receivers (e.g. inventory)
- Each receiver filters by its own namespace prefix

**Ordering:** Actions are dispatched in JSON array order. Each action runs on ALL receivers
before the next action starts. This guarantees `lock.unlock` completes before `motion.open`.

**Authority:** Action receivers require server authority. Dialogue dispatch must run on authority
context (listen server or dedicated server). Receivers log a warning and no-op on client.

**ActionTarget use case:** Grandpa starts dialogue but actions target a door.
Call `StartConversationWithInstigator(Player, DoorActor)` to route actions to the door.

**Action receivers** -- each plugin documents its own supported actions:
- ProjectDialogue -- `dialogue.set_tree:/ProjectObject/.../DLG_X.DLG_X` (switch tree after conversation end)
- [ProjectObjectCapabilities](../../Gameplay/ProjectObjectCapabilities/README.md) -- `lock.*`, `audio.*`
- [ProjectMotionSystem](../../Systems/ProjectMotionSystem/README.md) -- `motion.*`
- [ProjectInventory](../ProjectInventory/README.md) -- `inventory.*`

`dialogue.set_tree` notes:
- Uses a single `DialogueTreeAsset` reference (no array needed).
- If called during conversation, switch is queued and applied on `EndConversation()`.
- The switch is runtime state only; it does not overwrite placed actor defaults in the map asset.

## Door Trigger Path

If an actor has both `UProjectDialogueComponent` and `UActorWatcherComponent`, the dialogue
component can auto-start from watch events (default filter: `lock.access_denied`) and routes
node actions to the event `SourceActor` as ActionTarget.

When auto-start is triggered (not direct player interaction), `UProjectDialogueComponent`
now explicitly activates `IDialogueService` before entering conversation so Dialogue UI reflects
watcher-started conversations as expected.

## Troubleshooting

Door lock deny should trigger NPC dialogue:

1. `LogProjectObjectCapabilities` shows `Lockable` `Access denied`
2. `LogProjectObjectCapabilities` shows `ActorWatcher` `Event emitted (Name=lock.access_denied, ...)`
3. `LogDialogueComponent` shows `HandleActorWatchEvent` `Auto-start from event`
4. `LogDialogueVM` shows `RefreshFromService: bActive=1`

If step 1-2 pass but no dialogue panel:
- Check that NPC has `UProjectDialogueComponent` and `DialogueTreeAsset` is valid.
- Check `AutoStartWatchEventName` matches emitted event (`lock.access_denied` by default).
- Check `IDialogueService` is available at runtime (warning is logged when missing).

If panel appears and immediately disappears:
- Old behavior closed dialogue on any interact while active.
- Current behavior closes only when re-interacting with the same dialogue owner actor.
- Rebuild and verify no immediate `Dialogue active, ending on re-interact` after door-triggered start.

Log note:
- `Started dialogue tree 'None' at node 'greeting'` can still be valid. `TreeId` is optional metadata in the dialogue asset.

## Option Conditions

Options support `condition` field for availability gating:
- GameplayTag string -- player ASC has matching tag (e.g. `"Quest.ElderTrust"`)
- `inventory:<ObjectId>` -- player inventory has exact item (e.g. `"inventory:WaterBottle"`)
- `inventory:<ObjectId>*` -- player inventory has any matching family variant (e.g. `"inventory:WaterBottle*"`)

Condition behavior:
- Options stay visible even when condition fails.
- Failed condition -> option is disabled/greyed as `(Unavailable)`.
- Condition becomes true -> option is enabled and subtly highlighted while condition remains true.

Inventory condition notes:
- Queries `IInventoryReadOnly` (ProjectCore) on instigator components.
- Supports exact object id (`inventory:WaterBottle` -> `ObjectDefinition:WaterBottle`).
- Supports explicit family wildcard (`inventory:WaterBottle*` matches `WaterBottleBig`, `WaterBottleSmall`).

## JSON Data (co-located with objects)

Dialogue JSON files live alongside their owning entity in `Plugins/Resources/ProjectObject/Content/`.
Files use `DLG_` prefix so the generator can distinguish them from object definitions.

- Schema: `Data/Schemas/dialogue.schema.json`
- Object dialogue: `ProjectObject/Content/.../DLG_<name>.json`
- NPC dialogue: `ProjectObject/Content/Human/<NPC>/DLG_<name>.json`
- See [ProjectObject](../../Resources/ProjectObject/README.md) for content hierarchy

### Schema validation

Every `DLG_*.json` file MUST include a `$schema` field pointing to the dialogue schema
via relative path. This enables IDE-agnostic validation (VS Code, Rider, etc.) without
any editor-specific configuration.

```json
{
  "$schema": "<relative-path-to>/Features/ProjectDialogue/Data/Schemas/dialogue.schema.json",
  "id": "DLG_MyDialogue",
  ...
}
```

The `id` field uses `DLG_` prefix (matching filename) for quick asset type identification.
The generator ignores `$schema` during parsing.

## Key Files

| File | Purpose |
|------|---------|
| Public/Data/ProjectDialogueTypes.h | FDialogueNode, FDialogueOption structs |
| Public/Data/DialogueTreeDefinition.h | UAsset class for generated trees |
| Public/Components/ProjectDialogueComponent.h | Core component (attach to any actor) |
| Private/Services/DialogueServiceImpl.h | IDialogueService bridge |
| Private/Interaction/DialogueInteractionHandler.h | Press E handler |

## Dependencies

- ProjectCore (ServiceLocator, FProjectPaths)
- ProjectFeature (FFeatureRegistry)
- GameplayTags (conditions)
- GameplayAbilities (ASC query for conditions)

## See Also

- `Plugins/UI/ProjectDialogueUI/` - UI blackbox plugin
- `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Interfaces/IDialogueService.h` - Service interface
