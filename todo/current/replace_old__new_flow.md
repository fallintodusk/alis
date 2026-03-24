# Replace Third-Party Plugins with Native Flow

**Mark and plan replacement of third-party plugins (DialoguePlugin, InstanceArrayTool, LowEntryExtStdLib) currently in use.**

Since we are actively using these plugins, removal must be done gradually with migration paths to prevent breaking existing functionality. Create replacement plugins first, then migrate, then remove.

## Current Third-Party Usage

**Plugins/UI/DialoguePlugin:**
- Used for NPC dialogue trees, choice selection, localization
- Integrates with ProjectDialogue workflow once implemented

**Plugins/LEExtend/LowEntryExtStdLib:**
- Used for extended std lib (encryption, zlib, regex, base64, time utilities)
- AlisServer refactoring will address native crypto replacement

**Plugins/InstanceArrayTool:**
- Used for instanced static mesh optimizations (InstanceArrayTool)
- Optimization phase will replace with UE5 ISM/HISM (Instanced Static Meshes / Hierarchical Instanced Static Meshes)

## Replacement Plan

### Phase 1: Assessment (Current)
- [x] Identify current usages in codebase
- [ ] Document all integration points (TODO after code search)
- [ ] Build test cases for current functionality

### Phase 2: Create Replacements
- [ ] Implement ProjectDialogue (Phase 5+) - dialogue trees, localization, UI integration
- [ ] Use UE5 native crypto/RPC for AlisServer (server refactor phase)
- [ ] Migrate ISM optimizations to UE5 native HISM (optimization phase)

### Phase 3: Gradual Migration
- [ ] Feature flag toggle: enable new plugins alongside old
- [ ] Migrate subsystems one-by-one (data, UI, logic)
- [ ] Compatibility layer if needed for transitions

### Phase 4: Removal
- [ ] Full functional testing on all maps/scenarios
- MANUAL: Remove third-party plugins once replacements proven (.uproject edits)
- [ ] Update Docs, .gitignore, plugin lists

## Dependencies

**DialoguePlugin:**
- Depends on replacement: ProjectDialogue (from GameFeatures trio)
- Related: ProjectUI (dialogue widgets), ProjectSession (conversation state)

**LowEntryExtStdLib:**
- Depends on refactor: AlisServer (native crypto, improved RPC)
- Related: Any encryption/authentication features

**InstanceArrayTool:**
- Depends on optimization: UE5 HISM implementation
- Related: Performance profiling, render optimizations

## Risk Mitigation

- Keep third-party backups until full migration complete
- Comprehensive testing at each migration step
- Rollback procedures documented
- Asset/quest validation after each phase

## Timeline

- **Assessment:** Complete by Phase review
- **Replacements Ready:** By Phase 5 (Dialogue), Server refactor (LowEntry), Optimization (IAT)
- **Full Replacement:** End of Phase 6-7

## Notes

- DialoguePlugin most critical, ensure UI/voice acting unaffected
- LowEntry mostly backend, minimal disruption
- IAT is optimization, can be phased in performance mode
