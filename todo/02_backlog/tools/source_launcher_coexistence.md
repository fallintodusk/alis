# Source / Launcher Engine Coexistence — Investigation

**Status:** Open, investigation only (no code yet)
**Priority:** Minor — unblocks Phase 4 LLT revisit only; not on critical path
**Date:** 2026-04-23
**Owner:** unassigned

---

## Why this exists

ALIS uses the launcher-installed UE engine (`scripts/config/ue_path.conf` →
`<ue-path>`). We do NOT plan to switch to a source-built engine
for daily work. The current canonical decision (see
`docs/agents/canonical.md` → "Phase 4 Low Level Tests - DEFERRED on launcher
engine") treats "move to source engine" as a revisit condition. That adds
friction to cold reading: it suggests a dual-engine lifestyle we actually
don't want.

This todo reframes the question as a narrow coexistence investigation:

> **Can a parallel source UE tree coexist with the launcher install, be used
> for a specific target class (unique-build-env / LLT / debug-engine-code),
> AND not invalidate the launcher-anchored UBT cache when we switch back to
> day-to-day launcher builds?**

If yes + cheap: reopen Phase 4 LLT. If no / expensive: close this todo and
strike the revisit condition from canonical.md so future agents stop seeing
a "maybe switch to source" hint.

---

## What we already know (do not re-discover)

- Hard UE limit: `Engine/Source/Programs/UnrealBuildTool/System/RulesAssembly.cs:679`
  blocks any `TargetRules` that sets `TargetBuildEnvironment.Unique` on an
  installed engine. `TestTargetRules` with `bCompileAgainstCoreUObject = true`
  is affected.
- The marker that makes an engine "installed" is the file
  `Engine/Build/InstalledBuild.txt`. Remove it and the engine tree behaves
  as a source build. Launcher repairs this file on any update/verify.
- UBT keys build artifacts on engine root path. Changing `UE_PATH` =
  BuildRules cache invalidation. The project has hit this before.
- Past incident (2026-04-23): touching `<launcher>/Engine/Intermediate/Build/BuildRules/`
  broke the launcher install and required a launcher "Update" (not Verify,
  Verify was greyed out). Settings hard-deny rules
  (`.claude/settings.json`) now prevent agent writes under launcher paths.
- Epic's official position (forum thread linked in canonical.md): unique
  build environments require a source build.

---

## Non-goals

- Do NOT attempt to convert the existing launcher install to a source tree.
  That is what broke last time and is explicitly denied at the settings
  level now.
- Do NOT make daily iteration depend on a source tree.
- Do NOT rewrite launcher internals to accept unique-build-env targets.
- Do NOT land this investigation as a "just always have both" workflow. The
  goal is to measure a specific narrow path, not to institutionalize dual
  engines.

---

## Hypotheses to test (each is independent)

### H1 — Parallel tree, different path, per-target dispatch

- Lay down a source UE tree at a different path (e.g. `<ue-path>-src`).
- Keep `UE_PATH` in `scripts/config/ue_path.conf` pinned to launcher.
- Add a per-target script that passes `-EngineRoot <ue-path>-src`
  only when building a unique-build-env target.
- Question: does UBT maintain separate BuildRules caches per engine root,
  so that returning to the launcher-rooted build does NOT trigger a cold
  rebuild of ALIS project modules?

Expected measurement:
- baseline: AlisEditor incremental rebuild after a trivial cpp edit
- flow A: same rebuild immediately after running an LLT build against the
  source tree; record wall-clock and which modules got invalidated

### H2 — Overlay only the tools (UBT/UHT) from source

- Build UnrealBuildTool + UnrealHeaderTool from a source tree ONCE.
- Copy the produced binaries next to the launcher install (or run via an
  env override).
- Question: can newer/source UBT recognize the launcher tree as source for
  the purposes of unique-build-env targets? (strong prior: no — the
  `InstalledBuild.txt` check runs at target-rules-assembly time.)

Likely outcome: negative, but worth five minutes of verification before
discarding.

### H3 — Source tree used only for LLT exe production

- Keep the launcher install untouched.
- Treat the source tree as a one-shot builder: `Build.bat AlisLLTTests
  Win64 Development -Project=...` invoked against the source root.
- The produced .exe runs standalone (Catch2). No UBT state from the LLT
  build should touch the project's launcher-anchored intermediate.
- Question: does UBT actually keep the per-root caches clean, or does it
  write into `<Project>/Intermediate/` in a way that forces a reparse when
  the next launcher build runs?

Measurement rig:
1. Clean warm state against launcher (iterate.ps1 baseline).
2. Build LLT exe against source tree.
3. Re-run iterate.ps1 — record whether it is warm-fast or cold-slow.

### H4 — Accept the cache bounce, mitigate with explicit cache swap

- Document a tiny script that saves+restores `<Project>/Intermediate/` and
  `<Project>/Binaries/` per engine root.
- Accept one cache warmup on first switch, reuse afterwards.
- Only viable if LLT use becomes frequent. Currently it is not.

---

## Decision gate

After measuring H1 + H3 (H2 discard-fast, H4 bookkeeping-only), produce a
one-paragraph decision:

- **Cheap coexistence confirmed** → reopen Phase 4 LLT pilot. Update
  `docs/agents/canonical.md` "Phase 4 Low Level Tests" block. Remove the
  revisit condition pointing at this todo and replace with direct
  instructions.
- **Not cheap** → close this todo. Strike the "maybe switch to source"
  revisit condition from canonical.md entirely. Phase 4 stays deferred
  without the dangling hint.

Either outcome is a decision. Do not leave the investigation half-done.

---

## Concrete first step (when picked up)

1. Read this file top to bottom.
2. Confirm no hidden commitments exist — grep for `UnrealEngine-5.7-src`
   or other source-tree paths anywhere in `scripts/` and repo config. If
   someone already started this, start from whatever landed.
3. Do H1 first with the smallest possible source tree (single UE release
   branch clone, Setup + GenerateProjectFiles, no custom patches).
4. Run the measurement rig. Record numbers.
5. Write the decision paragraph.

Do NOT start by building all the hypotheses in parallel. One at a time.

---

## Cross-refs

- `docs/agents/canonical.md` — Phase 4 LLT decision block (current SOT).
- `docs/agents/overnight_mode.md` — launcher-engine safety rule (do not
  delete launcher intermediates).
- `.claude/settings.json` — hard deny rules that enforce that safety rule
  at the harness level.
- `todo/01_done/test/test_iteration_speedup.md` — Phase 4 summary references
  this todo as the revisit gate.

---

## Forbidden shortcuts

- Deleting anything under `<launcher>/Engine/Intermediate/` — denied.
- Moving/renaming `<launcher>/Engine/Build/InstalledBuild.txt` — denied.
- Writing under `C:/UnrealEngine/**` or `G:/UnrealEngine-*/**` in the
  launcher tree — denied.
- A source tree under a different path is fine; touching the launcher tree
  is not.
