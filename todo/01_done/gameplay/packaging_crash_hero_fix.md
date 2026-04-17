# Fix: Shipping Build Crash + Missing Assets (2026-04-16)

**Status:** Resolved
**Priority:** Critical
---

## Fixes Applied

| # | File | Problem | Fix |
|---|------|---------|-----|
| F1/F2 | [LocalBodyCorrectionChainIK.cpp:35-51](Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/LocalBody/LocalBodyCorrectionChainIK.cpp#L35-L51) | CCDIK `RotationLimitPerJoints` empty → ACCESS_VIOLATION | Fill via UE reflection (`FScriptArrayHelper`), 3 × 30.0f |
| F3 | [SinglePlayModeDefaults.cpp](Plugins/Gameplay/ProjectSinglePlay/Source/ProjectSinglePlay/Private/SinglePlayModeDefaults.cpp) | Broken `DefaultPawnClass` path to `BP_Hero` | Removed — Modular system uses [Hero.json](Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json) |
| F4 | [DefaultGame.ini:51-52](Config/DefaultGame.ini#L51-L52) | `ABP_MotionMatchingBridge` + `COI_Hero` not cooked | `+DirectoriesToAlwaysCook` for both directories |
| F5 | [Alis.Target.cs:20](Source/Alis.Target.cs#L20) | Development modular link fails on installed engine | `LinkType = Monolithic` for all configs |
| F6 | [ProjectIntegrationTests.uplugin:97](Plugins/Test/ProjectIntegrationTests/ProjectIntegrationTests.uplugin#L97) | Test plugin breaks Game target (depends on `UnrealEd`) | Module type → `Editor` |

---

## Crash Analysis

**Error:** `EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000004`
**Crash IDs:** `UECC-Windows-BA651...`, `UECC-Windows-12B90...`
**Tool:** [analyze_dump.ps1](scripts/debug/analyze_dump.ps1) (cdb.exe)

### Root Cause

Engine bug in `CCDIK.cpp:31` — reads `RotationLimitPerJoints[LinkIndex]` **before** checking `bEnableRotationLimit`. Empty array = null data pointer = crash.

The array was empty because `ResizeRotationLimitPerJoints()` is editor-only in UE 5.7 (`#if WITH_EDITOR`). Setting `bEnableRotationLimit = false` does NOT prevent the read.

### Crash Call Stack

```
SolveTwoBoneIK                           ← CRASH: movss xmm0, [rax+rdi*4] (rax=NULL)
  SolveCCDIK
    FAnimNode_CCDIK::EvaluateSkeletalControl_AnyThread
      UAnimInstance::ParallelEvaluateAnimation
        USkeletalMeshComponent::InitAnim
          ULocalFirstPersonCapability::ApplyVisibility          (line 329)
            OnMutableInstanceUpdated                            (line 419)
              UCustomizableObjectSystem::TickInternal
```

### Fix (F1/F2)

In [LocalBodyCorrectionChainIK.cpp:35-51](Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/LocalBody/LocalBodyCorrectionChainIK.cpp#L35-L51):

```cpp
// Fill private RotationLimitPerJoints via reflection (editor-only method unavailable)
FArrayProperty* ArrayProp = CastField<FArrayProperty>(
    FAnimNode_CCDIK::StaticStruct()->FindPropertyByName(TEXT("RotationLimitPerJoints")));
if (ArrayProp)
{
    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(&CCDIKNode));
    Helper.Resize(3);
    for (int32 i = 0; i < 3; ++i)
        *reinterpret_cast<float*>(Helper.GetRawPtr(i)) = 30.f;
}
```

---

## Asset Cooking Issue

**Problem:** Character had no animation and no mesh in packaged builds.

| Asset | Loaded via | Runtime error |
|-------|-----------|---------------|
| `ABP_MotionMatchingBridge` | `LoadObject<UClass>()` in [MotionMatchingCapability.cpp:179](Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/MotionMatchingCapability.cpp#L179) | SkipPackage — no animation |
| `COI_Hero` | `LoadSynchronous()` in [MutableCustomizationCapability.cpp:333](Plugins/Gameplay/ProjectSkeletalCapabilities/Source/ProjectSkeletalCapabilities/Private/MutableCustomizationCapability.cpp#L333) | SkipPackage — no mesh |

Both exist on disk but cooker can't discover dynamic `LoadObject` refs. Fix F4 adds `+DirectoriesToAlwaysCook` in [DefaultGame.ini:51-52](Config/DefaultGame.ini#L51-L52).

---

## Verification — PASSED

| Check | Result |
|-------|--------|
| Compile | 0 errors (292/292 actions) |
| Package Shipping | SUCCESS (11m 17s) |
| Package Development | SUCCESS (3m 15s) |
| Launch Shipping | No crash, map loads |
| Launch Development (`-log`) | ABP + COI load confirmed |
| Character | Mesh, animations, movement working |
| New crash dumps | None |

---

## Remaining Issues

| # | Severity | Description |
|---|----------|-------------|
| B1 | Major | Camera behind neck — [Hero.json:127](Plugins/Resources/ProjectObject/Content/Human/Hero/Hero.json#L127) `relativeOffset` Z=73, needs ~Z=80-90 |
| B2 | Medium | ESC does not open main menu (persists from Apr 10) |
| B3 | Low | [InteractionComponent.cpp:830](Plugins/Gameplay/ProjectInteraction/Source/ProjectInteraction/Private/InteractionComponent.cpp#L830) warns "No CameraComponent" — camera created after BeginPlay in [DefinitionCharacter.cpp:352](Plugins/Gameplay/ProjectCharacter/Source/ProjectCharacter/Private/DefinitionCharacter.cpp#L352) |

---

## References

- Test report: [16.04.26_test_report.md](16.04.26_test_report.md)
- Crash analysis tool: [analyze_dump.ps1](../../scripts/debug/analyze_dump.ps1)
- Minidumps: [<local-app-data>/Alis/Saved/Crashes/](file:///<local-app-data>/Alis/Saved/Crashes/)
- Runtime logs: [WindowsDev/Alis/Saved/Logs/Alis.log](file:///<user-home>/Desktop/WindowsDev/Alis/Saved/Logs/Alis.log)
- Engine bug: `AnimationCore/Private/CCDIK.cpp:31` (reads array before checking flag)
