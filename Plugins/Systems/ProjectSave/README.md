# ProjectSave (Save System)

Purpose
- Save/load for player profiles, progress, and settings.

Responsibilities
- `UProjectSaveGame` data, serialization helpers, and profile slots.
- Apply settings to runtime systems on load.
- Provide PluginBinaryData storage for feature plugin save blobs.

Non-Responsibilities
- No networking/auth. Use server-only code in server targets only.
- Not responsible for session management (see ProjectSession).

Loading Phase
- `PostConfigInit`.

Dependencies
- `ProjectCore` and Engine subsystems only.

Usage (feature plugin blob)
Example: inventory writes a binary blob into the save, then the save is written to disk.

```cpp
if (UProjectSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UProjectSaveSubsystem>())
{
    if (UProjectInventoryComponent* Inventory = Pawn->FindComponentByClass<UProjectInventoryComponent>())
    {
        Inventory->SaveToSaveSubsystem(SaveSubsystem, FName(TEXT("ProjectInventory.Inventory")));
        SaveSubsystem->SaveGame(UProjectSaveSubsystem::GetAutoSaveSlotName());
    }
}
```

Validation
- Build: `scripts/ue/build/build.bat AlisEditor Win64 Development`
- UHT/Assets: `scripts/ue/check/validate_*.bat`
- Tests: Run ProjectSaveTests in Session Frontend -> Automation.
