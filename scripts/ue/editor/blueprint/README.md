# Blueprint Editor Helpers

Editor-side Blueprint tooling lives here.

Use this folder for scripts that inspect or mutate Blueprint assets in Unreal Editor or `UnrealEditor-Cmd`.
If a script validates Blueprint state without owning the edit logic, put it under `scripts/ue/check/blueprint/` instead.

## Layout

- `helpers/` - reusable Blueprint asset helpers
- `character/` - narrow character-specific wrappers built on top of the generic helpers
- `docs/` - longer-form notes for the JSON snapshot workflow
- `tests/` - diagnostics and experiments
- `deprecated/` - old one-off scripts kept only for reference
- `export_blueprint.py`, `import_blueprint.py`, `config.py` - JSON snapshot workflow

## Current Helpers

- `helpers/set_blueprint_component_transform.py`
  - edits a Blueprint component template through `SubobjectDataSubsystem`
  - use this when inherited component overrides must be changed on the asset template itself
- `character/fix_bp_hero_character_mesh_defaults.bat`
  - aligns `BP_Hero` inherited `CharacterMesh0` transform with native `ProjectCharacter`
  - recompiles Blueprints after the template edit so generated defaults stay in sync

## Why Component Template Helpers Live Here

Blueprint component overrides are editor-owned asset state.
They are not the same responsibility as runtime validation, gameplay code, or content checks.

That split is intentional:
- edit helpers belong in `scripts/ue/editor/blueprint/`
- validation wrappers belong in `scripts/ue/check/blueprint/`

## Usage

Character-specific repair:

```bat
scripts\ue\editor\blueprint\character\fix_bp_hero_character_mesh_defaults.bat
```

Generic helper via environment variables:

```bat
set ALIS_BP_ASSET_PATH=/ProjectObject/Human/Hero/BP_Hero
set ALIS_BP_COMPONENT_VARIABLE_NAME=Mesh
set ALIS_BP_COMPONENT_OBJECT_NAME=CharacterMesh0
set ALIS_BP_RELATIVE_LOCATION=0,0,-90
set ALIS_BP_RELATIVE_ROTATION=0,-90,0
```

Then run `UnrealEditor-Cmd` with:

```bat
-run=pythonscript -script="scripts\ue\editor\blueprint\helpers\set_blueprint_component_transform.py"
```

## Related Docs

- [docs/README.md](docs/README.md) - JSON snapshot workflow index
- [../check/blueprint/README.md](../check/blueprint/README.md) - Blueprint validation wrappers
