# Engine Version Update Execution

Permanent contracts:

- [Engine update workflow](../../../docs/ue_engine/version_update.md)
- [Engine configuration SOT](../../../scripts/config/ue_path.conf)
- [Build workflow](../../../docs/build/workflow.md)

Do not edit engine pins or derived machine-local files by hand. Use the
orchestrator and repair only the first causal compatibility failure.

## Launcher update

- [x] Centralize launcher/source roots and strict cross-language resolution.
- [x] Add the single machine-local writer and drift validation.
- [x] Add resumable apply, verification invalidation, and rollback automation.
- [x] Build the editor and pass boot plus unit gates on the target launcher.
- [x] Restart VS Code and the MCP host so they inherit the new engine cache.
- [x] Resume the recorded run through development package and package boot.
- [x] Reach `DEV_COMPLETE`.

## Source release

- [x] Fail fast while the source engine is absent, unbuilt, or on another line.
- [x] Gate BuildService promotion with a verified source-release receipt.
- [x] Build the matching source engine.
- [x] Wire and verify the `-CompleteSource` child run.
- [x] Reach `SOURCE_COMPLETE`.

Run status and logs are generated under `Saved/EngineUpdate/`; they are not
copied into this TODO.
