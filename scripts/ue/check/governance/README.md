# Governance Checks

Architecture and registry audits live here.

Keep a validator in this folder when it checks repo conventions, marker registries, or documentation/code alignment rather than runtime asset validity.

## Validators

- `validate_legacy_object_parent_generalization.ps1` -- legacy actor-parent generalization audit.
- `validate_plugin_data_staging.py` -- audits plugins that read JSON from `Plugins/<X>/Data/` at runtime via `FProjectPaths::GetPluginDataDir(...)` and confirms each plugin's `.Build.cs` stages the directory through either the canonical `StageDataDir(Target)` helper or a per-file `RuntimeDependencies.Add` for `Data/`. Catches the silent class of bug where a feature works in Editor but its data file never ends up in cooked Shipping. Wired into `validate_all.bat` and `scripts/ue/package/package_release.ps1` pre-package validation.
- `validate_no_alis_prefix.py` -- enforces the `Project*` prefix rule. Scans first-party `.cpp/.h` under `Source/` and `Plugins/` (skipping the allowed `Source/Alis/` game module + vendored plugins) for forbidden `Alis*` declarations: UCLASS/USTRUCT/UENUM/UINTERFACE, `LogAlis*` log categories, `ALIS_*` non-API macros, and `Alis*.{cpp,h}` filenames. SOT: `docs/architecture/principles.md` "Universal Naming Convention". Wired into `validate_all.bat`. Commit-time enforcement only (no PreToolUse hook -- per-edit Python startup at ~85 ms doesn't scale across multiple governance rules).
- `validate_licensing.py` -- checks license-text and legal-doc links from the root `LICENSE`, explicit local ALIS declarations, all tracked Cargo package metadata, unexpected Epic/`LicenseRef` notices in first-party UE source, and locally declared public-mirror exclusions. It intentionally does not infer ownership or complete path classification, generate release evidence, certify dependency completeness, validate asset provenance, assemble notices, or certify Epic compliance. Wired into `validate_all.bat`.
- `generate_component_manifest.py` -- generates reproducible effective license
  evidence from a clean public source tag and fails closed on unknown
  boundaries, ownership notices, excluded components, and asset provenance.
- `validate_text_format.py` -- checks docs/text content for disallowed character blocks such as Cyrillic and CJK, and checks tracked paths for any non-ASCII character. Use `--all-blocks` to include emoji and typography checks, and `--all-text` to widen content scanning beyond `.md/.dsl/.txt`. The public mirror runs this against the filtered snapshot with `--all-text`. Direct repo path scans currently remain manual because historical asset paths contain known Cyrillic homoglyphs that must be renamed before this can join `validate_all.bat`.
