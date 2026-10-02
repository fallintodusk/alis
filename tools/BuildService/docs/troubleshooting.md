# Build Service Troubleshooting

## Wrapper or Rust failure

Run the narrowest wrapper check from `tools/BuildService`:

```powershell
.\scripts\build.ps1 check
.\scripts\build.ps1 test --workspace
```

The wrapper propagates Cargo's exit code. Confirm CMake, NASM, Cargo, and the
MSVC toolchain are available when dependency compilation fails.

## Engine identity or tool path failure

Build Service resolves the source engine through the project-owned
`scripts/config/ue_path.conf`. Verify that configuration with the project's
engine tooling; do not add another engine path to Build Service configuration.

## No units selected

Default build selection compares current files with `BuildUnit.yaml`
fingerprints. Use `--plugin <name>` for one unit or `--force` for all units.
Inspect the descriptors before deleting local state.

## Unreal build or packaging failure

Use the emitted Unreal command, log path, unit name, and build ID. Reproduce
through Build Service so its engine pins, sanitized project, and state handling
remain in the route. Do not replace the command with an ad hoc `Build.bat`
invocation.

## Publication failure

Stage/promote require the configured Control API and token environment
variable. Promote additionally requires a matching source-release receipt.
Missing or mismatched receipts fail closed; rebuild from the intended source
rather than editing a receipt.

External CDN tests are explicitly selected and are not proven by a local
workspace pass. See [testing](testing.md).
