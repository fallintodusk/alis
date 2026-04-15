# Fix Grandpa Dialogue (Tasks #5 + #6)

## Architecture: Modular Dialogue Files (Industry Gold Standard)

Pattern: **CD Projekt / Larian** — separate files per conversation context, chained via `dialogue.set_tree`.

| Approach | Used By | Pros | Cons |
|----------|---------|------|------|
| **Multiple files** | Witcher 3, BG3, Mass Effect, ALIS | Easy to test, no merge conflicts, each file = one visit | More files to manage |
| Single file, conditional roots | Fallout NV, Skyrim | Everything in one place | Unmaintainable at scale, merge hell |
| Single file, flow fragments | Disco Elysium | Maximum flexibility | Requires custom tooling |

**Rule: one file = one reason the player is talking to this NPC right now.**

## Dialogue Chain

```
Entry → Inside → AfterBread → Cigarettes → FamilyApartment → FamilyApartment_Return → AfterKey → Idle
```

8 files, each 3-12 nodes. Each = one conversation context.

## File Map

| File | Purpose | Transitions To |
|------|---------|---------------|
| `DLG_GrandPa_Entry` | Door encounter. Water trade → 3-step door open | Inside |
| `DLG_GrandPa_Inside` | Bread offer. "Go eat, come back" or skip to quest | AfterBread or Cigarettes |
| `DLG_GrandPa_Inside_AfterBread` | Hero returns after eating. Story + cigarettes quest assignment | Cigarettes |
| `DLG_GrandPa_Cigarettes` | Hero returns with cigs. Friend's fate, mechanic hint, crowbar | FamilyApartment |
| `DLG_GrandPa_FamilyApartment` | Hero returns from mechanic. Backpack deal, 20 cigs for key | FamilyApartment_Return |
| `DLG_GrandPa_FamilyApartment_Return` | Hero returns with 20 cigs. Short trade dialogue | AfterKey |
| `DLG_GrandPa_AfterKey` | Grandpa asks about backpack. Did you find it? | Idle |
| `DLG_GrandPa_Idle` | Final state. Small talk, no more quests | stays on Idle |

## Player Journey

```
1. Hero finds grandpa's door (locked)
2. Trades water → door opens (Entry)
3. Grandpa offers bread → hero eats (Inside)
4. Grandpa tells what happened, sends hero to friend (AfterBread)
5. Hero finds drug den, no friend, finds crowbar + cigarettes
6. Returns, reports bad news, trades 3 cigs (Cigarettes)
7. Grandpa hints at mechanic's flimsy door
8. Hero breaks into mechanic's apartment with crowbar
9. Returns, can't carry everything (FamilyApartment)
10. Grandpa remembers couple upstairs with backpack, wants 20 cigs
11. Hero finds 20 cigs, trades for key (FamilyApartment_Return)
12. Door unlocks remotely via remote: action
13. Hero finds backpack, returns to grandpa (AfterKey)
14. Grandpa says goodbye (Idle)
```

## Blocking Issues

- [ ] `DLG_GrandPa_FamilyApartment.json` -- `lock_reminder` node needs `dialogue.set_tree` to `DLG_GrandPa_AfterKey` (temporarily removed, action is empty `[]`). Restore after creating `DLG_GrandPa_AfterKey.json`.
- [ ] `DLG_GrandPa_FamilyApartment_Return.json` -- listed in File Map but not yet created
- [ ] `DLG_GrandPa_Idle.json` -- listed in File Map but not yet created

## Code Changes Made

### ProjectDialogueComponent.cpp
- **Quantity conditions**: `inventory:Cigarette*>=20` checks for at least N items
- **Remote actions**: `remote:Scenario.FamilyApartmentDoor:lock.unlock` dispatches actions to actors by tag
- **Hidden options**: unmet conditions hide the option (W_DialoguePanel.cpp) instead of showing "(Unavailable)"

### Condition Formats
```
inventory:WaterBottle*       — has any water bottle (wildcard prefix)
inventory:Cigarette*>=3      — has at least 3 cigarettes
inventory:Cigarette*>=20     — has at least 20 cigarettes
inventory:Medkit*            — has any medkit (proof player visited handyman's apartment)
inventory:Crowbar*           — has any crowbar
```

### Action Formats
```
inventory.consume:Cigarette*:3                              — remove 3 cigarettes
inventory.consume:Cigarette*:20                             — remove 20 cigarettes
lock.unlock                                                 — unlock ActionTarget door
motion.open                                                 — open ActionTarget door
remote:Scenario.FamilyApartmentDoor:lock.unlock             — unlock remote door by tag
dialogue.set_tree:/ProjectObject/.../DLG_Name.DLG_Name      — switch dialogue tree
```

## Design Decisions

- **Trade, not rescue** — grandpa is in control, trades info for resources
- **3-step door opening** — consume water → unlock → open (each on player advance)
- **Hero finds addicts by accident** — grandpa never mentions them
- **Crowbar discovery** — grandpa says door is "flimsy" and "would fly out with one blow" — player connects crowbar themselves
- **Handyman hint is vague** — `go_mechanic` says "a buddy of mine, worked at the factory, always hoarded supplies" — no specific items mentioned
- **Backpack motivation** — hero can't carry mechanic's loot (no Medkit condition — option always available), grandpa remembers couple with backpack
- **Quantity gating** — `>=3` and `>=20` conditions ensure player has enough before trading
- **Key via inventory.give** — grandpa gives key to player's inventory after 20-cig trade
- **Ending** — hero tells grandpa he's equipped, plans to stay at handyman's apartment first, then move on. "Staying in one place too long is death."

## set_tree Fix

`DLG_GrandPa_Inside`: `dialogue.set_tree` moved from `skip_to_quest` to `quest_confirm`. Previously, exiting dialogue mid-conversation (before quest_assignment) would prematurely switch to Cigarettes tree.

## Door Dialogues (Inner-Voice Style)

Two self-contained door dialogues triggered by same-actor sibling notify pattern
(LockableComponent → IActorWatchEventListener → DialogueComponent on same actor):

| File | Condition | Actions |
|------|-----------|---------|
| DLG_Door_Handyman | `inventory:Crowbar*` | `lock.unlock`, `motion.open` |
| DLG_Door_FamilyApartment | `inventory:KeyPlayerApartment*` | `lock.unlock`, `motion.open`, `inventory.consume:KeyPlayerApartment*` |

Text is inner monologue (no speaker field), not NPC speech.

## Condition Option Visibility

Options with unmet conditions are **hidden** (W_DialoguePanel.cpp). Player doesn't see options they can't use — no spoilers for undiscovered items.

## Balance Approach

Calorie values stay as-is (BreadBig=1000, FishPaste=765, StewedBeef=785). Balance is controlled by **quantity and placement** of items on the map, not by adjusting individual calorie values. See `quest_resource_placement.md` for item placement and metabolic tracking.
