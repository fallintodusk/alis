# Test and Runtime Troubleshooting

Start at the first ownership boundary whose observable output differs from the
expected result. Do not rerun a broad suite to discover a focused defect.

## First Checks

1. Record the exact command, executable, build configuration, map, RHI, and
   selected test identity.
2. Read the terminal exit code, matching automation report, and Unreal log.
3. Reproduce the smallest exact test in the same execution envelope.
4. Compare with a known-good envelope only when that comparison distinguishes
   an environment failure from a product failure.

Use the protocol in [Scientific Debugging](../agents/scientific_debugging.md)
when evidence contradicts a previously green gate.

## Routes

| Symptom | Owner |
|---|---|
| Build, UHT, or link failure | [Build workflow](../build/workflow.md) |
| Test selection, no match, timeout, or stale binaries | [Automation](automation.md) |
| Early manifest or module activation failure | [Boot chain](../loading/boot_chain.md) |
| Experience resolution or loading-phase failure | [ProjectLoading](../../Plugins/Systems/ProjectLoading/README.md) |
| Missing or detached UI | [ProjectUI troubleshooting](../../Plugins/UI/ProjectUI/docs/troubleshooting.md) |
| Generated World or streaming failure | [World pitfalls](../../Plugins/World/ProjectWorld/docs/pitfalls.md) |
| Packaged build differs from Editor | [Packaging](../build/packaging_guide.md) |

## Evidence Locations

- Unreal logs: `Saved/Logs/`
- Automation reports: `Saved/Automation/Reports/`
- Project-owned transient diagnostics: `tmp/<domain>/<component>/`

Do not delete the failing artifact before collecting the measurements needed
to identify the first broken edge.
