# BP_Hero_Motion — MCP Audit (2026-03-20)

## Priority Table

| # | Issue | Criticality | Status |
|---|-------|------------|--------|
| 1 | C++/BP Speed Conflict — GAS multiplier stomped | **CRITICAL** | ROLLED BACK |
| 2 | Default Gait: Walk + Shift=Run, Sprint removed | **HIGH** | ROLLED BACK |
| 3 | C++ runtime inputs block BP content inputs | **CRITICAL** | OPEN — PRIORITY |
| 4 | CalculateBrakingFriction not wired | **MEDIUM** | OPEN |
| 5 | GetTraversalCheckInputs empty branches | **LOW** | OPEN |
| 6 | Ragdoll capsule teleport snap-back | **MEDIUM** | OPEN |
| 7 | Ragdoll replication (multiplayer) | **LOW** | OPEN |
| 8 | Event naming cleanup (unnamed events) | **LOW** | OPEN |

---

## CRITICAL

### Issue 1 — C++/BP Speed Conflict (ROLLED BACK)

GAS `MovementSpeedMultiplier` was ignored — C++ `RefreshMovementSpeed()` overwrote `MaxWalkSpeed` every frame, stomping the BP pipeline.

**Fix (documented, rolled back — to be re-applied after Issue 3):**
- Gut `RefreshMovementSpeed()` in `ProjectCharacter.cpp` — BP becomes sole authority
- Add `GetMovementSpeedMultiplier()` as `BlueprintCallable`
- Wire `Multiply` node into `CalculateMaxSpeed` and `CalculateMaxCrouchSpeed`:
  `SelectFloat → Multiply(A) * GetMovementSpeedMultiplier(B) → Return`

**Files:** `ProjectCharacter.cpp`, `ProjectCharacter.h`, `BP_Hero_Motion` (2 graphs)

---

### Issue 3 — C++ Runtime Inputs Block BP Content Inputs (OPEN — PRIORITY)

#### Problem Summary

`ProjectCharacter.cpp` creates **runtime duplicate** input objects (`NewObject<UInputAction>`) that conflict with the **content assets** (`IMC_Sandbox`, `IA_Sprint`) used by BP_Hero_Motion's Blueprint event nodes. Two different `UInputAction` objects fight for the same physical key (LeftShift) — one wins, the other is silenced.

**Proof:** `SandboxCharacter_CMC` (parent: vanilla `ACharacter`, no C++ input) works perfectly with the same BP setup. `BP_Hero_Motion` (parent: `AProjectCharacter` with C++ input) doesn't.

**Architectural rule:** `ProjectCharacter.cpp` is the single source of truth for input, not BP.

---

#### Root Causes

| # | Root Cause | Impact |
|---|-----------|--------|
| **1** | `CreateDefaultInputAssets()` creates runtime `UInputAction` objects (e.g. `NewObject IA_Sprint`) that are **different objects** from the content assets (`/MotionMatching/Input/IA_Sprint`) referenced by BP nodes | C++ binds to runtime action, BP listens to content action — they never meet |
| **2** | `PawnClientRestart()` adds runtime `DefaultMappingContext` at priority 0 — same priority as BP's `IMC_Sandbox` | Two contexts compete for the same keys; Enhanced Input LIFO rule means whichever is added last wins |
| **3** | `CreateDefaultInputAssets()` is called **3 times** (BeginPlay, PawnClientRestart, SetupPlayerInputComponent) — the runtime context may be re-added after BP's IMC_Sandbox | Runtime context becomes last-added → wins → `bConsumeInput=true` (default) blocks content actions |
| **4** | C++ `BindAction()` binds to runtime `SprintAction`, not content `IA_Sprint` | Even if content action fires, C++ `StartSprint`/`StopSprint` callbacks don't trigger |

---

#### Solutions Table (Epic UE 5.7 Gold Standards)

