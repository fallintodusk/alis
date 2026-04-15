# ROM/BIOS Manual Checklist

**Status:** ✅ System functional with fallback - Manual step OPTIONAL for production

---

## Critical Manual Task (5 minutes)

### Create ProjectCoreFeature GameFeatureData Asset

**When Needed:** Production deployment (NOT required for development)

**Steps:**
1. Open Unreal Editor
2. Navigate to: `Plugins/GameFeatures/ProjectCoreFeature/Content/`
3. Right-click → `Game Features` → `Game Feature Data`
4. Name: `ProjectCoreFeature` (EXACT name match)
5. Open asset, configure:
   - Initial State: `Registered`
   - Add any dependency features if needed
6. Save asset
7. Verify: Run `scripts/ue/test/smoke/gamefeature_registration.bat`

**Detailed Guide:** @docs/manual/manual_plugin_architecture.md

---

## Development Workaround (No Manual Step)

**Config:** `Config/DefaultProjectBoot.ini`

```ini
[/Script/ProjectBoot.ProjectBootSettings]
; Development: Use ProjectMenuExperience as fallback
FallbackOrchestratorFeatureName="ProjectMenuExperience"

; Production: Leave empty for pure ROM/BIOS
; FallbackOrchestratorFeatureName=""
```

**Effect:**
- System falls back to ProjectMenuExperience if ProjectCoreFeature asset missing
- Graceful degradation - no blocking errors
- Logs show: "Attempting fallback orchestrator: ProjectMenuExperience"

---

## Verification

**First Start (with fallback configured):**
```bash
# Run editor
UnrealEditor.exe Alis.uproject

# Expected logs:
# "First start detected. Showing orchestrator installation proposal."
# "Auto-accepting orchestrator installation in 3 seconds."
# "Orchestrator 'ProjectCoreFeature' activation FAILED: ..."
# "Attempting fallback orchestrator: ProjectMenuExperience"
# "Orchestrator 'ProjectMenuExperience' activated successfully."
# "First-start complete. Orchestrator 'ProjectMenuExperience' installed and configured."
```

**Subsequent Starts:**
```bash
# Config now has: OrchestratorFeatureName="ProjectMenuExperience"
# Expected logs:
# "Activating orchestrator feature: ProjectMenuExperience"
# "Orchestrator 'ProjectMenuExperience' activated successfully."
```

---

## Production Deployment Checklist

- [ ] Create ProjectCoreFeature.uasset (5 min with editor)
- [ ] Test activation: `scripts/ue/test/smoke/gamefeature_registration.bat`
- [ ] Remove fallback: Set `FallbackOrchestratorFeatureName=""`
- [ ] Test first-start flow in packaged build
- [ ] Verify config persists: Check `Saved/Config/*/ProjectBoot.ini`

---

## Troubleshooting

**Issue:** "Orchestrator activation failed and no fallback available"
**Fix:** Add fallback to config (see Development Workaround above)

**Issue:** First-start UI loops on every boot
**Fix:** Delete config file: `Saved/Config/*/ProjectBoot.ini`

**Issue:** Orchestrator fails to activate
**Fix:** Check 4 GameFeature rules:
1. Asset name = Plugin name (ProjectCoreFeature.uasset)
2. Location: `Plugins/GameFeatures/ProjectCoreFeature/Content/`
3. .uplugin path: `/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature`
4. Content-only (no C++ modules)

**Detailed Troubleshooting:** @docs/agents/troubleshooting.md#gamefeature-issues

---

## Summary

**Current Status:**
- ✅ ROM/BIOS architecture complete
- ✅ First-start proposal implemented
- ✅ Fallback mechanism working
- ⚠️ ProjectCoreFeature.uasset creation deferred (5 min manual task)

**Impact:**
- **Development:** No manual step needed (uses fallback)
- **Production:** 5-minute manual task required for optimal UX
- **Blocking:** None (system functional with fallback)
