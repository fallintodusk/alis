# ProjectCombat TODO

## Current State: Registered with FFeatureRegistry

Feature registers on module startup. GameMode calls init lambda with pawn context.

---

## Future

### Implement Init Lambda
- [ ] Attach `UCombatComponent` to `Context.Pawn`
- [ ] Parse `Context.ConfigJson` for damage multipliers, etc.
- [ ] Set up combat subsystem if needed

### Content
- [ ] Add combat-related assets when ready
- [ ] Configure `PrimaryAssetTypesToScan` if using AssetManager

---

## Architecture

```cpp
// Module startup - REGISTRATION ONLY
FFeatureRegistry::RegisterFeature(TEXT("Combat"), [](const FFeatureInitContext& Context)
{
    // Called by GameMode after pawn spawn
    APawn* Pawn = Context.Pawn;
    UCombatComponent* Combat = NewObject<UCombatComponent>(Pawn);
    Pawn->AddInstanceComponent(Combat);
    Combat->RegisterComponent();
});
```

---

## See Also

- `Plugins/Gameplay/ProjectFeature/README.md` - Registry design
