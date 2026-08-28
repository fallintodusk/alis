# ProjectSkeletalCapabilities Pitfalls

Verified failures and their regression evidence for skeletal capability assets and
runtime adapters.

## Retarget profile controller used after a failed cast

**Symptom:** `ABP_WorldBodyRetarget` emits `None` access warnings from
`UpdateRetargetProfile` every animation update. Packaged performance evidence is
polluted by tens of thousands of warnings.

**Root cause:** an execution `Sequence` continued into `SetSettings` even when
`GetOpControllerFromRetargetProfile` did not produce an
`IKRetargetIKChainsController`.

**Fix:** gate the complete get/modify/set chain behind the successful controller cast.
On cast failure, leave the input profile untouched and return without dereferencing the
controller.

**File:** `Content/MotionMatching/ABP_WorldBodyRetarget.uasset`

**Regression evidence:** run the canonical character parity capture and require all
five scenarios to pass with zero `UpdateRetargetProfile` or Blueprint `None` access
warnings.

