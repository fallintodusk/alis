# Manual Task: Create ProjectCoreFeature GameFeatureData Asset

**Priority:** Critical (but non-blocking due to fallback)
**Time Required:** 5 minutes
**Status:** ⚠️ PENDING (Asset missing, system uses fallback)
**Owner:** Human with Unreal Editor access

---

## Current System Status

✅ **System Fully Functional:**
- ProjectBoot successfully activates fallback orchestrator (ProjectMenuExperience)
- All builds passing, 413 unit tests green
- No blocking issues - graceful degradation working

⚠️ **Asset Missing:**
- ProjectCoreFeature.uasset does not exist in Content/ directory
- System falls back to ProjectMenuExperience automatically
- User experience unchanged, but orchestrator pattern incomplete

---

## Why This Task Exists

**Architecture Goal:** Immutable loader (ROM/BIOS-like) that activates updatable orchestrator GameFeature

**Current Implementation:**
1. ✅ ProjectBoot (immutable loader) - NO hardcoded GF dependencies
2. ✅ Fallback mechanism - graceful degradation to ProjectMenuExperience
3. ⚠️ ProjectCoreFeature orchestrator - plugin structure exists, ASSET MISSING
4. ⏳ Update mechanism - not yet implemented (Phase 3+)

**Why Agent Can't Do This:**
- Agents cannot create .uasset files (binary UE editor format)
- GameFeatureData asset requires Unreal Editor GUI operations
- Agent CAN create plugin structure, config, C++ code (all complete)
- Agent CANNOT manipulate binary UE assets

---

## Task: Create ProjectCoreFeature.uasset

### Prerequisites
- Unreal Editor running with Alis.uproject loaded
- ProjectCoreFeature plugin visible in Plugins browser
- Content Browser open

### Steps

1. **Open Content Browser**
   - Window → Content Browser
   - Navigate to: `/ProjectCoreFeature/` (plugin content root)

2. **Create GameFeatureData Asset**
   - Right-click in Content Browser
   - Miscellaneous → Data Asset
   - **Class:** `GameFeatureData` (search if not visible)
   - **Name:** `ProjectCoreFeature` (CRITICAL - must match plugin name)

3. **Configure Asset**
   - Double-click `ProjectCoreFeature.uasset` to open
   - **Initial State:** Registered
   - **Dependencies:** (empty for now - orchestrator manages its own dependencies)
   - **Actions:** (empty for now - minimal orchestrator)
   - Save asset

4. **Verify 4 Critical Rules**
   ✅ **Rule 1:** Asset name = Plugin name (`ProjectCoreFeature`)
   ✅ **Rule 2:** Location = `Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset`
   ✅ **Rule 3:** .uplugin path = `/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature` (already correct)
   ✅ **Rule 4:** Content-only plugin (no C++ modules) (already correct)

5. **Test Boot Flow**
   ```bash
   # Run smoke test
   scripts/ue/test/smoke/gamefeature_registration.bat

   # Check logs for ProjectCoreFeature activation
   grep "ProjectCoreFeature" Saved/Logs/Alis.log
   ```

6. **Expected Logs**
   ```
   ProjectBoot: Activating orchestrator feature: ProjectCoreFeature
   ProjectBoot: Resolved orchestrator URL via GameFeatures subsystem: file:///ProjectCoreFeature/ProjectCoreFeature.uplugin
   ProjectBoot: Orchestrator 'ProjectCoreFeature' activated successfully.
   ```

---

## What Happens If You Don't Do This?

**System gracefully degrades:**
- ProjectBoot attempts ProjectCoreFeature activation
- Activation fails (asset missing)
- ProjectBoot automatically falls back to ProjectMenuExperience
- User sees menu as expected
- No functional impact

**Why still do it:**
- Completes immutable loader architecture
- Enables future orchestrator update mechanism (Phase 3+)
- Prepares for dynamic GF discovery and version management
- Follows intended design pattern (ROM → orchestrator → features)

---

## After Asset Creation

**Next Steps (Automated):**
1. ✅ ProjectBoot will activate ProjectCoreFeature instead of fallback
2. ✅ Logs will show orchestrator activation success
3. ⏳ Phase 3: Add version checking and update mechanism (agent-compatible)
4. ⏳ Phase 3: Implement first-run prompt for orchestrator installation (agent-compatible)
5. ⏳ Phase 4: Enable orchestrator to discover and activate child features dynamically (agent-compatible)

**No further manual steps required after asset creation.**

---

## Troubleshooting

### Asset created but not activating
- Check Content Browser refresh (F5)
- Verify asset path matches `/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature`
- Check logs for activation errors
- Ensure .uplugin GameFeatureData path is correct

### Fallback still activating
- Check ProjectBootSettings config: OrchestratorFeatureName should be "ProjectCoreFeature"
- Verify no typos in plugin name
- Check GameFeatureData asset class (must be UGameFeatureData, not data asset base)

### Boot screen shows error
- If error persists after asset creation, check logs for specific error message
- Verify GameFeatureData asset is valid (open in editor, check for warnings)
- Ensure plugin is enabled in Plugins browser

---

## Reference Documentation

**Architecture Details:**
- @docs/architecture/plugin_architecture.md (Immutable Loader section)
- @todo/create_architecture_core.md (implementation progress)

**GameFeature Rules:**
- @docs/agents/conventions.md#gamefeature-plugin-requirements-critical

**Manual Intervention Guide:**
- @docs/manual/manual_plugin_architecture.md (detailed version)
- This file (quick reference)

---

## Status Tracking

**Created:** 2025-10-30 (Autonomous CI Session)
**Status:** ⚠️ PENDING
**Blocker:** Requires human with Unreal Editor access
**Impact:** Non-blocking (fallback functional)
**Priority:** Medium (completes architecture, not critical for functionality)

**Estimated Time:** 5 minutes
**Difficulty:** Easy (basic UE editor operations)
**Prerequisites:** Unreal Editor, Alis.uproject loaded
