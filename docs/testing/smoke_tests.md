# Smoke Tests

Smoke tests answer whether a supported critical route can start and reach its
minimal acceptance surface. They are not substitutes for focused regressions.

| Route | Entry point |
|---|---|
| Editor boot health | `scripts/ue/test/smoke/boot_test.bat` |
| Packaged boot | `scripts/ue/test/smoke/packaged_boot_test.ps1` |
| Cross-system boot integration | `scripts/ue/test/integration/autonomous_boot_test.bat` |

Inspect the selected script before using it as evidence: editor, commandlet,
packaged client, and Shipping are different execution envelopes. The test must
consume the representation being accepted.

For ordinary code iteration, run one exact test through
[the automation route](automation.md) before escalating to a smoke gate.
