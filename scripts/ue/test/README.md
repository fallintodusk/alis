# Unreal Test Scripts

## Development Entry Point

Run one exact test through the guarded iteration wrapper:

```powershell
.\scripts\ue\test\unit\iterate.ps1 -TestFilter "Project.Full.Exact.Test.Name"
```

`unit/run_single.ps1` provides strict exact dispatch without compile-mode
selection. Both routes reject broad filter shapes during normal development.

## Gate Routes

| Boundary | Owner |
|---|---|
| Broad Unreal automation acceptance | `unit/iterate.ps1 -Mode Gate` |
| Editor boot smoke | `smoke/boot_test.bat` |
| Packaged boot smoke | `smoke/packaged_boot_test.ps1` |
| Cross-system boot | `integration/autonomous_boot_test.bat` |
| Character evidence capture | `character/capture_parity.ps1` |
| Inventory evidence capture | [Inventory testing](inventory/README.md) |

Shared process and log helpers under `lib/` are not independent user entry
points. Test selection policy and evidence interpretation are owned by the
[testing documentation](../../../docs/testing/README.md).

The [shared process helper](lib/owned_test_process.ps1) and
[Windows Job adapter](lib/owned_test_process.cs) own suspended startup, descendant
containment, bounded completion and verified cleanup. The
[unit adapter](unit/owned_test_process.ps1) adds the cold-editor preflight;
[composed acceptance](lib/run_owned_process.ps1) reuses the same process owner.
Composed acceptance refuses normal root exit with live descendants, even if
forced cleanup succeeds. The native unit runner explicitly retains its established
worker-retirement policy: retire its own workers, verify the Job empty, retain
the root exit code, then require the exact test census and results. Retirement is
recorded; uncertain cleanup refuses in both policies.
The [real-process regressions](unit/test/ProcessCompletion.Tests.ps1) cover
deadlines, fast intermediate exit, retained output and cleanup failure.

Automated render-capable gates launch Unreal with `-RenderOffScreen` so they
cannot take keyboard or mouse focus. Callers that create a separate Windows
process also hide that process window. Interactive play remains owned by the
standalone run routes and is intentionally not forced offscreen.
