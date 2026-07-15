# Validation Scripts

Scripts for fast validation and compile-adjacent checks without running a full packaged build.

## Structure

- `validate_*.bat` - primary generic validation entry points
- `check.bat` - legacy compatibility router for older `--uht/--all` style usage
- `bp_compile.bat` - legacy compatibility wrapper for Blueprint validation
- `project_validate.bat` - legacy compatibility wrapper for asset validation
- `assets/` - asset-level validation (null materials, soft references, primary asset completeness)
- `blueprint/` - targeted Blueprint validators for project-specific runtime contracts
- `config/` - ini config validation (shipping-unsafe settings, debug overrides)
- `gameplay/` - gameplay-plugin-specific validation checks
- `gamefeatures/` - static GameFeature configuration checks
- `governance/` - architectural or registry audits that are not generic engine validation
- `reports/` - generated validation logs

## Main Entry Points

### Generic validation

- `validate_uht.bat` - Unreal Header Tool validation
- `validate_syntax.bat` - C++ syntax validation without full compile
- `validate_blueprints.bat` - Blueprint compilation
- `validate_assets.bat` - Unreal DataValidation only
- `validate_all.bat` - fast project validation bundle: generic checks plus declared project gating checks

### Specialized validation

- `data/validate_all.bat` - cross-reference data validation (objectId, lootProfileId, DialogueTreeAsset, AudioPresetAsset, dialogue actions/conditions)
- `gameplay/projectmind/validate_data.bat` - ProjectMind data-schema validation (signal_tags vs dialogue trees)
- `gamefeatures/validate_registration.bat` - static GameFeature registration/configuration check
- `governance/validate_legacy_object_parent_generalization.bat` - legacy marker and docs registry audit
- `governance/validate_text_format.bat` - docs/text character-set audit (Cyrillic/CJK by default; optional emoji/typography blocks) plus strict ASCII path audit

### Build-time prevention (pre-package)

- `config/validate_shipping_ini.bat` - pure Python ini audit: flags disabled PSO cache, PathTracing=1, TSR > 100%, missing asset registry deps. No editor required.
- `assets/validate_primary_assets.bat` - editor commandlet: verifies known primary-asset backing classes exist in asset registry (presence check, not full Asset Manager scan validation). Optional-empty types like ProjectAbilitySet warn instead of failing.
- `assets/validate_soft_refs.bat` - editor commandlet: scans level actors for null material slots on StaticMeshComponent and SkeletalMeshComponent (catches "Invalid ShaderMap material (None)" errors)

### Compatibility wrappers

- `check.bat [--uht|--syntax|--blueprints|--assets|--all]` - compatibility router to the explicit `validate_*.bat` scripts
- `bp_compile.bat` - calls `validate_blueprints.bat`
- `project_validate.bat` - calls `validate_assets.bat`
- `data_validate.bat` - calls `validate_assets.bat`

## When To Use

- before pushing code
- after header changes
- after refactors that can break Blueprint references
- after asset moves or data updates

Use the root `validate_*.bat` scripts when you want generic fast validation.

Use the subfolder validators only when you are working in that domain or when `validate_all.bat` explicitly includes them as a project-gating check.

### Packaged build smoke test

- `../test/smoke/packaged_boot_test.ps1` - launches packaged Shipping exe, parses log for PSO hitches (max cumulative), ShaderMap errors, Mutable overflows, CVar spam. Supports `-SecondRun` for comparison, `-NoForceResolution` to observe packaged defaults. NOTE: smoke test, not benchmark - does not clear PSO caches between runs.

## Related Docs

- Build router: [../../../docs/build/README.md](../../../docs/build/README.md)
- Testing router: [../../../docs/testing/README.md](../../../docs/testing/README.md)
- Blueprint editor helpers: [../editor/blueprint/README.md](../editor/blueprint/README.md)
