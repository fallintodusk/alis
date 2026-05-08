# Grandpa Dialogue

## Flow (4 files, wired end-to-end)

```
Entry --water--> Inside --3 cigs--> Cigarettes --20 cigs--> FamilyApartment --key--> $end
```

| File | Role |
|------|------|
| [DLG_GrandPa_Entry](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Entry.json) | Door encounter. Water trade unlocks + opens door. |
| [DLG_GrandPa_Inside](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Inside.json) | Inside the apartment. World-talk + first quest (3 cigs). |
| [DLG_GrandPa_Cigarettes](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Cigarettes.json) | Hero returns with 3 cigs. Hands out neighbor task. |
| [DLG_GrandPa_FamilyApartment](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_FamilyApartment.json) | 20 cigs traded for `KeyPlayerApartment`. Backpack lives in the upstairs room. |

`KeyPlayerApartment` is granted in [FamilyApartment:40](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_FamilyApartment.json#L40); 3-cig consume in [Cigarettes:43](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Cigarettes.json#L43); 20-cig consume in [FamilyApartment:34](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_FamilyApartment.json#L34).

## Door Dialogues (inner-voice, sibling-notify pattern)

| File | Condition | Actions |
|------|-----------|---------|
| `DLG_Door_Handyman` | `inventory:Crowbar*` | `lock.unlock`, `motion.open` |
| `DLG_Door_FamilyApartment` | `inventory:KeyPlayerApartment*` | `lock.unlock`, `motion.open`, `inventory.consume:KeyPlayerApartment*` |

No speaker field -- inner monologue, not NPC speech.

## Dropped Files (do not recreate)

`DLG_GrandPa_FamilyApartment_Return`, `DLG_GrandPa_AfterKey`, `DLG_GrandPa_Idle`, `DLG_GrandPa_Inside_AfterBread` were planned/existed in earlier drafts. Their roles collapse into the 4 files above:
- "Return with 20 cigs" = `thanks_cigs` in FamilyApartment.
- "Ask about backpack" = narrative beat handled by player finding it upstairs.
- "Idle re-entry" = FamilyApartment greeting with the give-twenty option auto-hidden once cigs drop below 20.
- **Inside_AfterBread** (deleted) = was a duplicate of Inside's quest portion reachable only via an eat-or-talk fork. Eat-fork removed; food in kitchen is now flavor text. Orphaned `.uasset` should be cleaned up via UE editor on next reimport.

## Outstanding Work

### 1. Text rewrite (atmosphere + character arc)

Current text is functional but flat. Rewrite passes:

- **Old voice.** Weathered, slow, plain, and older in rhythm - not theatrical archaic speech. See [docs/style/dialogues.md#old-voice--what-it-means](../../docs/style/dialogues.md) for the full definition. Short sentences, occasional fragments.
- **Less exposition.** Cut the "sirens, shaking, screaming people scattered like rats" beats -- player already knows the world ended. Imply, don't recap.
- **Kindness arc.** Grandpa is wary at the door, transactional after entry, *warmer after the 3-cig trade* (relief, gratitude, treats hero like a person), and quietly affectionate by the 20-cig key handover. He is not angry, not evil, not threatening -- guarded turning to grandfatherly.
- **Trim wordiness.** Each node should land in one or two breaths. Strip filler ("Just don't forget to lock the door when you leave." -> "Lock up after.").

Files touched: all 5 dialogue JSONs, both door dialogues if any text is grandpa-adjacent.

### 2. Possible branch refactor

If the kindness arc is hard to express inside the current node graph (e.g. Cigarettes greeting still sounds suspicious), consider splitting greeting nodes per emotional state instead of layering tone onto existing branches. Decide during the rewrite -- don't pre-refactor.

### 3. Cigarette gating (deferred - not currently broken)

The 3-cig and 20-cig conditions use `"exact": true, "id": "Cigarette"` ([Cigarettes:13-18](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_Cigarettes.json#L13-L18), [FamilyApartment:13-18](../../Plugins/Resources/ProjectObject/Content/Human/GrandPa/DLG_GrandPa_FamilyApartment.json#L13-L18)), which only matches loose `Cigarette` items and ignores `CigarettePacket`. Currently fine - `CigarettePacket` is not placed in any level. Revisit if/when packets get level placement: either switch to `"exact": false` (packet counts as 1 cig - undervalued) or give `CigarettePacket` a split-on-consume capability that expands to ~20 `Cigarette` items.