| Option | Approach | Epic Uses? | C++ is SoT? | Designer Friendly | Scalability | Complexity | Verdict |
|--------|----------|-----------|-------------|-------------------|-------------|-----------|---------|
| **A** | Runtime `NewObject` (current) | No | Yes | Poor | Low | Low | **BROKEN** — conflicts with BP content assets |
| **B** | `ConstructorHelpers::FObjectFinder` loads content assets in C++ constructor | **Yes (Lyra 5.7)** | **Yes** | Good | High | Medium | **RECOMMENDED — Epic gold standard** |
| **C** | `UPROPERTY(EditAnywhere)` + set in BP Class Defaults | Yes (Lyra) | Partial | Excellent | High | Low | Good, but BP overrides C++ = not pure SoT |
| **D** | `UDataAsset` config container (like Lyra's `ULyraInputConfig`) | **Yes (Lyra 5.7)** | Yes | Good | Very High | Medium-High | Best for large projects, overkill now |
| **E** | Pure Blueprint — remove all C++ input code | Limited | No | Excellent | Low | Low | Violates "C++ is SoT" rule |
| **F** | Game Feature Actions with auto-registration | **Yes (Lyra 5.7)** | Yes | Excellent | Very High | **High** | Enterprise modular pattern, overkill now |

**Epic's Lyra 5.7 uses Options B + D + F together:**
- **B** for core native actions (Move, Look, Crouch) — loaded via `ConstructorHelpers` or UPROPERTY defaults
- **D** for ability actions — `ULyraInputConfig` DataAsset maps `UInputAction` → `FGameplayTag`
- **F** for modular input — Game Features register their own IMCs on activation

**For our project:** Option **B** is the minimum viable fix. Single file change, matches Epic pattern, keeps C++ as SoT.

---

#### Research: All Options (Epic gold standard + alternatives)

| Option | Approach | Epic Uses? | C++ is SoT? | Designer Friendly | Scalability | Complexity |
|--------|----------|-----------|-------------|-------------------|-------------|-----------|
| **A** | Runtime `NewObject` (current) | No | Yes | Poor | Low | Low |
| **B** | `ConstructorHelpers` load content assets | Yes (Lyra) | **Yes** | Good | High | Medium |
| **C** | UPROPERTY + BP class defaults | Yes (Lyra) | Partial | Excellent | High | Low |
| **D** | UDataAsset config container | Optional | Yes | Good | Very High | Medium-High |
| **E** | Pure Blueprint (no C++ input) | Limited | No | Excellent | Low | Low |
| **F** | Game Feature Actions (Lyra modular) | Yes (Lyra) | Yes | Excellent | Very High | **High** |

**Option A — Runtime NewObject (current, broken)**
- C++ creates `NewObject<UInputAction>` and `NewObject<UInputMappingContext>` at runtime
- Maps keys via `DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift)`
- Pros: Zero content assets, everything in code
- Cons: **Duplicates BP content actions → input conflict**, designers can't tweak, not Epic standard
- Verdict: **BROKEN** when BP also has content assets

**Option B — ConstructorHelpers (RECOMMENDED)**
- C++ loads MotionMatching content assets in constructor via `ConstructorHelpers::FObjectFinder`
- Same objects as BP → no duplication, no conflict
- Pros: **C++ stays SoT**, matches Lyra pattern, content assets editable in editor
- Cons: Hard-coded asset paths in C++ (breaks if assets move)
- Verdict: **Epic's gold standard** — Lyra does this

**Option C — UPROPERTY + BP Class Defaults**
- C++ exposes `UPROPERTY(EditAnywhere)`, BP_Hero_Motion sets them in Class Defaults panel
- Pros: No hard-coded paths, designer sets which assets to use
- Cons: BP overrides C++ defaults → BP becomes partial SoT, not pure C++ authority
- Verdict: Good for teams, but violates "C++ is sole SoT" rule

**Option D — UDataAsset Config Container**
- Create `UInputDataConfig` with arrays of action references
- Character loads one config asset instead of many individual ones
- Pros: Scales well, one reference instead of 6+ UPROPERTYs
- Cons: Extra class, extra indirection
- Verdict: Overkill for current scope, good for future

**Option E — Pure Blueprint**
- Remove all C++ input code, let BP handle everything
- Pros: Simple, no conflicts
- Cons: Violates "C++ is SoT", loses C++ binding benefits
- Verdict: **Not suitable** — user requires C++ authority

**Option F — Game Feature Actions**
- Modular input via Game Feature Plugins with auto-registration
- Pros: Most scalable, plugin-compatible
- Cons: Heavy infrastructure, Lyra-level complexity
- Verdict: Enterprise pattern, overkill now

---

#### Recommended: Option B — ConstructorHelpers

```cpp
// In AProjectCharacter constructor:
static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(
    TEXT("/MotionMatching/Input/IMC_Sandbox"));
if (IMC.Succeeded()) DefaultMappingContext = IMC.Object;

static ConstructorHelpers::FObjectFinder<UInputAction> SprintIA(
    TEXT("/MotionMatching/Input/IA_Sprint"));
if (SprintIA.Succeeded()) SprintAction = SprintIA.Object;
// ... same for MoveAction, LookAction, JumpAction, CrouchAction
```

**Why this works:**
1. C++ loads the **same** content assets that BP's `EnhancedInputAction` nodes reference
2. `if (!SprintAction)` skips runtime creation — no duplicates
3. C++ remains **sole authority** — BP reads from C++ actions, doesn't create its own
4. `PawnClientRestart` adds the real `IMC_Sandbox`, not a runtime context
5. BP's `SetupInput` → `AddMappingContext(IMC_Sandbox)` becomes a harmless no-op (already added)

---

## HIGH

### Issue 2 — Default Gait Rework (ROLLED BACK)

Walk-by-default, Shift = Run, Sprint removed.

**GetDesiredGait return nodes changed:**
- `01503E88`: Sprint(2) → Run(1) — Shift + moving
- `FA7B19F6`: Run(1) → Walk(0) — Shift + idle
- `DFACC3F9`: Run(1) → Walk(0) — moving without Shift

---

## MEDIUM

### Issue 4 — CalculateBrakingFriction Not Wired

`CalculateBrakingFriction` exists (7 nodes) but is never called in `UpdateMovement_PreCMC`. The Sequence sets `BrakingDeceleration` and `GroundFriction` but skips `BrakingFriction`.

**Fix:** Add `SET BrakingFriction = CalculateBrakingFriction()` to the Sequence.

---

### Issue 6 — Ragdoll Capsule Teleport

`Ragdoll_End` re-enables capsule collision without teleporting it to the ragdolled mesh position. Character snaps back to pre-ragdoll location.

**Fix:** Before re-enabling capsule, set actor location to pelvis bone world position.

---

## LOW

### Issue 5 — GetTraversalCheckInputs Empty Branches

Logic paths terminate without setting output. Needs investigation — dead paths or missing logic.

### Issue 7 — Ragdoll Replication

`IsRagdolling` is local only. No server RPC or multicast — other clients won't see ragdoll.

### Issue 8 — Event Naming Cleanup

Multiple `on None` events in EventGraph. Should be renamed: `OnLanded`, `OnJumped`, `OnTick`, `OnPossessed`, etc.