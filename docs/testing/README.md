# Testing

Public testing router for ALIS.

Use this section to understand which tests exist, when to run them, and where the main automation entry points live.

## Decision Guide

If you need:
- a fast health check -> [smoke_tests.md](smoke_tests.md)
- multi-system validation -> [integration_tests.md](integration_tests.md)
- isolated logic verification -> [unit_tests.md](unit_tests.md)
- automation pipeline details -> [automation.md](automation.md)
- UI dump and agent-readable inspection -> [agent_ue_inspection.md](agent_ue_inspection.md)
- debugging and manual verification notes -> [troubleshooting.md](troubleshooting.md)

## Test Layers

### Smoke tests

Critical-path health checks for boot and basic usability.

- Docs: [smoke_tests.md](smoke_tests.md)
- Typical command: `scripts/ue/test/smoke/boot_test.bat`

### Integration tests

Cross-plugin and cross-system validation.

- Docs: [integration_tests.md](integration_tests.md)
- Typical command: `scripts/ue/test/integration/autonomous_boot_test.bat`
- Main integration test plugin: `Plugins/Test/ProjectIntegrationTests/`

### Unit tests

Fast, isolated verification for plugin or subsystem logic.

- Docs: [unit_tests.md](unit_tests.md)
- Typical command: `scripts/ue/test/test.bat --unit`

## Common Commands

```powershell
.\scripts\ue\test\test.bat --unit
.\scripts\ue\test\test.bat --integration
.\scripts\ue\test\smoke\boot_test.bat
```

## Related Public Routes

- Scripts router: [../../scripts/README.md](../../scripts/README.md)
- Test scripts: [../../scripts/ue/test/README.md](../../scripts/ue/test/README.md)
- Build router: [../build/README.md](../build/README.md)
