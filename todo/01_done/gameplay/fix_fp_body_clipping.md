# Fix First-Person Body Clipping

Active work tracker for the first-person body clipping fix.

**Architecture SOT:** `Plugins/Gameplay/ProjectSkeletalCapabilities/docs/architecture.md`
(proven dead ends, policy ownership, skeleton chain, key discoveries, test infrastructure)

---

## Current Status (2026-04-13)

- Strategy pattern refactor complete (ILocalBodyCorrection interface)
- 4 modes: Disabled, TransitionGuard, AngleClamp, ChainIK
- Default: ChainIK (CCDIK neck chain + TwoBoneIK arm restore)
- NormalizeAxis pitch fix applied (PIE/test parity)
- Clipping solved for sprint-stop (zero ray/proxy/capsule hits with TransitionGuard)
- Build needs editor closed to link (DLL lock from last session)

## Remaining Work

1. Fix build (close editor, rebuild with unity-safe anonymous namespaces)
2. Test ChainIK with NeckLock + CCDIK + TwoBoneIK pipeline in PIE
3. Investigate CCDIK + CopyPose jitter (solver fights copied pose each frame)
   - Current approach: NeckLock(BMM_Replace) before CCDIK so neck is hard-pinned
   - May need per-bone alpha blending (neck=0% CopyPose, spine_05=100% CopyPose)
4. Validate arm IK quality (TwoBoneIK restores hand positions after spine correction)
5. Run ClipMatrix test suite with ChainIK as default
6. If hands acceptable, mark clipping as solved

## Test Commands

```powershell
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.Baseline" -TimeoutSeconds 900
./scripts/ue/test/character/capture_parity.ps1 -TestFilter "ProjectIntegrationTests.Character.FirstPerson.ClipMatrix.FilterV1" -TimeoutSeconds 900
```
