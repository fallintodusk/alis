# Replace Third-Party Plugins with Native Flow

**Status: 2 of 3 done. InstanceArrayTool remains.**

## Completed

### DialoguePlugin -> ProjectDialogue [DONE]

- Plugin removed from `Alis.uproject`
- `ProjectDialogue` + `ProjectDialogueUI` fully implemented
- Integration tests in `ProjectIntegrationTests`
- Only stale doc refs remain in `docs/architecture/content_structure.md`
  and `docs/gameplay/dialogue_guide.md` (cleanup minor)

### LowEntryExtStdLib [DONE]

- Plugin removed from `Alis.uproject`
- Zero C++ dependencies (was Blueprint-only)
- Cleanup: remove from `tools/BuildService/crates/executor/src/sanitize.rs`
  allowlist and `Alis_sanitized.uproject`

## Remaining

### InstanceArrayTool -> Procedural Building System

- Plugin still active in `Alis.uproject` (editor tool for ISM/HISM placement)
- Source at `Plugins/InstanceArrayTool/` (3 modules: Runtime, Editor, ActorArray)
- **Replacement:** JSON-driven procedural buildings via UE5 PCG + C++

See backlog: `todo/02_backlog/world/procedural_buildings.md`

### Cleanup tasks

- [ ] Remove stale "DialoguePlugin" refs from docs
- [ ] Remove LowEntryExtStdLib from sanitize.rs allowlist
- [ ] Remove LowEntryExtStdLib from Alis_sanitized.uproject
