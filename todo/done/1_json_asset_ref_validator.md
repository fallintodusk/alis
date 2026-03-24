# JSON Asset Reference Validator & Auto-Fix

## Problem Statement

JSON definition files reference UE assets by string path:
```json
{
  "id": "wooden_chair",
  "meshes": [{ "asset": "/Game/Meshes/SM_WoodenChair" }]
}
```

When assets are renamed/moved in editor, JSON references break silently.
Unlike Blueprint/C++ references, JSON has no UUID - UE redirectors don't help.

---

## Solution Architecture

### Plugin: ProjectAssetSync

New editor plugin in `Plugins/Editor/ProjectAssetSync/` with responsibility:
**Keep JSON data files synchronized with UE asset changes.**

**Design principle:** Renames are rare - don't maintain a live index. Just fix JSON when rename happens.

**TODO: Future consolidation (evaluate after core implementation):**
- [ ] Consider moving `FDefinitionValidator` from ProjectDefinitionGenerator
- [ ] Consider moving `FDefinitionEvents` from ProjectDefinitionGenerator
- [ ] Consider moving `DefinitionActorSyncSubsystem` from ProjectPlacementEditor

---

## V1: Simple Implementation (Start Here)

### Core Components

```
ProjectAssetSync/
  Source/ProjectAssetSync/
    Public/
      AssetSyncSubsystem.h        // UEditorSubsystem - hooks rename delegate
      JsonAssetRefFixer.h         // Static helper - scan & replace logic
    Private/
      AssetSyncSubsystem.cpp
      JsonAssetRefFixer.cpp
      ProjectAssetSyncModule.cpp  // Module startup
```

### V1 Workflow

```
Asset Renamed in Editor
         |
         v
OnAssetRenamed(NewData, OldPath) - collect to PendingRenames map
         |
         v
Debounce timer (200-500ms) - batch folder moves
         |
         v
For each JSON file in plugin Data/ directories:
  - Quick check: if file doesn't contain OldPath as substring, skip
  - Parse JSON
  - Replace exact string values matching OldPath with NewPath
  - Write file (checkout if SCC)
         |
         v
Notification: "Updated N JSON refs for [AssetName]"
```

### Key Classes

```cpp
// UEditorSubsystem - lifecycle, hooks AssetRegistry
UCLASS()
class UAssetSyncSubsystem : public UEditorSubsystem
{
    // Pending renames (OldPath -> NewPath), batched via timer
    TMap<FString, FString> PendingRenames;
    FTimerHandle DebounceTimer;

    void OnAssetRenamed(const FAssetData& NewData, const FString& OldPath);
    void ProcessPendingRenames();
};

// Static helper - no state, just logic
class FJsonAssetRefFixer
{
public:
    // Scan all JSON files and replace OldPath with NewPath
    static int32 FixReferences(
        const TMap<FString, FString>& PathMappings,  // OldPath -> NewPath
        const TArray<FString>& JsonRoots);           // Directories to scan

    // Normalize path (handle .AssetName suffix)
    static FString NormalizePath(const FString& Path);

    // Check if string looks like asset path
    static bool LooksLikeAssetPath(const FString& Value);
};
```

### Correctness Rules

**1. Normalize paths both ways:**
```cpp
// JSON may have either format:
"/Game/Foo/SM_Bar"           // Package path
"/Game/Foo/SM_Bar.SM_Bar"    // Full object path

// Rename event gives full object path
// Must treat both as equivalent
```

**2. Match all mount points (not just /Game/):**
```cpp
bool FJsonAssetRefFixer::LooksLikeAssetPath(const FString& Value)
{
    return Value.StartsWith(TEXT("/Game/")) ||
           Value.StartsWith(TEXT("/Script/")) ||
           Value.StartsWith(TEXT("/Project"));  // Plugin mounts
}
```

**3. Replace exact string values only:**
```cpp
// DON'T do raw string replace (breaks partial matches)
// DO parse JSON and replace values that exactly equal OldPath
```

---

## V2: Index + Cache (Only If V1 Becomes Slow)

If scanning becomes slow with many JSON files, add:

### Additional Components

