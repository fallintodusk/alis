# Manual Intervention: Plugin Architecture

**Critical steps that require human with Unreal Editor access**

---

## CRITICAL: ProjectCoreFeature GameFeatureData Asset

**Status:** [!] **BLOCKING** - Automated setup complete, asset creation required

The immutable loader architecture is 95% automated. The final step requires creating a binary asset in Unreal Editor that agents cannot generate.

---

## What Was Automated (Complete)

[OK] **Plugin Structure:**
- Directory: `Plugins/GameFeatures/ProjectCoreFeature/`
- `.uplugin` file with correct configuration
- Content folder structure
- Content-only (no C++ modules)

[OK] **System Configuration:**
- `ProjectBootSettings.OrchestratorFeatureName = "ProjectCoreFeature"`
- `ProjectBootSettings.FallbackOrchestratorFeatureName = "ProjectMenuExperience"`
- `GameFeaturesSubsystemSettings` search paths configured
- `DefaultEngine.ini` updated for dynamic discovery

[OK] **Fallback Mechanism:**
- Automatic fallback to ProjectMenuExperience if asset missing
- 20 unit tests validating fallback logic
- Boot flow continues without blocking
- Error logging with diagnostics

[OK] **Code Implementation:**
- `OnOrchestratorStateChanged` callback in ProjectBootFlowController
- `ActivateOrchestratorByName` method with fallback support
- Comprehensive error handling and recovery

---

## What Requires Manual Creation

[X] **ProjectCoreFeature.uasset** (GameFeatureData asset)

**Location:** `Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset`

**Why agents cannot create this:**
- `.uasset` files are binary Unreal Engine assets
- Agents can only create text files (C++, .uplugin, JSON, config)
- GameFeatureData requires UE Editor UI to configure properly
- Binary format is engine-internal and version-specific

---

## Step-by-Step Creation Guide

### Prerequisites
- Unreal Editor 5.5 installed
- Project loaded successfully
- ProjectCoreFeature plugin visible in Plugins window

### Steps

1. **Open Unreal Editor**
   ```
   Launch: Alis.uproject
   Wait for editor to fully load
   ```

2. **Navigate to Plugin Content Folder**
   ```
   Content Browser -> Settings -> Show Plugin Content (enable)
   Navigate to: Plugins/ProjectCoreFeature/Content
   ```

3. **Create GameFeatureData Asset**
   ```
   Right-click in Content folder
   -> Miscellaneous -> Data Asset
   -> Select: GameFeatureData
   -> Name: ProjectCoreFeature
   ```

4. **CRITICAL: Verify Asset Name**
   ```
   Asset name MUST be exactly: ProjectCoreFeature
   Extension: .uasset (automatic)
   Location: Plugins/GameFeatures/ProjectCoreFeature/Content/

   Final path: Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset
   ```

5. **Configure GameFeatureData Asset**
   ```
   Double-click ProjectCoreFeature.uasset to open

   Initial State: Loaded (default is fine)

   Actions to Add (optional, for orchestrator logic):
   - Add Data Registry (if needed)
   - Add World Actions (if needed)
   - Add Component requests (if needed)

   For minimal orchestrator, leave empty initially.
   Save asset.
   ```

6. **Verify Plugin Reference**
   ```
   Open: Plugins/GameFeatures/ProjectCoreFeature/ProjectCoreFeature.uplugin

   Confirm line exists:
   "GameFeatureData": "/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature"

   This was set by automated setup.
   ```

---

## Verification

### Step 1: Check Asset Exists
```bash
# From repository root
ls -la Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset

# Should show the binary asset file
```

### Step 2: Run GameFeature Registration Test
```bash
# Smoke test to verify all GameFeatures register correctly
scripts/ue/test/smoke/gamefeature_registration.bat

# Expected output:
# [OK] ProjectCoreFeature registered
# [OK] ProjectMenuExperience registered
# [OK] ProjectCombat registered
# [OK] ProjectDialogue registered
# [OK] ProjectInventory registered
```

