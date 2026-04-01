# Blueprint Validation

Blueprint-specific validation checks live here instead of the top-level `check/` folder.

This keeps the separation clear:
- `scripts/ue/editor/blueprint/` mutates or inspects Blueprint assets
- `scripts/ue/check/blueprint/` verifies Blueprint state without owning the edit logic

## Current Validators

- `validate_fp_body_defaults.bat` - compares `BP_Hero` inherited component defaults with native `ProjectCharacter`

## Usage

```bat
scripts\ue\check\blueprint\validate_fp_body_defaults.bat
```

## Scope

This validator is intentionally project-specific.

Keep checks here when they verify a stable runtime contract for a known Blueprint/native pairing.

Do not wire one-off investigation scripts or temporary repro exporters into the generic `check.bat --assets` flow.
