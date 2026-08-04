# Engine Version Update

ONE command drives a normal engine update. Do not follow manual
checklists and do not edit engine paths by hand - the orchestrator owns
every deterministic mutation and calls only existing project routes.

Data SOT: `scripts/config/ue_path.conf` (`UE_PATH` = launcher engine for
daily work, `UE_SOURCE_PATH` = source engine for source-required release
builds and packaging, such as modular launcher/server artifacts or Shipping
with logging;
grammar + authority model in the file header). Second pin:
`Alis.uproject` `EngineAssociation` - derived by the orchestrator from
the target engine's `Engine/Build/Build.version`, never typed by hand.
The Orchestrator development manifest `engine_build_id` is another
derived pin; the same command rewrites and validates it from Build.version.
Launcher and source roots must share Major.Minor. A source patch difference is
allowed only through the source child because its actual Build.version is
recorded and the candidate must pass the full Shipping package and boot gates.

## The command

```powershell
# Dry-run (default): prints the full change plan, creates no run
scripts\ue\update\update_engine.ps1 -LauncherRoot "C:\UnrealEngine\UE_X.Y"

# Execute
scripts\ue\update\update_engine.ps1 -LauncherRoot "C:\UnrealEngine\UE_X.Y" -Apply

# Operate on a run
scripts\ue\update\update_engine.ps1 -Status   [-RunId <id>]
scripts\ue\update\update_engine.ps1 -Resume   [-RunId <id>]
scripts\ue\update\update_engine.ps1 -Rollback [-RunId <id>]

# Complete source-release compatibility after DEV_COMPLETE
scripts\ue\update\update_engine.ps1 -CompleteSource `
  -RunId <dev-run-id> -SourceRoot "G:\UnrealEngine-X.Y"
```

`-SourceRoot` is intentionally rejected on the launcher `-Apply` path. The
source candidate is admitted only by `-CompleteSource`, and its path is written
to the configuration SOT only after the source Shipping gates pass.

What `-Apply` does: preflight (engine identity from Build.version,
managed-file cleanliness, editor/UBT process checks, plugin-pin
inventory) -> stops the persistent editor -> rewrites the SOT files ->
syncs ALL machine-local derived state via `scripts/setup/setup_ue_env.ps1`
(env cache, .vscode, .claude/settings.local.json grants, .mcp.json engine path
and absolute commandlet project root) ->
regenerates project files through `make generate` -> runs full resolver
conformance (PowerShell/batch, Python, shell/Make, and affected BuildService
crates) -> runs project gates (validate_all, editor build, smoke boot, unit
tier, dev cook/package, package boot) -> restores a previously running
persistent editor. Machine-local rewriting moves only paths equal to or below
the previously selected roots; unrelated engine grants are preserved. All
machine-local file outputs are prepared and replaced as one rollback-capable
transaction, then the environment cache is updated last. Run state, logs, and
backups live under `Saved/EngineUpdate/<run-id>/`.

## Agent repair loop (the ONLY agent involvement)

1. Run `-Apply`. The script stops at the first FAILING PHASE and prints
   the log path plus a diagnostic candidate (the FULL log is
   authoritative - determine the causal error yourself).
2. Make the smallest safe compatibility fix for the first causal error;
   no opportunistic refactoring; never hand-edit generated or
   script-managed files.
3. `-Resume`. Any workspace change reruns project generation and ALL
   verification phases as one conservative closure. Repeat until DEV_COMPLETE.
4. Add a regression test when a failure was project behavior rather
   than a mechanical UE API rename.

`ACTION_REQUIRED` pauses are external process restarts the script cannot own
(VS Code and its MCP host). Reload them, then use `-Resume`; that resume is the
operator attestation because a shared VS Code process replacement cannot be
identified reliably. Independently, a fresh MCP stdio client is launched from
the exact `.mcp.json` command and environment and must successfully call
BlueprintMCP `server_status`. The probe proves configuration and protocol
health, not replacement of the existing client process.

## Completion states

- `DEV_COMPLETE`: dev gates passed and the dev package boots on the new
  launcher engine. This is an observable Development package; it does not
  claim Shipping compatibility for precompiled launcher engine binaries.
  Daily work may proceed. Its `completionFingerprint` is captured only after
  editor restoration, operator restart attestation, and MCP protocol health.
  Tracked finalization changes block completion and force the cold closure to
  rerun with the editor stopped.
- Source release stays FROZEN (enforced in code: BuildService and
  source packaging fail fast unless `UE_SOURCE_PATH` is a built source
  engine matching `Alis.uproject`; publishing is receipt-gated) until
  the source engine is re-homed and its Shipping completion gate passes.
- `SOURCE_COMPLETE`: a child of one `DEV_COMPLETE` run has passed source
  identity, a source-engine Shipping package plus archive, and clean plus warm
  packaged boots. This state neither publishes nor signs a release;
  BuildService independently enforces its receipt at promotion. The candidate
  becomes `UE_SOURCE_PATH` only after these gates pass; a blocked child leaves
  the prior source SOT active. Every child stops a previously running persistent
  editor before its cold gates and activates the verified source path. Because
  source UBT writes project module manifests into shared `Binaries/`, the child
  then rebuilds the launcher editor target before restoring the editor and
  completing the external-client attestation/protocol gate.
- `-CompleteSource` first requires the parent launcher Build.version, current
  `Alis.uproject`, current resolved `UE_PATH`, and supplied source root to share
  the parent engine line. If the workspace differs from the parent's final
  completion checkpoint, the immutable parent remains `DEV_COMPLETE` and the
  linked source child owns the development revalidation before source gates.
  A tracked source-child repair or finalization drift latches that closure as
  required across resumes until every development gate succeeds. The child
  records `parent-drift`, `source-child-repair`, or `finalization-drift` as the
  effective reason; source phases cannot reuse evidence from before that event.

## Rollback

`-Rollback` compares ordinary paths with `BASELINE` and managed paths with
`MANAGED-STATE`. It blocks before overwriting a managed file changed after
that checkpoint, and it never crosses a HEAD or submodule-state change. A safe
rollback restores deterministic state, synchronizes machine-local state, and
executes `make generate`; failure is
`ROLLBACK_FAILED`. It NEVER reverts source changes. If semantic repairs exist,
it lists them and does not pretend the old engine builds.

## Follow-ups the script cannot own

- Restart VS Code / MCP host when prompted (ACTION_REQUIRED).
- Re-verify `docs/agents/canonical.md` engine line-number citations
  against the new engine and update its version stamps.
- If `${UE_PATH}` expansion in `.mcp.json` ever fails after a restart,
  rerun `scripts/setup/setup_ue_env.ps1` and check env `UE_PATH`; the
  config phase pinpoints path drift and the external-action probe prevents
  completion until a configured MCP client can call `server_status`.

Enforcement: `scripts/ue/check/governance/validate_engine_env.py` (in
`validate_all`) fails on engine-identity drift, any hardcoded versioned
engine path in tracked text (write `%UE_PATH%` / `%UE_SOURCE_PATH%`
instead), and undocumented `.uplugin` EngineVersion pins.
