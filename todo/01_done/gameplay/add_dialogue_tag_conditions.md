# Add Dialogue Tag Condition Support

**Goal:** Enable dialogue options to gate on gameplay tags (quest progress, trust, story flags) via `IGameplayTagAssetInterface` -- not by hard-coupling `ProjectDialogue` to Gameplay Ability System.

**Branch:** Artur's branch (builds on `dc93268ee` dialogue condition refactor)

---

## Context

Artur refactored dialogue conditions from flat strings to structured `FDialogueCondition` objects (`type`, `id`, `quantity`, `exact`). The struct shape already supports a `"tag"` type, but `CheckCondition()` only implements `"inventory"`. GameplayTag conditions were silently dropped.

The struct is ready. The runtime has a gap.

---

## Prerequisite: Character Tag Interface (Step 0)

Neither `AProjectCharacter` nor `ADefinitionCharacter` implements `IGameplayTagAssetInterface`. The actor-level cast will return nullptr without this.

`AProjectCharacter` is legacy. `ADefinitionCharacter` is the modern data-driven path. Only add the interface to `ADefinitionCharacter`.

**Assumption:** all dialogue instigators that need tag-gated options are `ADefinitionCharacter`-based. If any other dialogue-capable actor can be the instigator, that actor must also expose `IGameplayTagAssetInterface`.

- [ ] **DefinitionCharacter.h** -- add `IGameplayTagAssetInterface` to `ADefinitionCharacter` inheritance
- [ ] **DefinitionCharacter.cpp** -- implement `GetOwnedGameplayTags()` delegating to owned ASC. If ASC is missing, return empty tag container (no crash).
- [ ] Verify the owning module for `ADefinitionCharacter` already depends on `GameplayTags`. No new module dependencies expected.

**Why actor-level, not component scan:**
- One authoritative tag view per actor
- Character decides what counts as "owned tags" (ASC today, quest/faction/trust later)
- Dialogue doesn't care where tags come from

**Future aggregation intent (document in code):**
- Today: `GetOwnedGameplayTags()` forwards to ASC
- Later: may aggregate ASC + quest + trust + faction + temporary story state
- No change to ProjectDialogue when that happens

---

## Step 1: ProjectDialogueComponent.cpp

- [ ] Extract inventory logic into `CheckInventoryCondition(AActor&, FDialogueCondition)` static/namespace helper
- [ ] Add `CheckTagCondition(AActor&, FDialogueCondition)` static/namespace helper
- [ ] Refactor `CheckCondition()` to dispatch: inventory -> helper, tag -> helper, else -> warn + false
- [ ] Move `InstigatorActor` null-check above the type dispatch (shared by both branches)

### CheckTagCondition logic

1. `FGameplayTag::RequestGameplayTag(FName(*CondData.Id), false)` -- if invalid, return false
2. `Cast<IGameplayTagAssetInterface>(InstigatorActor)` -- if nullptr, return false
3. `GetOwnedGameplayTags(OwnedTags)`
4. `Exact=true` -> `OwnedTags.HasTagExact(Tag)`, `Exact=false` -> `OwnedTags.HasTag(Tag)`
5. `Quantity` ignored for tags (boolean presence)

### Hierarchy match semantics (document for content authors)

- `exact=true`: only `Quest.ElderTrust` matches `Quest.ElderTrust`
- `exact=false`: query `Quest.Elder` matches owned `Quest.Elder.Trust` (parent-family query)
- NOT bidirectional: owned `Quest.Elder` does NOT match query `Quest.Elder.Trust`

---

## Step 2: Dependency Cleanup (ProjectDialogue.Build.cs)

- [ ] Remove `GameplayAbilities` from `PrivateDependencyModuleNames`
- [ ] Remove `#include "AbilitySystemInterface.h"` from ProjectDialogueComponent.cpp
- [ ] Remove `#include "AbilitySystemComponent.h"` from ProjectDialogueComponent.cpp
- [ ] Add `#include "GameplayTagAssetInterface.h"`

`ProjectDialogue` must depend directly on `GameplayTags` if it includes `GameplayTagAssetInterface.h`. Do not rely on transitive module dependencies. Verify `GameplayTags` is listed in `PublicDependencyModuleNames` (it is today).

---

## Step 3: ProjectDialogueTypes.h (doc-only)

- [ ] Update `Type` comment: `"inventory"` or `"tag"` (remove `(future)`)
- [ ] Update `Id` comment: item ObjectDefinition id (inventory) or gameplay tag name (tag)
- [ ] Update `Quantity` comment: note ignored for tag conditions
- [ ] Update `Exact` comment: exact id vs prefix (inventory), exact tag vs hierarchy match (tag)
- [ ] Update struct doc block with tag JSON example

---

## Step 4: dialogue.schema.json

- [ ] Use `oneOf` discriminated schema for condition:
  - inventory variant: `type=const "inventory"`, `id`, `quantity`, `exact`
  - tag variant: `type=const "tag"`, `id`, `exact` (no `quantity`)
- [ ] Update descriptions for content authors

---

## Error Handling

Runtime rule for bad content:
- Unknown condition type -> warn and return false
- Invalid tag id -> return false, no crash, no ensure; optional low-noise warning
- Missing tag provider on actor -> return false silently
- Missing instigator -> return false silently

Do not add warn-once bookkeeping in this change. Avoid repeated log spam by keeping non-critical failures silent or low-noise. Prefer schema/content validation for authoring mistakes.

When warning is emitted, include at minimum: owner name, condition type, condition id.

---

## Non-Goals

- No tag mutation actions (`grant_tag`, `remove_tag`) -- separate feature
- No condition plugin registry -- only two types, premature abstraction
- No `FDialogueCondition` struct redesign -- current shape is correct
- No quest system integration -- that grants tags, dialogue only reads them

---

## Acceptance Criteria

### Inventory regression
- [ ] Existing inventory exact match still works
- [ ] Existing inventory prefix match still works

### Tag exact positive
- [ ] Owned `Quest.ElderTrust` + condition `{type:tag, id:Quest.ElderTrust, exact:true}` -> true

### Tag exact negative (proves exact stays exact)
- [ ] Owned `Quest.Elder.Trust` + condition `{type:tag, id:Quest.Elder, exact:true}` -> false

### Tag hierarchy positive
- [ ] Owned `Quest.Elder.Trust` + condition `{type:tag, id:Quest.Elder, exact:false}` -> true

### Tag hierarchy negative (direction matters)
- [ ] Owned `Quest.Elder` + condition `{type:tag, id:Quest.Elder.Trust, exact:false}` -> false

### Missing tag provider
- [ ] Actor without `IGameplayTagAssetInterface` -> false

### Invalid tag name
- [ ] Unregistered tag id -> false (no crash, no ensure)

### Dependency
- [ ] `ProjectDialogue` compiles without `GameplayAbilities` in Build.cs
- [ ] Schema validates both inventory and tag conditions
- [ ] Schema rejects `quantity` on tag variant (oneOf strictness)

---

## Precedent

ProjectMind already uses `Cast<IGameplayTagAssetInterface>` at [MindServiceImpl.cpp:173](Plugins/Gameplay/ProjectMind/Source/ProjectMind/Private/Services/MindServiceImpl.cpp#L173). Same pattern, proven in ALIS.

---

## Key Architectural Rule

```
ProjectDialogue evaluates "tag" conditions by asking the instigator actor
for its owned gameplay tags through IGameplayTagAssetInterface,
then applying Exact as exact-vs-hierarchy match.

Dialogue depends on GameplayTags, not GameplayAbilities.
The actor decides what its effective tag set is.
```
