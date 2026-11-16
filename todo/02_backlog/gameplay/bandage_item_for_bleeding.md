# City17: Separate Bandage Item

## Problem

Medkit currently serves as the bleeding cure for City17 sniper zone (temporary). Need a dedicated Bandage item with its own mesh so Medkit can stay as a bigger heal item.

## What to do

1. Create or source a bandage mesh asset (SM_Bandage)
2. Create `Bandage.json` in `ProjectObject/Content/HumanMade/Consumables/Vital/Health/Medical/Bandage/`
3. Bandage: bleeding-only cure (`SetByCaller.Bleeding: -1.0`), no condition heal
4. Medkit: remove bleeding cure from magnitudes, keep as bigger heal item
5. Replace Medkit pickup in City17 sniper area with Bandage

## References

- Current Medkit with magnitudes: `ProjectObject/Content/HumanMade/Consumables/Vital/Health/Medical/Medkit/Medkit.json`
- Sniper zone: `ProjectObject/Content/Environment/Hazard/SniperZone.json`
- Schema: `ProjectObject/Data/Schemas/object.schema.json` (magnitudes + consumeOnUse verified)
