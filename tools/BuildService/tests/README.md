# Build Service Test Sources

| Boundary | Source |
|---|---|
| Manifest validation | `integration/manifest_validation_test.rs` |
| Pin and state workflow | `integration/pin_workflow_test.rs` |
| CDN Control API publication | `integration/publisher_container_test.rs` |
| Composed smoke workflow | `smoke/e2e_workflow_test.rs` |

Execution and environment requirements are owned by
[Build Service testing](../docs/testing.md). Crate-local tests remain beside
their implementation.
