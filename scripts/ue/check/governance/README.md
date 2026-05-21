# Governance Checks

Architecture and registry audits live here.

Keep a validator in this folder when it checks repo conventions, marker registries, or documentation/code alignment rather than runtime asset validity.

## Validators

- `validate_legacy_object_parent_generalization.ps1` -- legacy actor-parent generalization audit.
- `validate_plugin_data_staging.py` -- audits plugins that read JSON from `Plugins/<X>/Data/` at runtime via `FProjectPaths::GetPluginDataDir(...)` and confirms each plugin's `.Build.cs` stages the directory through either the canonical `StageDataDir(Target)` helper or a per-file `RuntimeDependencies.Add` for `Data/`. Catches the silent class of bug where a feature works in Editor but its data file never ends up in cooked Shipping. Wired into `validate_all.bat` and `scripts/ue/package/package_release.ps1` pre-package validation.
- `validate_no_alis_prefix.py` -- enforces the `Project*` prefix rule. Scans first-party `.cpp/.h` under `Source/` and `Plugins/` (skipping the allowed `Source/Alis/` game module + vendored plugins) for forbidden `Alis*` declarations: UCLASS/USTRUCT/UENUM/UINTERFACE, `LogAlis*` log categories, `ALIS_*` non-API macros, and `Alis*.{cpp,h}` filenames. SOT: `docs/architecture/principles.md` "Universal Naming Convention". Wired into `validate_all.bat`. Commit-time enforcement only (no PreToolUse hook -- per-edit Python startup at ~85 ms doesn't scale across multiple governance rules).
