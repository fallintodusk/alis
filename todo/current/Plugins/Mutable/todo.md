# Mutable and HairStrands Integration

Reference for Mutable + HairStrands integration work. Contains configuration, code changes, and asset setup needed for character customization pipeline.

## 1. Project and Engine Configuration Files

### `Alis.uproject`
- Enabled the following plugins:
  - `MetaHumanLiveLink`
  - `MutableDataflow`
  - `MutablePopulation`

### `Config/DefaultEngine.ini`
- **HairStrands + Mutable (Crash & Race Condition Fixes):**
  - Added CVars for HairStrands:
    ```ini
    r.HairStrands.Enable=1
    r.HairStrands.BindingValidation=0
    r.HairStrands.AsyncLoad=0
    r.HairStrands.MeshProjection=1
    ```
  - Added CVars for Mutable (to prevent thread synchronization crashes):
    ```ini
    mutable.GameThreadTaskMaxTime=1.0
    mutable.EnableMutableTaskLowPriority=0
    ```
- **SkinCache and Lumen:**
  - `r.SkinCache.ForceRecomputeDeltaPos=1`
  - `r.Lumen.HardwareRayTracing.LightingMode=2`
- Changed default startup maps to `/Game/Project/Test/Mutable_Test/TEST_MUT_2.TEST_MUT_2`.

## 2. C++ Source Code (`ProjectCharacter` Module)

### `ProjectCharacter.Build.cs`
- Added `"CustomizableObject"` to `PublicDependencyModuleNames`.

### `ProjectCharacter.uplugin`
- Appended `{ "Name": "Mutable", "Enabled": true }` to the `Plugins` dependencies list.

### `ProjectCharacter.h`
- Added Forward Declarations for: `UCustomizableObject`, `UCustomizableObjectInstance`, `UCustomizableSkeletalComponent`.
- Declared UPROPERTY member variables in `AProjectCharacter`:
  - `MutableComponent` (Type: `UCustomizableSkeletalComponent*`)
  - `MutableAsset` (Type: `UCustomizableObject*`)
  - `MutableInstance` (Type: `UCustomizableObjectInstance*`)
- Added header definitions for `UpdateCharacterAppearance()` and `SetMutableColorParameter(const FString&, FLinearColor)`.

### `ProjectCharacter.cpp`
- **Mutable Initialization:**
  - Created the Mutable component in the `AProjectCharacter` constructor:
    ```cpp
    MutableComponent = CreateDefaultSubobject<UCustomizableSkeletalComponent>(TEXT("MutableComponent"));
    MutableComponent->SetupAttachment(GetMesh());
    ```
- *(Note: The entire file was automatically formatted by `clang-format`, resulting in numerous whitespace/indentation changes in the git diff).*

## 3. Blueprints and Assets

### `BP_Hero_Motion.uasset` (Main Character Blueprint)
- **True First Person Camera Fix (For Modular Characters):**
  - Added camera re-attachment logic in the **Construction Script** because the parent body mesh does not have a `head` bone natively.
  - Utilized the `Attach Component To Component` node: Target = `FirstPersonCamera`, Parent = `Head`, Socket Name = `head`. Transform Rules = `Snap to Target`.
  - Used an `Add Local Offset` node (e.g., `X=15, Z=5`) immediately after to push the camera forward and prevent interior head mesh clipping.

### Mutable Assets (Base Body and Head)
- `CO_Character.uasset`
- `CO_Character_Head.uasset`
- These files contain parameter modifications. We previously increased the bone influence limit (`CustomizableObjectNumBoneInfluences`) to 12 for correct HairStrands mapping.

### Test Maps and Objects
- `COI_Test.uasset` (Modified) — Character instance that previously caused editor crashes (resolved by updating to Unreal Engine 5.7.4).
- `TEST_MUT_2.umap` (Newly created) / `Test_Mutable.umap` (Modified) — Test environments used to verify Mutable plugin logic.