```
Public/
  JsonAssetRefIndex.h       // In-memory index: AssetPath -> JSON refs
  JsonAssetRefCache.h       // Disk persistence for fast startup
```

### When to Add V2

- Startup scan takes >2 seconds
- Rename fix takes >500ms
- You have 1000+ JSON files

---

## UE5 Integration Points

### Asset Registry Delegates (IAssetRegistry.h:936-937)

```cpp
// Signature: void(const FAssetData& NewAssetData, const FString& OldObjectPath)
DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FAssetRenamedEvent, const FAssetData&, const FString&);
virtual FAssetRenamedEvent& OnAssetRenamed() = 0;
```

### Hook Pattern (from AssetSourceFilenameCache.cpp)

```cpp
void UAssetSyncSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    AssetRegistry.OnAssetRenamed().AddUObject(this, &UAssetSyncSubsystem::OnAssetRenamed);
}

void UAssetSyncSubsystem::Deinitialize()
{
    if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
    {
        if (IAssetRegistry* AssetRegistry = Module->TryGet())
        {
            AssetRegistry->OnAssetRenamed().RemoveAll(this);
        }
    }
    Super::Deinitialize();
}
```

---

## Edge Cases

### 1. Source Control Integration

```cpp
// Checkout JSON files before modify (if SCC active)
ISourceControlProvider& SCC = ISourceControlModule::Get().GetProvider();
if (SCC.IsEnabled())
{
    SCC.Execute(ISourceControlOperation::Create<FCheckOut>(), JsonFilePath);
}
```

### 2. Invalid JSON

- Log warning, skip file, continue processing others
- Don't crash on malformed JSON

### 3. Debounce for Folder Moves

- Folder rename triggers many OnAssetRenamed events
- Debounce timer batches them into single processing pass

---

## Validation Commandlet (CI)

```cpp
// For CI: find broken references without fixing
UCLASS()
class UValidateJsonAssetRefsCommandlet : public UCommandlet
{
    virtual int32 Main(const FString& Params) override;
};
```

Usage: `UnrealEditor-Cmd.exe Alis.uproject -run=ValidateJsonAssetRefs`

Validates by checking if Asset Registry can resolve each path.

---

## Implementation Order

### Phase 1: V1 Core (Start Here)
- [ ] Create ProjectAssetSync plugin structure
- [ ] Implement UAssetSyncSubsystem (hook OnAssetRenamed)
- [ ] Implement FJsonAssetRefFixer::FixReferences()
- [ ] Implement path normalization (reuse from DefinitionJsonParser)
- [ ] Debounce timer for folder moves
- [ ] Editor notification on fix

### Phase 2: Polish
- [ ] Source control integration
- [ ] Validation commandlet for CI
- [ ] Handle edge cases (invalid JSON, etc.)

### Phase 3: V2 Optimization (Only If Needed)
- [ ] FJsonAssetRefIndex (in-memory index)
- [ ] FJsonAssetRefCache (disk persistence)
- [ ] Incremental rebuild on startup

---

## Dependencies

- `AssetRegistry` module (OnAssetRenamed delegate)
- `Json` module (JSON parsing)
- `SourceControl` module (optional, for SCC integration)
- `Slate` module (notifications)

---

## Reusable Code

**From ProjectDefinitionGenerator:**
- `FDefinitionJsonParser::NormalizeAssetPath()` - path normalization
- `FDefinitionJsonParser::NormalizeClassPath()` - class path normalization

Consider extracting to shared location (ProjectEditorCore) if both plugins need it.

---

## Acceptance Tests

1. Rename mesh used in JSON (exact path match) -> JSON updates
2. Move folder with many assets -> debounce batches, each JSON written once
3. Validator commandlet reports broken ref if Asset Registry can't resolve path
4. Invalid JSON files are skipped with warning, don't crash

---

## References

- [UE5 Asset Registry Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-registry-in-unreal-engine)
- [UEditorSubsystem API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/EditorSubsystem/UEditorSubsystem)
- [FAssetRegistryModule API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AssetRegistry/FAssetRegistryModule)
- UE Source: `Engine/Source/Editor/UnrealEd/Private/AutoReimport/AssetSourceFilenameCache.cpp`
