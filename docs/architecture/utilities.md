# ProjectCore Utility Classes

Helper utilities and common patterns for the ProjectCore infrastructure.

**Location:** `Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/`

---

## FProjectValidationHelpers

**File:** `ProjectValidationHelpers.h`

Static validation helper utilities that reduce boilerplate for common validation patterns. All methods use `TArray<FText>& OutErrors` for consistent error collection.

### Basic Type Validation

```cpp
#include "ProjectValidationHelpers.h"

TArray<FText> Errors;

// Validate FName
FName RegionId = FName(TEXT("Downtown"));
if (!FProjectValidationHelpers::IsValidName(RegionId, Errors, FText::FromString(TEXT("RegionId"))))
{
    // RegionId is NAME_None
    UE_LOG(LogTemp, Error, TEXT("%s"), *Errors[0].ToString());
}

// Validate FText
FText Description = Manifest->GetDescription();
if (!FProjectValidationHelpers::IsValidText(Description, Errors))
{
    // Description is empty
}

// Validate FString
FString SessionData = Request.SessionData;
if (!FProjectValidationHelpers::IsValidString(SessionData, Errors, FText::FromString(TEXT("SessionData"))))
{
    // SessionData is empty
}
```

### Asset Path Validation

```cpp
// Validate FSoftObjectPath
FSoftObjectPath MapPath = FSoftObjectPath(TEXT("/Game/Maps/TestMap"));
if (!FProjectValidationHelpers::IsValidSoftObjectPath(MapPath, Errors, FText::FromString(TEXT("MapPath"))))
{
    // MapPath is null
}

// Validate TSoftObjectPtr
TSoftObjectPtr<UWorld> WorldAsset;
if (!FProjectValidationHelpers::IsValidSoftObjectPtr(WorldAsset, Errors, FText::FromString(TEXT("WorldAsset"))))
{
    // WorldAsset is null
}

// Validate UObject pointer
UProjectWorldManifest* Manifest = GetManifest();
if (!FProjectValidationHelpers::IsValidObject(Manifest, Errors, FText::FromString(TEXT("Manifest"))))
{
    // Manifest is null or pending kill
}
```

### Collection Validation

```cpp
// Validate array is not empty
TArray<FName> Features = Request.FeaturesToActivate;
if (!FProjectValidationHelpers::IsValidArray(Features, Errors, FText::FromString(TEXT("Features"))))
{
    // Features array is empty
}

// Validate all array elements with predicate
TArray<FProjectFeatureDependency> Dependencies = Manifest->GetDependencies();

auto Validator = [](const FProjectFeatureDependency& Dep, TArray<FText>& OutErrors) -> bool
{
    return FProjectValidationHelpers::IsValidName(Dep.FeatureId, OutErrors, FText::FromString(TEXT("FeatureId")));
};

if (!FProjectValidationHelpers::ValidateArrayElements(Dependencies, Validator, Errors, FText::FromString(TEXT("Dependencies"))))
{
    // One or more dependencies have invalid FeatureId
    // Errors will contain: "Dependencies[0] FeatureId is None (empty)"
}
```

### Numeric Validation

```cpp
// Validate value is within range
int32 Priority = Request.Priority;
if (!FProjectValidationHelpers::IsInRange(Priority, 0, 100, Errors, FText::FromString(TEXT("Priority"))))
{
    // Priority is out of range [0, 100]
}

// Validate value is positive
float Progress = PhaseState.Progress;
if (!FProjectValidationHelpers::IsPositive(Progress, Errors, FText::FromString(TEXT("Progress"))))
{
    // Progress <= 0
}
```

### Error Collection Pattern

All validation methods append to the same `TArray<FText>& OutErrors` array, allowing multiple validations to accumulate errors:

```cpp
bool FProjectWorldManifest::Validate(TArray<FText>& OutErrors) const
{
    bool bIsValid = true;

    // Validate world root (accumulate errors)
    bIsValid &= FProjectValidationHelpers::IsValidSoftObjectPath(
        WorldRoot, OutErrors, FText::FromString(TEXT("WorldRoot"))
    );

    // Validate description (accumulate more errors)
    bIsValid &= FProjectValidationHelpers::IsValidText(
        Description, OutErrors, FText::FromString(TEXT("Description"))
    );

    // Validate all regions (accumulate errors with indices)
    bIsValid &= FProjectValidationHelpers::ValidateArrayElements(
        Regions,
        [](const FProjectWorldRegionDescriptor& Region, TArray<FText>& OutErrors) {
            return FProjectValidationHelpers::IsValidName(
                Region.RegionId, OutErrors, FText::FromString(TEXT("RegionId"))
            );
        },
        OutErrors,
        FText::FromString(TEXT("Regions"))
    );

    // OutErrors now contains all validation failures:
    //   "WorldRoot is null"
    //   "Description is empty"
    //   "Regions[2] RegionId is None (empty)"

    return bIsValid;
}
```

