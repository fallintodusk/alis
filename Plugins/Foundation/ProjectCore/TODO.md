# ProjectCore TODO

## Service Interfaces (DIP Compliance)

### ILoadingService Interface

- [ ] Create `Public/Services/ILoadingService.h`
- [ ] Define interface methods:
  - `virtual TSharedPtr<ILoadingHandle> StartLoad(const FLoadRequest& Request) = 0`
  - `virtual bool CancelActiveLoad(bool bForce = false) = 0`
  - `virtual bool IsLoadInProgress() const = 0`
  - `virtual TSharedPtr<ILoadingHandle> GetActiveLoadHandle() const = 0`
- [ ] Add comprehensive documentation:
  - Explain DIP pattern (why this interface exists)
  - Document architecture: Features -> ILoadingService ← Systems
  - Add usage examples
- [ ] Add convenience accessor: `GetLoadingService()` using ServiceLocator

**Why this interface exists:**

Dependency Inversion Principle (DIP) - the "D" in SOLID.

Without this interface:
```
ProjectMenuMain (Feature, high-level)
    ↓ BAD: depends directly on
UProjectLoadingSubsystem (System, low-level)
```

With this interface:
```
ProjectMenuMain (Feature, high-level)
    ↓ depends on abstraction
ILoadingService (ProjectCore interface)
    ↑ implemented by
UProjectLoadingSubsystem (System, low-level)
```

**Benefits:**
- Features can trigger loading without knowing ProjectLoading exists
- Can swap implementations or mock for testing
- Proper tier separation (Features don't depend on Systems)
- Follows SOLID principles

**Usage Example:**
```cpp
// In ProjectMenuMain (Feature plugin)
void UProjectMapBrowserWidget::OnNewGameClicked()
{
    // Resolve ILoadingService from ServiceLocator
    TSharedPtr<ILoadingService> LoadingService =
        FProjectServiceLocator::Resolve<ILoadingService>();

    if (!LoadingService)
    {
        UE_LOG(LogMenu, Error, TEXT("ILoadingService not available"));
        return;
    }

    // Build load request
    FLoadRequest Request;
    Request.MapAssetId = FPrimaryAssetId(TEXT("ProjectWorld"), TEXT("GameplayMap"));
    Request.LoadMode = ELoadMode::SinglePlayer;

    // Trigger loading
    TSharedPtr<ILoadingHandle> Handle = LoadingService->StartLoad(Request);

    // Subscribe to completion
    // (via UProjectLoadingSubsystem delegates - accessible through subsystem)
}
```

**References:**
- [RESPONSIBILITIES.md](../../../docs/architecture/RESPONSIBILITIES.md) - DIP pattern explanation
- [plugin_architecture.md](../../../docs/architecture/plugin_architecture.md) - Loading flow
- Lyra uses similar pattern for Experience queries (though we don't use Lyra's Experience system)
