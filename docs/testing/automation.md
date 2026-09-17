# Unreal Test Automation

The supported development entry point is
`scripts/ue/test/unit/iterate.ps1`. It validates filter shape, synchronizes
Live Coding when configured, prefers the persistent editor when available, and
falls back to the cold runner.

## Exact Development Loop

```powershell
.\scripts\ue\test\unit\iterate.ps1 -TestFilter "Project.Full.Exact.Test.Name"
```

Use `run_single.ps1` when compile-mode selection is not required:

```powershell
.\scripts\ue\test\unit\run_single.ps1 "Project.Full.Exact.Test.Name"
```

Both entry points reject wildcards, unions, short prefixes, group shapes, and
tag expressions in normal development mode.

## Acceptance Gates

Only end-of-slice or explicitly requested suite work uses broad selection:

```powershell
.\scripts\ue\test\unit\iterate.ps1 -Mode Gate -TestFilter "Project.Accepted.Prefix"
.\scripts\ue\test\unit\iterate.ps1 -Mode Gate -Tags "Fast"
```

The wrapper owns timeout, dispatch, and exit-code behavior. Do not bypass it by
constructing a direct UnrealEditor command line.

## Evidence

- Reports: `Saved/Automation/Reports/`
- Unreal logs: `Saved/Logs/`
- Persistent-editor state: `scripts/ue/test/artifacts/persistent/`

The command exit code and the matching report/log together own the result. A
log snippet without the selected test identity is diagnostic, not acceptance.
