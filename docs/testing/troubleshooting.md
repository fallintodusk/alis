# Troubleshooting & Debugging

Strategies and tools for diagnosing and fixing test failures in Alis.

---

## Quick Checklist

1. **Check Logs:** `Saved/Logs/Alis.log` (look for "Error" or "fail")
2. **Reproduce Locally:** Can you run just that test? `scripts/ue/test/unit/run_cpp_tests.bat MyTest`
3. **Check Environment:** Is it failing only in CI? (Check `-NullRHI` or networking)
4. **Interactive Debug:** Run with Session Frontend in Editor.

---

## Debugging Workflow

### Step 1: Identify Failure

**Batch Output:**
```
Tests FAILED!
Exit code: 1
```

**Check:** `Saved/Automation/Reports/index.json` or `Saved/Logs/Alis.log`.

### Step 2: Read Test Logs

**Search for:**
```
LogAutomationController: Error: Test Completed. Result={Fail}
```

**Context:** Look at lines *before* the failure for assertions or crashes.

### Step 3: Reproduce in PIE (Interactive)

1. Find failing test class (e.g., `FMenuViewModelNavigationTest`)
2. Open Session Frontend (`Window` -> `Developer Tools` -> `Session Frontend`)
3. Go to **Automation** tab
4. Filter for your test name
5. **Enable Breakpoints** in your IDE
6. Select test and click `Start Tests`

### Step 4: Run with Verbose Logging (CLI)

If you can't reproduce in PIE, run from CLI with extra logging:
```bash
"<ue-path>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "<project-root>\Alis.uproject" ^
  -ExecCmds="Automation RunTests MyFailingTest" ^
  -unattended -nopause -NullRHI -log -LogCmds="LogProjectLoading Verbose"
```

---

## Common Issues

### 1. GameFeature Not Loading
**Symptom:** Test fails with "GameFeature not found" or assets missing.
**Fix:**
- Check [GameFeature Requirements](architecture/conventions.md#gamefeature-requirements).
- Run diagnostic: `scripts/ue/test/smoke/gamefeature_registration.bat`.
- Ensure `GameFeatureData` asset exists and state is `Installed`.

### 2. Test Times Out
**Symptom:** Test hangs or exceeds time limit.
**Fix:**
- Check for infinite loops.
- Verify async operations (latents) are completing.
- **CRITICAL:** Ensure `-testexit="Automation Test Queue Empty"` flag is present if running custom scripts.

### 3. Flaky Test (Intermittent)
**Symptom:** Pass/Fail random.
**Fix:**
- **Race conditions:** Are you relying on `tick` order?
- **Initialization:** Is a static variable persisting between tests?
- **Floating point:** Use `TestEqual` with tolerance, not `==`.

### 4. CI-Only Failure
**Symptom:** Passes locally, fails in GitHub/GitLab.
**Fix:**
- **Rendering:** CI often uses `-NullRHI`. Does your test require a GPU/Viewport?
- **Audio:** CI often uses `-NoSound`.
- **Resolution:** CI might have different screen size. Use explicit resolution adjustments.

---

## Manual Verification

**Purpose:** Verify UI, UX, and visuals that cannot be automated.

**When to use:**
- Widget layout/styling (did the CSS break?)
- Animation timing (feels janky?)
- Input responsiveness
- Audio/VFX mixing

**Process:**
1. Launch **Standalone Game** (not just PIE, to ensure full boot).
2. Manually trigger the flow (e.g., "Open Inventory").
3. Verify against [Design Document](design/README.md).

---

# Runtime & Integration Troubleshooting

**Scope:** Integration issues across systems (boot, update, feature activation, loading pipeline).

## Where to look first
- **Boot/update:** `orchestrator.log`, `bootrom.log`
- **Game runtime:** `Saved/Logs/Alis.log`
- **Plugin-specific:** Check the plugin's `docs/` folder (e.g. `Plugins/UI/ProjectUI/docs/`).

## 1. Feature Activation
**Symptom:** Plugins or content not showing up.
- **Verify Manifest:** Check `orchestrator_manifest.json` entries.
- **Check API Macros:** Must be uppercase (`PROJECTNAME_API`) for linker.
- **Check .Build.cs:** Missing dependency modules prevent loading.
- **Diagnostic:** Run `scripts/ue/test/smoke/gamefeature_registration.bat`.

## 2. Loading Pipeline Stalls
**Symptom:** Game stuck on loading screen or "Resolving Assets".
- **Check Log Phase:** Find last completed phase in `Alis.log` (Phases 1-6).
- **Asset Scans:** Ensure scan paths cover the feature content.
- **Asserts:** Look for fatal errors around `ProjectLoadingSubsystem`.
- **Deep Dive:** [Loading Pipeline](systems/loading_pipeline.md).

## 3. Orchestrator / Boot Gating
**Symptom:** Game refuses to launch or update.
- **Build ID Mismatch:** Manifest build_id must match binary.
- **Cycles:** Check `depends_on[]` for circular dependencies.
- **Signatures:** Thumbprint must match allow-list.
- **Deep Dive:** [Boot Chain](architecture/boot_chain.md).

## 4. Network / Integration
**Symptom:** Connection failures or missing server data.
- **Config:** Confirm IP/Port in `DefaultGame.ini`.
- **Database:** Verify server credentials (if running server).
