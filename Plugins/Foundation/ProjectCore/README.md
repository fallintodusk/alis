# ProjectCore Plugin - Abstraction Layer

Foundation module that owns stable interfaces, shared types, and light utilities for the whole project.

## Start Here

- Architecture router: [../../../docs/architecture/README.md](../../../docs/architecture/README.md)
- Core principles: [../../../docs/architecture/principles.md](../../../docs/architecture/principles.md)
- Public interfaces: [Source/ProjectCore/Public/Interfaces/README.md](Source/ProjectCore/Public/Interfaces/README.md)

## Role
- Single place for abstractions (DIP guardrail).
- Data-only experience descriptors and registry live here so plugins do not depend on ProjectLoading.
- No AssetManager calls and no content logic in this module.

## What It Provides
- Interfaces (`Interfaces/`) for loading, session, feature activation, etc.
- Shared types (`Types/`) used by systems and features.
- Experience descriptors (`Experience/ProjectExperienceDescriptorBase`) and registry/registration helpers.
- Service locator for runtime interface lookups.
- Config helpers for consistent INI access.
- ProjectCore log category (`LogProjectCore`) for diagnostics (plugins own their own logs).

## Gameplay Tags Ownership

- Project-owned gameplay tags are declared/defined as native tags in:
  - `Source/ProjectCore/Public/ProjectGameplayTags.h`
  - `Source/ProjectCore/Private/ProjectGameplayTags.cpp`
- `Config/DefaultGameplayTags.ini` is reserved for engine sample tags, imported tags,
  redirects, and plugin-local non-native tags.

## Architecture (DIP)
```
High-Level Systems (ProjectLoading, ProjectSession, ...)
    ^ depend on abstractions
ProjectCore (interfaces and data types)
    v implemented by
Low-Level Systems (OrchestratorCore, BootROM, ...)
```

## Logging (Policy)
- Each plugin defines its own log category.
- ProjectCore only provides `LogProjectCore` for its own diagnostics.

Version: 1.0.0 | Category: Framework.Core | Loading Phase: PostConfigInit

## Service Locator & Config Helpers

### `FProjectServiceLocator`

- Template registry for module-level services (loading orchestrators, feature registries, save facades).
- Core module clears registrations on startup/shutdown, preventing hot-reload leaks.
- Recommended usage:
  - Register interfaces during module startup.
  - Resolve in subsystem initialization (never constructors) and guard with `IsRegistered`.
  - Keep stored objects lightweight; expose world-facing functionality via interfaces.

```cpp
// Loading plugin startup
void FProjectLoadingModule::StartupModule()
{
	TSharedRef<FProjectLoadingService> Service = MakeShared<FProjectLoadingService>();
	FProjectServiceLocator::Register<FProjectLoadingService>(Service);
}

// UI plugin needs loading stats
void UProjectLoadingWidget::InitializeWidget()
{
	if (const TSharedPtr<FProjectLoadingService> Loading = FProjectServiceLocator::Resolve<FProjectLoadingService>())
	{
		BindToLoadingEvents(*Loading);
	}
	else
	{
		PROJECT_LOG_WARNING(UI, "Loading service not available yet");
	}
}

// Chaining: FeatureRegistry depends on Loading service
void FProjectFeatureRegistryModule::StartupModule()
{
	TSharedRef<FProjectFeatureRegistryService> Registry = MakeShared<FProjectFeatureRegistryService>();

	// Resolve dependency after both modules have booted
	if (const TSharedPtr<FProjectLoadingService> Loading = FProjectServiceLocator::Resolve<FProjectLoadingService>())
	{
		Registry->AttachLoadingService(Loading);
	}

	FProjectServiceLocator::Register<FProjectFeatureRegistryService>(Registry);
}

// Third service can pull the registry later
void FProjectSessionService::Initialize()
{
	if (const TSharedPtr<FProjectFeatureRegistryService> Registry = FProjectServiceLocator::Resolve<FProjectFeatureRegistryService>())
	{
		BuildSessionFeatures(*Registry);
	}
}
```

**Load-order tips**

- Ensure plugin `.uplugin` files declare `Plugins` dependencies so Unreal loads them in the right order.
- When one service needs another, resolve lazily (during `Initialize`/`OnWorldReady`) and handle null results; the dependency may register after hot-reload.
- For circular cases, split into interface registration + `LateInitialize` call once all services exist to avoid hard dependencies.

### `FProjectConfigHelpers`

- Thin wrapper over `GConfig` providing consistent getters for strings, ints, floats, bools, arrays, and whole sections.
- Defaults to `GGameIni` but allows per-call overrides (e.g., `GEngineIni`).
- Keeps configuration access centralized for logging/validation hooks.

```cpp
// FeatureRegistry plugin reading configuration
void FProjectFeatureRegistry::LoadConfig()
{
	TArray<FString> FeatureIds;
	if (FProjectConfigHelpers::GetArray(TEXT("ProjectFeatures"), TEXT("Enabled"), FeatureIds))
	{
		EnabledFeatureIds = FeatureIds;
	}
	else
	{
		PROJECT_LOG_WARNING(Features, "No features configured; using defaults");
	}
}
```