### Step 3: Test Boot Flow
```bash
# Launch editor and check logs
# Open: Saved/Logs/Alis.log

# Search for:
grep "ProjectBoot.*Orchestrator" Saved/Logs/Alis.log

# Expected:
# [ProjectBoot] Activating orchestrator: ProjectCoreFeature
# [ProjectBoot] Orchestrator activated successfully
```

### Step 4: Test Fallback (Optional)
```bash
# Temporarily rename asset to test fallback
mv Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset \
   Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset.backup

# Launch editor, check logs
grep "Fallback" Saved/Logs/Alis.log

# Expected:
# [ProjectBoot] Orchestrator activation failed, attempting fallback
# [ProjectBoot] Activating fallback orchestrator: ProjectMenuExperience
# [ProjectBoot] Fallback orchestrator activated successfully

# Restore asset
mv Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset.backup \
   Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset
```

---

## Critical Rules (MUST Follow)

### Rule 1: Asset Name = Plugin Name
```
Plugin Name:      ProjectCoreFeature
Asset Name:       ProjectCoreFeature.uasset
                  ^^^^^^^^^^^^^^^^^^^
                  MUST BE IDENTICAL
```

**Why:** UE GameFeatures system requires exact match for registration.

**Violation Result:** Plugin fails to register, fallback activates, warning in logs.

### Rule 2: Asset Location
```
CORRECT:   Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset
WRONG:     Plugins/GameFeatures/ProjectCoreFeature/Content/Data/ProjectCoreFeature.uasset
WRONG:     Plugins/GameFeatures/ProjectCoreFeature/ProjectCoreFeature.uasset
```

**Why:** .uplugin references `/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature` which resolves to Content folder.

**Violation Result:** Asset not found, fallback activates.

### Rule 3: Asset Type
```
CORRECT:   GameFeatureData (Miscellaneous -> Data Asset -> GameFeatureData)
WRONG:     Primary Data Asset
WRONG:     Blueprint Class
WRONG:     Data Table
```

**Why:** Only GameFeatureData assets work with UGameFeaturesSubsystem.

**Violation Result:** Type mismatch error, plugin fails to activate.

### Rule 4: Content-Only Plugin
```
CORRECT:   "Modules": [] (empty array in .uplugin)
WRONG:     "Modules": [{"Name": "ProjectCoreFeature", ...}]
```

**Why:** Orchestrator must be updatable without code changes (immutable loader principle).

**Violation Result:** Plugin requires code compilation, breaks updatability.

---

## Troubleshooting

### Issue: Asset Not Found After Creation
**Symptom:** Logs show "GameFeatureData not found" warning

**Check:**
1. Asset name exactly matches plugin name (case-sensitive)
2. Asset is in Content folder, not subdirectory
3. .uplugin GameFeatureData path is correct
4. Editor restarted after asset creation

**Fix:**
```bash
# Verify .uplugin path
cat Plugins/GameFeatures/ProjectCoreFeature/ProjectCoreFeature.uplugin | grep GameFeatureData

# Should show:
"GameFeatureData": "/ProjectCoreFeature/ProjectCoreFeature.ProjectCoreFeature"

# Restart editor if asset just created
```

### Issue: Plugin Fails to Register
**Symptom:** GameFeature registration test shows ProjectCoreFeature missing

**Check:**
1. Plugin enabled in Editor: Edit -> Plugins -> search "ProjectCoreFeature" -> Enabled checkbox
2. Content Browser showing plugin content: Settings -> Show Plugin Content
3. Asset type is GameFeatureData, not other asset type

**Fix:**
```
1. Open Plugins window
2. Search: ProjectCoreFeature
3. Enable checkbox (if disabled)
4. Restart editor
5. Verify asset in Content Browser
```

### Issue: Fallback Always Activates
**Symptom:** Logs show fallback to ProjectMenuExperience every boot

