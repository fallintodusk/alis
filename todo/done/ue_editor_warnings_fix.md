# UE Editor Warnings Fix (2026-01-09)

## Issues Detected

### 1. CRITICAL - Orchestrator Engine Version Mismatch [FIXED]
- **Error:** Manifest requires UE 5.5, but engine is UE 5.7
- **Impact:** Orchestrator rejects manifest, blocks plugin loading
- **Status:** RESOLVED

### 2. LogStreaming - Missing Engine Platform Files
- **Warning:** VisionOS platform assets not found (24x, 128x icons)
- **Warning:** Missing ButtonHoverHint.png, doc_16x.png
- **Impact:** None (editor-only UI, non-functional issue)
- **Status:** IGNORED (non-critical)

### 3. LogLinker - Assets with Empty Engine Version
- **Warning:** 2,855 .uasset files saved with empty engine version
- **Affected:** ProjectAudio, ProjectItems, City17 plugins
- **Impact:** Assets load but may have compatibility warnings
- **Status:** OPEN (non-blocking)

---

## Solutions Applied

### Orchestrator Manifest Update
**File:** `Plugins/Boot/Orchestrator/Data/dev_manifest.json:3`

```json
"engine_build_id": "++UE5+Release-5.7-CL-48512491"
```

**Result:** Orchestrator now accepts current engine version

---

## Prevention Strategy

### 1. Engine Version Management
- **Action:** Update dev_manifest.json when upgrading UE version
- **Location:** `Plugins/Boot/Orchestrator/Data/dev_manifest.json:3`
- **Frequency:** Every engine upgrade (5.5 → 5.6 → 5.7)

### 2. Asset Version Tracking
- **Option A (Recommended):** Ignore warnings - assets are functional
- **Option B:** Batch resave via Python/Blueprint automation (15-30min)
- **Option C:** Resave on-demand when editing assets

### 3. Continuous Validation
- **Pre-commit hook:** Check manifest engine version matches project
- **CI/CD:** Automated check for version mismatches
- **Log monitoring:** Filter critical vs non-critical warnings

---

## Resolution Options for Asset Warnings

| Option | Time | Benefit | Drawback |
|--------|------|---------|----------|
| A. Ignore | 0 min | No work | Warnings persist |
| B. Batch resave | 30 min | Clean logs | Upfront time cost |
| C. On-demand | Variable | Gradual fix | Incomplete coverage |

**Recommendation:** Option A (ignore) - warnings are cosmetic, assets work fine.

---

## Status Summary

- [FIXED] Critical error (Orchestrator version)
- [IGNORED] Streaming warnings (VisionOS) - non-critical
- [OPEN] Asset version warnings (2,855 files) - cosmetic

**Next Steps:** Monitor editor startup for new errors after manifest fix.
