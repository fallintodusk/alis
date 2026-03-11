# ProjectMenuMain TODO

## Architecture Compliance (DIP)

### Remove Direct ProjectLoading Dependency

- [ ] Remove `ProjectLoading` from `ProjectMenuMain.uplugin` dependencies:
  ```json
  "Plugins": [
      { "Name": "ProjectCore", "Enabled": true },
      { "Name": "ProjectUI", "Enabled": true },
      { "Name": "ProjectSettingsUI", "Enabled": true }
      // REMOVE: { "Name": "ProjectLoading", "Enabled": true }
  ]
  ```
- [ ] Remove `ProjectLoading` from `ProjectMenuMain.Build.cs` dependencies:
  ```csharp
  PublicDependencyModuleNames.AddRange(new string[] {
      "ProjectCore",
      "ProjectUI",
      // REMOVE: "ProjectLoading"
  });
  ```
- [ ] Update includes in source files:
  - Replace: `#include "ProjectLoadingSubsystem.h"`
  - With: `#include "Services/ILoadingService.h"`
- [ ] Update code to use `ILoadingService` via ServiceLocator:
  - Replace: `GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>()`
  - With: `FProjectServiceLocator::Resolve<ILoadingService>()`

### Implement Flow Coordination

#### "New Game" Button Flow

- [ ] In button click handler:
  ```cpp
  void UProjectMapBrowserWidget::OnNewGameClicked()
  {
      // Resolve ILoadingService from ServiceLocator (DIP)
      TSharedPtr<ILoadingService> LoadingService =
          FProjectServiceLocator::Resolve<ILoadingService>();

      if (!LoadingService)
      {
          // Handle error: show error dialog to user
          UE_LOG(LogMenu, Error, TEXT("ILoadingService not available"));
          ShowErrorDialog(TEXT("Loading system unavailable"));
          return;
      }

      // Build FLoadRequest
      FLoadRequest Request;
      Request.MapAssetId = SelectedMapId; // From UI selection
      Request.LoadMode = ELoadMode::SinglePlayer;

      // Trigger loading
      TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);

      // Subscribe to loading delegates for feedback
      // (Need to get subsystem for delegates - interface doesn't expose them)
      if (UProjectLoadingSubsystem* LoadingSys = GetGameInstance()->GetSubsystem<UProjectLoadingSubsystem>())
      {
          LoadingSys->OnCompleted.AddDynamic(this, &UProjectMapBrowserWidget::OnLoadCompleted);
          LoadingSys->OnFailed.AddDynamic(this, &UProjectMapBrowserWidget::OnLoadFailed);
      }
  }
  ```

#### "Continue" (Load Game) Flow

- [ ] Query ProjectSave for last save slot
- [ ] Build `FLoadRequest` from save metadata:
  ```cpp
  void UProjectMainMenuWidget::OnContinueClicked()
  {
      // Query ProjectSave
      UProjectSaveSubsystem* SaveSys = GetGameInstance()->GetSubsystem<UProjectSaveSubsystem>();
      UProjectSaveGame* LastSave = SaveSys ? SaveSys->GetLastSaveSlot() : nullptr;

      if (!LastSave)
      {
          ShowErrorDialog(TEXT("No save game found"));
          return;
      }

      // Build load request from save
      FLoadRequest Request;
      Request.MapAssetId = LastSave->CurrentMapId;
      Request.LoadMode = ELoadMode::SinglePlayer;
      Request.CustomOptions.Add(TEXT("SaveSlot"), LastSave->SlotName);

      // Trigger loading
      TSharedPtr<ILoadingService> LoadingService =
          FProjectServiceLocator::Resolve<ILoadingService>();
      if (LoadingService)
      {
          LoadingService->StartLoad(Request);
      }
  }
  ```

#### Error Handling

- [ ] Implement `OnLoadFailed` handler:
  - Show error dialog with retry/cancel options
  - Log error for diagnostics
  - Return to menu on cancel
- [ ] Implement `OnLoadCompleted` handler:
  - Clean up loading UI
  - Log success for telemetry

### Optional: JSON Catalog for Data-Driven UI

- [ ] Design catalog schema: `Content/Catalog/menu.json`
  ```json
  {
      "schema": "MenuCatalog/v1",
      "worlds": [
          {
              "id": "main",
              "name": "Main World",
              "mapPath": "/Game/Maps/Main",
              "thumbnail": "/Game/UI/T_Main",
              "tags": ["default"]
          }
      ],
      "modes": [
          {
              "id": "survival",
              "name": "Survival",
              "gameMode": "/Script/Alis.SurvivalGM",
              "desc": "Hardcore survival",
              "tags": ["solo", "coop"]
          }
      ],
      "defaults": {
          "world": "main",
          "mode": "survival"
      }
  }
  ```
- [ ] Implement catalog loader:
  - Load on widget initialization
  - Hot-reload on file change (editor only)
  - Periodic poll (builds)
  - Merge: `Saved/` > `Plugins/**/Content/` > `Content/`
- [ ] Update map browser UI to read from catalog
- [ ] Add `OnCatalogReloaded` delegate for UI refresh

**Why DIP Matters:**

Before (BAD):
```
ProjectMenuMain (Feature, Tier 3)
    ↓ direct dependency
ProjectLoading (System, Tier 2)
```

After (GOOD):
```
ProjectMenuMain (Feature, Tier 3)
    ↓ depends on abstraction
ILoadingService (ProjectCore interface, Tier 1)
    ↑ implemented by
ProjectLoading (System, Tier 2)
```

**Benefits:**
- ✅ ProjectMenuMain doesn't know ProjectLoading exists
- ✅ Can mock ILoadingService for testing
- ✅ Proper tier separation (Features → Foundation, not Features → Systems)
- ✅ Follows SOLID principles

**Current State:**

- Dependency declared in `.uplugin` but not used in code (stub integration)
- One integration test references old pattern (needs update)

**Testing:**
- [ ] Update `MenuFlowPIETest.cpp` to use ILoadingService
- [ ] Verify menu builds without ProjectLoading dependency
- [ ] Integration test: New Game flow end-to-end
- [ ] Integration test: Continue flow with save data

**References:**
- [RESPONSIBILITIES.md](../../docs/architecture/RESPONSIBILITIES.md) - Feature tier boundaries
- [plugin_architecture.md](../../docs/architecture/plugin_architecture.md) - Menu architecture
