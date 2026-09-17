# Integration Tests

Integration tests prove a contract that crosses two or more component owners.
They do not duplicate unit coverage for each implementation detail.

## Placement

- Keep a narrow provider-consumer contract test with the owning provider when
  that module can construct the real boundary.
- Use `Plugins/Test/ProjectIntegrationTests` for project composition and
  multi-plugin behavior.
- Keep environment-dependent acceptance with the script or tool that owns the
  environment.

## Execution

Iterate with one exact registered test:

```powershell
.\scripts\ue\test\unit\iterate.ps1 -TestFilter "ProjectIntegrationTests.Full.Exact.Test.Name"
```

Use `-Mode Gate` only for an accepted broad integration envelope. A passing
provider unit test does not replace evidence through the real consumer path.

See [Automation](automation.md) for dispatch and
[Troubleshooting](troubleshooting.md) for failure triage.
