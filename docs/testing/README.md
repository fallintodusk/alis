# Testing

Testing is routed by the boundary being accepted, not by a desire to run the
largest available suite.

| Need | Owner |
|---|---|
| Iterate on one exact Unreal automation test | [Unit and focused tests](unit_tests.md) |
| Verify a cross-component contract | [Integration tests](integration_tests.md) |
| Check a critical boot or packaged route | [Smoke tests](smoke_tests.md) |
| Understand dispatch, filters, and reports | [Automation](automation.md) |
| Select a World pipeline evidence layer | [World pipeline layers](world_pipeline_layers.md) |
| Inspect UI state for an agent | [UE inspection](agent_ue_inspection.md) |
| Verify character parity | [Character parity](character_parity.md) |
| Diagnose failures | [Troubleshooting](troubleshooting.md) |

Normal development runs one exact test:

```powershell
.\scripts\ue\test\unit\iterate.ps1 -TestFilter "Project.Full.Exact.Test.Name"
```

Broad filters, tags, suites, and matrices are acceptance gates. Use them only
with the explicit gate mode described in [Automation](automation.md).
