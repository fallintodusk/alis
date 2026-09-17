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
