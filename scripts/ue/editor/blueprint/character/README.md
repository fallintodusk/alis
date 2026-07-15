# Character Blueprint Wrappers

This folder contains narrow wrappers for character-specific Blueprint edits.

Use this level only when the operation is strongly tied to a specific character asset.
If the logic is reusable, keep it in `../helpers/` and call it from a wrapper here.

## Current Wrappers

- `create_world_body_retarget_wrapper.bat`
  - creates or refreshes the project-owned retarget wrapper
  - writes the narrow runtime retarget map used by ObjectDefinition mesh data
