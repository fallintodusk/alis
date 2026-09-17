# Unit and Focused Tests

Place a test with the module that owns the behavior. Cross-module fixtures
belong in `Plugins/Test/ProjectIntegrationTests` only when no single component
can own the acceptance boundary.

## Run One Test

```powershell
.\scripts\ue\test\unit\iterate.ps1 -TestFilter "Project.Full.Exact.Test.Name"
```

Use an exact registered test name. This is the default loop for production
logic, parsers, ViewModels, state machines, and focused regressions.

## Authoring Contract

- Name tests by owner, behavior, and expected outcome.
- Keep fixtures deterministic and local to the owning module.
- Assert the externally meaningful result, not an implementation step.
- A reproduced bug gains a focused regression that is observed failing before
  the fix and passing after it.
- Use Unreal automation flags appropriate to the real execution envelope.

For filter and dispatch behavior, see [Automation](automation.md). The script
router is [scripts/ue/test](../../scripts/ue/test/README.md).
