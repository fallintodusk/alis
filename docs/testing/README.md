# Testing Documentation

The specific guide for automated testing strategies, tiers, and execution in Alis.

> **Quick Decision:** [Which test should I run?](#decision-tree)

## Decision Tree

**Q: What type of test do you need?**
- **[Smoke Test](#smoke-tests)** (< 5 min) -> Critical path / Pre-commit.
- **[Integration Test](#integration-tests)** (5-30 min) -> Multi-system / Nightly.
- **[Unit Test](#unit-tests)** (< 1 min) -> Single system / TDD loop.

**Q: What task are you doing?**
- **Run existing tests** -> `make test` or `make test-unit`
- **Write new test** -> See [Writing Tests guide](unit_tests.md#writing-tests)
- **Debug failure** -> See [Troubleshooting & Debugging](troubleshooting.md)
- **Setup CI** -> See [Automation](automation.md)

---

## Test Pyramid

```
        /\
       /  \      Unit Tests (<1 min each)
      /____\     - Single system, isolated
     /      \    - Mocked dependencies
    /________\
   /          \  Integration Tests (5-30 min)
  /____________\ - Multi-system, real dependencies
 /              \
/________________\ Smoke Tests (<5 min total)
                   - Critical path, pre-commit
```

**Strategy:** More unit tests (fast), fewer integration tests (comprehensive), minimal smoke tests (critical paths only).

---

## Test Framework Overview

We use a **Two-Tier Architecture**:

### 1. Tier 1: Unit & Plugin Tests
- **Location**: Inside each Plugin (e.g., `Plugins/UI/ProjectMenuMain/Source/ProjectMenuMainTests/`)
- **Speed**: Fast (<1s), Mocked dependencies.
- **Goal**: Verify internal logic validation (ViewModels, parsers, state machines).
- **Run**: `make test-unit`

### 2. Tier 2: Integration Tests
- **Location**: `Plugins/Tests/ProjectIntegrationTests/`
- **Speed**: Slower (seconds), Real subsystems.
- **Goal**: Verify cross-plugin wiring (Boot -> Menu -> Loading -> World).
- **Run**: `make test-integration`

[Read detailed philosophy (Tier 1 vs Tier 2)](unit_tests.md#tier-1-vs-tier-2)

---

## Test Types

### [Smoke Tests](smoke_tests.md)
*Health checks for critical paths.*
- **Commands**: `scripts/ue/test/smoke/boot_test.bat`
- **When**: Before every commit.

### [Integration Tests](integration_tests.md)
*Multi-system validation.*
- **Commands**: `scripts/ue/test/integration/autonomous_boot_test.bat`
- **When**: Nightly CI, Pre-release.

### [Unit Tests](unit_tests.md)
*Isolated logic verification.*
- **Commands**: `scripts/ue/test/unit/run_cpp_tests.bat`
- **When**: Every build (TDD).

---

## Advanced Topics

- **[Automation Pipeline](automation.md)** - CI/CD setup.
- **[Agent UE Inspection](agent_ue_inspection.md)** - Autonomous UE state validation (layouts, widgets).
- **[Manual Verification](troubleshooting.md#manual-tests-visual-ux)** - Human checklists.

## External Tools (Rust)

- **[Tools Testing Strategy](../../tools/docs/rust_testing.md)** - Logic for BuildService/Launcher (Rust) testing (Mocks vs Remote).

## Quick Reference

| Command | Description |
|---|---|
| `make test` | Run **ALL** automated tests. |
| `make test-unit` | Run all Tier 1 unit tests. |
| `make test-integration` | Run all Tier 2 integration tests. |
| `make test-quick` | Run smoke tests only. |

## Detailed Command Reference

**Primary Script:** `scripts\win\test.bat`

| Task | Command |
|------|---------|
| Unit tests | `scripts\win\test.bat --unit` |
| Integration tests | `scripts\win\test.bat --integration` |
| Specific module | `scripts\win\test.bat --filter <name>` |
| All tests | `scripts\win\test.bat --all` |

## Log Locations

| Type | Path |
|------|------|
| Test reports | `Saved/Automation/Reports/` |
| Full UE log | `Saved/Logs/Alis.log` |
| Crashes | `Saved/Crashes/` |
