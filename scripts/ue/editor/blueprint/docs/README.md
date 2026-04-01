# Blueprint Docs

This folder contains longer-form notes for the Blueprint JSON snapshot workflow.

## Scope

These docs are mainly about:
- exporting Blueprint defaults to JSON
- editing those snapshots outside Unreal
- importing the edited snapshot back into the asset

Runtime validation does not live here.
Use `scripts/ue/check/blueprint/` for validation wrappers.

Component-template mutation helpers also do not live here.
Use `scripts/ue/editor/blueprint/helpers/` for reusable editor-side asset edits.

## Current References

- `QUICKSTART.md` - shortest path through the JSON export/import workflow
- `USAGE.md` - detailed usage examples
- `LIMITATIONS.md` - current API limits and tradeoffs
- `CLAUDE_WORKFLOW.md` - AI-assisted snapshot editing flow
- `EXAMPLE_SESSION.md` - example operator session

## Important Note

Older docs in this folder describe UE 5.5-era limitations around direct Blueprint component access.
That limitation does not apply to the newer subobject-template helper flow in:

`scripts/ue/editor/blueprint/helpers/set_blueprint_component_transform.py`

That helper edits component templates through `SubobjectDataSubsystem`, which is the correct path for inherited Blueprint component overrides.
