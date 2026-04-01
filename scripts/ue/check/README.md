# Validation Scripts

Scripts for fast validation and compile-adjacent checks without running a full packaged build.

## Structure

- `validate_*.bat` - primary generic validation entry points
- `check.bat` - legacy compatibility router for older `--uht/--all` style usage
- `bp_compile.bat` - legacy compatibility wrapper for Blueprint validation
- `project_validate.bat` - legacy compatibility wrapper for asset validation
- `blueprint/` - targeted Blueprint validators for project-specific runtime contracts
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

- `blueprint/validate_fp_body_defaults.bat` - targeted guard for `BP_Hero` inherited defaults vs native `ProjectCharacter`
- `gameplay/projectmind/validate_data.bat` - ProjectMind data-schema validation
- `gamefeatures/validate_registration.bat` - static GameFeature registration/configuration check
- `governance/validate_legacy_object_parent_generalization.bat` - legacy marker and docs registry audit

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

Use `blueprint/validate_fp_body_defaults.bat` only when working on the legacy first-person hero path or its native component defaults. It is intentionally not part of the generic asset-validation flow.

Use the subfolder validators only when you are working in that domain or when `validate_all.bat` explicitly includes them as a project-gating check.

## Related Docs

- Build router: [../../../docs/build/README.md](../../../docs/build/README.md)
- Testing router: [../../../docs/testing/README.md](../../../docs/testing/README.md)
- Blueprint editor helpers: [../editor/blueprint/README.md](../editor/blueprint/README.md)
