# Script Development

Extend the existing command owner before adding a parallel wrapper.

| Operation | Owner |
|---|---|
| Configure repository-local tooling | `scripts/config/` or `scripts/setup/` |
| Build Unreal targets | `scripts/ue/build/` |
| Validate without building | `scripts/ue/check/` |
| Run Unreal automation | `scripts/ue/test/` |
| Run editor or packaged executables | `scripts/ue/run/` |
| Package and verify releases | `scripts/ue/package/` |
| Realize World data in Unreal | `scripts/ue/world/` |
| Mirror public Git source | `scripts/git/` |
| Host a standalone application | `tools/` |
| Provide a small engine-independent helper | `scripts/utils/` |

## Contract

- Use lowercase underscore names that describe one operation.
- Resolve paths from the script location or repository configuration; do not
  embed a developer machine path.
- Propagate child exit codes and make terminal failure visible.
- Validate all inputs before durable or external mutation.
- Put transient output under the repository `tmp/` hierarchy.
- Keep secrets outside arguments, logs, and tracked files.
- Document the supported entry point in the nearest README and remove replaced
  wrappers in the same change.

Use [the architecture decision guide](docs/architecture.md) for ownership and
[the scripts router](README.md) for existing entry points.