**Cause:** ProjectCoreFeature asset missing or invalid

**Check:**
```bash
# Verify asset exists
ls -la Plugins/GameFeatures/ProjectCoreFeature/Content/ProjectCoreFeature.uasset

# If missing, create following guide above
# If exists, check it's GameFeatureData type in editor
```

---

## Fallback Behavior (Safe Operation)

**System continues functioning even without manual asset creation:**

1. **Boot Flow:**
   - ProjectBoot attempts to activate ProjectCoreFeature
   - Activation fails (asset not found)
   - OnOrchestratorStateChanged callback fires
   - Automatic fallback to ProjectMenuExperience
   - Boot completes successfully

2. **User Experience:**
   - No visual difference to user
   - Same menu functionality
   - No crashes or errors shown to player

3. **Logging:**
   - Warning logged: "Orchestrator activation failed"
   - Info logged: "Attempting fallback orchestrator"
   - Success logged: "Fallback orchestrator activated"

4. **Testing:**
   - 20 unit tests validate fallback logic
   - See: `Plugins/Core/ProjectBoot/Source/ProjectBootTests/Private/Unit/ProjectBootFallbackTests.cpp`

**Conclusion:** Asset creation is recommended but not critical for daily operation. System gracefully degrades to previous orchestrator.

---

## Implementation Status

**Automated by Autonomous CI Session (2025-10-30):**
- [OK] ProjectBoot orchestrator activation refactored
- [OK] ProjectBootSettings configured with orchestrator names
- [OK] Fallback mechanism implemented and tested
- [OK] GameFeaturesSubsystemSettings configured
- [OK] DefaultEngine.ini updated for dynamic discovery
- [OK] Plugin structure created with correct .uplugin

**Requires Manual Work:**
- [X] ProjectCoreFeature.uasset creation (this guide)

**Architecture Achievement:**
- [OK] 6/7 checklist items complete
- [!] 1/7 requires editor access (binary asset)
- [OK] Immutable loader achieved (ProjectBoot fully decoupled)
- [OK] Fallback mechanism robust (no blocking issues)

---

## Additional References

- **Detailed Architecture:** [docs/architecture/plugin_architecture.md](../architecture/plugin_architecture.md)
- **Autonomous CI Session Summary:** [artifacts/overnight/manual-intervention-orchestrator.md](../../artifacts/overnight/manual-intervention-orchestrator.md)
- **GameFeature Conventions:** [conventions.md](conventions.md#gamefeature-plugin-requirements-critical)
- **Unit Tests:** `Plugins/Core/ProjectBoot/Source/ProjectBootTests/Private/Unit/ProjectBootFallbackTests.cpp`

---

## Next Steps After Asset Creation

1. **Verify Registration:**
   ```bash
   scripts/ue/test/smoke/gamefeature_registration.bat
   ```

2. **Test Boot Flow:**
   ```bash
   # Launch editor, check orchestrator activation in logs
   grep "Orchestrator" Saved/Logs/Alis.log
   ```

3. **Configure Orchestrator Logic (Optional):**
   - Open ProjectCoreFeature.uasset in editor
   - Add Actions to GameFeatureData:
     - Data Registry sources
     - World actions (spawn actors, add components)
     - Asset bundles for streaming
   - Define dependencies on other GameFeatures
   - Save and test activation

4. **Commit Asset to Repository:**
   ```bash
   git add Plugins/GameFeatures/ProjectCoreFeature/Content/
   git commit -m "feat(architecture): add ProjectCoreFeature orchestrator asset

   Manual creation in Unreal Editor following docs/architecture/modular_features.md

   Completes immutable loader architecture (7/7 checklist items)"
   ```

5. **Update Documentation:**
   - Mark manual intervention complete in `todo/create_architecture_core.md`
   - Update `docs/architecture/plugin_architecture.md` checklist status

---

**Status:** Ready for manual asset creation. All automated setup complete.