---

## FProjectAsyncHelpers

**File:** `ProjectAsyncHelpers.h`

Static async utilities for common timer-based patterns. Reduces 5-7 line timer boilerplate to single-line calls.

### Basic Timer Operations

```cpp
#include "ProjectAsyncHelpers.h"

UWorld* World = GetWorld();

// Execute callback after delay (one-shot)
FTimerHandle Handle = FProjectAsyncHelpers::ExecuteAfterDelay(
    World,
    2.0f,  // Delay in seconds
    []() { UE_LOG(LogTemp, Log, TEXT("Delayed execution")); }
);

// Execute on next tick
FTimerHandle TickHandle = FProjectAsyncHelpers::ExecuteNextTick(
    World,
    []() { UE_LOG(LogTemp, Log, TEXT("Next tick execution")); }
);

// Execute repeatedly at interval
FTimerHandle RepeatHandle = FProjectAsyncHelpers::ExecuteRepeating(
    World,
    1.0f,  // Interval in seconds
    []() { UE_LOG(LogTemp, Log, TEXT("Repeating execution")); }
);

// Cancel timer
FProjectAsyncHelpers::CancelTimer(World, Handle);

// Check if timer is active
bool bIsActive = FProjectAsyncHelpers::IsTimerActive(World, Handle);
```

### Thread-Safe Execution

```cpp
// Execute callback on game thread (safe from any thread)
FProjectAsyncHelpers::ExecuteOnGameThread([]() {
    // This always runs on game thread, regardless of calling thread
    UE_LOG(LogTemp, Log, TEXT("Running on game thread"));
});

// Check if on game thread
if (FProjectAsyncHelpers::IsOnGameThread())
{
    // Safe to access UObjects
}

// Execute async on background thread
FProjectAsyncHelpers::ExecuteAsync([]() {
    // Heavy computation on background thread
    // DO NOT access UObjects here
});

// Execute async, then callback on game thread
FProjectAsyncHelpers::ExecuteAsyncThenOnGameThread(
    []() -> FString {
        // Background thread work (no UObject access)
        return LoadFileFromDisk();
    },
    [](FString Result) {
        // Game thread callback (safe UObject access)
        ProcessResult(Result);
    }
);
```

### Retry with Exponential Backoff

```cpp
// Execute callback with exponential backoff delay
// Delay = BaseDelaySeconds * 2^AttemptNumber
// Attempt 0: 2s, Attempt 1: 4s, Attempt 2: 8s

int32 AttemptNumber = 1;  // Second retry
FTimerHandle RetryHandle = FProjectAsyncHelpers::ExecuteWithBackoff(
    World,
    AttemptNumber,
    2.0f,  // Base delay
    [this]() {
        RetryDownload();
    }
);
```

### Practical Example: Content Pack Retry Logic

```cpp
// From ProjectContentPackSubsystem.cpp
void UProjectContentPackSubsystem::RetryDownloadWithBackoff(FName PackId, int32 ChunkId, int32 AttemptNumber)
{
    const float BaseDelay = 2.0f;

    FProjectAsyncHelpers::ExecuteWithBackoff(
        GetWorld(),
        AttemptNumber,
        BaseDelay,
        [this, PackId, ChunkId, AttemptNumber]() {
            // Retry download after exponential backoff delay
            InitiateDownload(PackId, ChunkId, AttemptNumber + 1);
        }
    );
}
```

---

## ProjectErrorCodes Namespace

**File:** `ProjectErrorCodes.h`

Centralized error code definitions organized by loading phase.

**Range:** `100-999` (codes outside this range are invalid)

### Phase Ranges

```cpp
#include "ProjectErrorCodes.h"

// Phase-specific error codes
PhaseState.ErrorCode = ProjectErrorCodes::ResolveAssets::ManifestNotFound;       // 101
PhaseState.ErrorCode = ProjectErrorCodes::MountContent::DownloadFailed;          // 201
PhaseState.ErrorCode = ProjectErrorCodes::PreloadAssets::AssetLoadFailed;        // 301
PhaseState.ErrorCode = ProjectErrorCodes::ActivateFeatures::FeatureNotFound;     // 401
PhaseState.ErrorCode = ProjectErrorCodes::Travel::TravelFailed;                  // 502
PhaseState.ErrorCode = ProjectErrorCodes::Warmup::ShaderCompileFailed;           // 601

// General system errors
PhaseState.ErrorCode = ProjectErrorCodes::System::Cancelled;                     // 801
PhaseState.ErrorCode = ProjectErrorCodes::System::Timeout;                       // 802
PhaseState.ErrorCode = ProjectErrorCodes::System::OutOfMemory;                   // 803
```

