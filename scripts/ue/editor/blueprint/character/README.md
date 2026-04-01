# Character Blueprint Wrappers

This folder contains narrow wrappers for character-specific Blueprint edits.

Use this level only when the operation is strongly tied to a specific character asset.
If the logic is reusable, keep it in `../helpers/` and call it from a wrapper here.

## Current Wrappers

- `fix_bp_hero_character_mesh_defaults.bat`
  - repairs `BP_Hero` inherited `CharacterMesh0` transform drift
  - recompiles Blueprints after the asset edit
