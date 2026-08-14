# Prepare the World Slice 2 Flow

Status: done on 2026-08-12.

Purpose: remove Slice 1 iteration noise before any Slice 2 implementation.
Permanent contracts remain in the linked SOTs; this file is only the bounded
execution checklist.

## Sources

- [World pipeline layers](../../../docs/testing/world_pipeline_layers.md)
- [World tool router](../../../tools/World/README.md)
- [Territory generation](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Canonical realization](../../../scripts/ue/world/README.md)

## Checklist

- [x] Verify the current ownership split and L0-L4 pipeline need no redesign.
- [x] Replace the multi-slice active plan with one lean Slice 2 execution todo;
  preserve completed Slice 0/1 evidence and later-slice planning outside the
  active queue.
- [x] Freeze a compact Slice 2 invariant sheet before implementation.
- [x] Split deep generated-authority mechanics out of the territory router so
  a fresh agent can import only the detail needed for its task.
- [x] Verify the one-command enrollment timing. Keep it as a hard Slice 3
  precondition if Slice 2 has neither Unreal mutation nor a territory E2E
  profile to authenticate the command.
- [x] Encode the review cadence: design review before code, system review after
  focused tests, then one final affected L2 boundary.
- [x] Verify routes, links, ASCII, exact focused tests, and fresh-agent command
  discovery without running Matrix, L3, or L4.
- [x] Stop `scripts/ue/world/README.md` edits from impersonating generator
  implementation changes in the static planner; cover the boundary exactly.
- [x] Prepare the Slice 1 diff for external review and an operator-approved
  commit boundary; do not commit or start Slice 2 implicitly.

## Done when

- A fresh agent can identify the current slice, its invariants, exact owner
  commands, and required test layer from the routing docs in under five
  minutes without reading completed slice history.
- The cleanup task is archived under `todo/01_done/`.
