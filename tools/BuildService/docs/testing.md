# Build Service Testing

Run Build Service checks through the repository wrapper from
`tools/BuildService`:

```powershell
.\scripts\build.ps1 test --workspace
```

The wrapper configures the native dependencies used by the Rust workspace and
propagates Cargo's exit code.

## Test Boundaries

| Boundary | Test owner |
|---|---|
| Crate-local parsing, planning, state, and packaging behavior | Tests beside each crate |
| Manifest validation and pin workflow | `tests/integration/` |
| Real CDN Control API publication | `publisher_container_test.rs` |
| Composed artifact-to-publication flow | `tests/smoke/e2e_workflow_test.rs` |

Tests requiring an external CDN stack are marked ignored and must be selected
explicitly in an environment that owns that stack. A local workspace pass does
not prove external publication or promotion.

See [the test-source router](../tests/README.md) and
[Build Service architecture](architecture/README.md).