### Error Code Validation

```cpp
// Validate error code is in valid range [100, 999]
if (ProjectErrorCodes::IsValidErrorCode(ErrorCode))
{
    UE_LOG(LogProjectLoading, Error, TEXT("Phase failed with error code: %d"), ErrorCode);
}
else
{
    UE_LOG(LogProjectLoading, Warning, TEXT("Invalid error code: %d (expected 100-999)"), ErrorCode);
}
```

### Complete Error Code List

**31 error codes defined across 7 ranges:**

**ResolveAssets (100-199):**
- `101` - ManifestNotFound
- `102` - InvalidManifest
- `103` - AssetManagerNotReady
- `104` - DependencyResolutionFailed
- `105` - CircularDependency

**MountContent (200-299):**
- `201` - DownloadFailed
- `202` - MountFailed
- `203` - ChunkNotCached
- `204` - PakCorrupted
- `205` - InsufficientDiskSpace
- `206` - NetworkTimeout

**PreloadAssets (300-399):**
- `301` - AssetLoadFailed
- `302` - StreamingTimeout
- `303` - InvalidAssetPath
- `304` - AssetTypeMismatch

**ActivateFeatures (400-499):**
- `401` - FeatureNotFound
- `402` - ActivationFailed
- `403` - DependencyMissing
- `404` - FeatureIncompatible

**Travel (500-599):**
- `501` - MapNotFound
- `502` - TravelFailed
- `503` - ConnectionLost
- `504` - ServerFull

**Warmup (600-699):**
- `601` - ShaderCompileFailed
- `602` - StreamingWarmupFailed
- `603` - WarmupTimeout

**System (800-899):**
- `800` - UnknownError
- `801` - Cancelled
- `802` - Timeout
- `803` - OutOfMemory
- `804` - InvalidState

**See:** [Loading Pipeline Error Handling](../systems/loading_pipeline.md#error-code-reference)

---

## Best Practices

### Validation Pattern

[OK] **DO:**
```cpp
bool ValidateMyData(TArray<FText>& OutErrors) const
{
    bool bIsValid = true;

    // Accumulate all errors (don't early-return)
    bIsValid &= FProjectValidationHelpers::IsValidName(Name, OutErrors);
    bIsValid &= FProjectValidationHelpers::IsValidText(Description, OutErrors);
    bIsValid &= FProjectValidationHelpers::IsValidArray(Items, OutErrors);

    return bIsValid;  // Return false if ANY validation failed
}
```

[X] **DON'T:**
```cpp
bool ValidateMyData(TArray<FText>& OutErrors) const
{
    // Early return means user only sees first error, not all issues
    if (!FProjectValidationHelpers::IsValidName(Name, OutErrors))
        return false;  // [X] Stops checking remaining fields

    if (!FProjectValidationHelpers::IsValidText(Description, OutErrors))
        return false;  // [X] User never sees this if Name failed

    return true;
}
```

### Async Execution Pattern

[OK] **DO:**
```cpp
// Capture shared pointers by value in lambdas
FProjectAsyncHelpers::ExecuteAfterDelay(World, 2.0f, [SharedPtr = MySharedPtr]() {
    // SharedPtr keeps object alive
    SharedPtr->DoWork();
});
```

[X] **DON'T:**
```cpp
// Raw pointer may be deleted before callback executes
FProjectAsyncHelpers::ExecuteAfterDelay(World, 2.0f, [this]() {
    // [X] 'this' may be invalid after 2 seconds
    this->DoWork();
});
```

### Error Code Usage

[OK] **DO:**
```cpp
// Use named constants from ProjectErrorCodes namespace
PhaseState.ErrorCode = ProjectErrorCodes::MountContent::DownloadFailed;
```

[X] **DON'T:**
```cpp
// Magic numbers make code unmaintainable
PhaseState.ErrorCode = 201;  // [X] What does 201 mean?
```

---

## Related Documentation

- [Loading Pipeline System](../systems/loading_pipeline.md) - Error handling and retry logic
- [ProjectCore Types](../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Types/) - Core data structures
- [Common Pitfalls](conventions.md#critical-blueprint-rules) - Avoid common validation mistakes


