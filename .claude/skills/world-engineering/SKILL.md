---
name: world-engineering
description: >-
  Use this skill for non-trivial ALIS World engineering: investigation,
  decomposition, architecture, implementation, debugging, performance
  diagnosis, or independent review involving world data, canonical
  compilation, Unreal realization, generated layers, persistence/rollback,
  World Partition/streaming, or visual/operator evidence. Before changing
  behavior, identify the owning black box and contract, declare expected
  changed/untouched components, route to current SOTs, and stop on unexpected
  cross-boundary propagation. Do not use for unrelated ALIS systems or simple
  read-only questions that need no World-specific workflow.
metadata:
  author: alis-team
  version: "1.0"
---

# World Engineering

Use this skill as a thin engineering router. It owns HOW to work on ALIS World safely; repository SOTs own WHAT the current architecture, policies, data, commands, and phase state are.

Do not copy changing World details into this skill. Do not hardcode current territory counts, hashes, phase names, algorithms, runtime-grid values, engine version numbers, or current blockers.

## Core loop

For any non-trivial World task:

1. Locate the owning black box and its public contract.
2. Open only the current SOTs needed for that concern.
3. State expected components CHANGED and UNTOUCHED.
4. Prefer the existing interface/extension point and current native engine capability.
5. Make the smallest coherent change or investigation.
6. Prove the claim on the correct acceptance surface and execution envelope.
7. Stop when the boundary, evidence, or operator gate says stop.

For a small local change whose ownership and contract are already obvious, keep this lightweight: revalidate the owner and boundary, make the focused change, run the focused proof. Do not manufacture a large design exercise.

## First route

Start from repository-root paths. Read only the branch the task needs.

| Need | Open first |
|---|---|
| Continuing or resuming current World work | `todo/README.md`, then locate the current World slice |
| World ownership / architecture / current domain SOT | `Plugins/World/ProjectWorld/docs/territory_generation.md` |
| World tooling / source-to-canonical pipeline | `tools/World/README.md` |
| World Partition, runtime streaming, performance | `Plugins/World/ProjectWorld/docs/world_partition.md` |
| Visual presence, screenshots, operator evidence | `tools/World/VisualVerification/README.md` |
| Known Unreal realization incident or pitfall | `Plugins/World/ProjectWorld/docs/pitfalls.md` |
| Test depth, proof traceability, review cadence, change locality | `docs/testing/world_pipeline_layers.md` |
| Generated-world transaction, rollback, manifests, authority | `scripts/ue/world/README.md` |
| Reality contradicts green evidence, repeated failed hypotheses, unclear failure | `docs/agents/scientific_debugging.md` |
| Live Editor / MCP control, only when driving the Editor | `docs/ue_engine/mcp_editor_control.md` |

From those routers, open the owning deeper SOT only when needed. Typical examples include `architecture_overview.md`, `territory_contract.md`, `world_partition.md`, `pitfalls.md`, Canonical Compilation, End-to-End Validation, and Visual Verification docs.

Do not preload the World documentation tree.

Two different authorities, and they are not interchangeable:

```text
stable SOT    = architecture, contracts, ownership, policy
current plan  = execution status, accepted decisions, evidence,
                falsified hypotheses, blockers, unchecked work
```

A resuming agent that reads only stable SOTs can be perfectly correct about
how the system works while repeating investigation that was already done or
already falsified. Read the current slice for state; read the SOT for design.

A current plan never overrides a stable SOT. When an active slice exists, never
infer current work from a done or archived plan.

## Establish the boundary before changing behavior

Before substantial coding or design, record briefly:

```text
owning black box:
public interface / contract:
expected components CHANGED:
expected components UNTOUCHED:
```

The names must come from current repository ownership, not from memory.

If implementation unexpectedly requires editing an owner declared UNTOUCHED, treat that as ARCHITECTURAL RED. Stop expanding the diff and review the boundary first.

A local feature requiring unrelated generators, persistence, validation, or runtime owners to change is not automatically "more implementation". It may show the extension point or ownership boundary is wrong.

## Architecture and decomposition

When ownership, contracts, or extension shape are not already accepted:

- reconstruct the current path from stable SOTs before proposing a new abstraction;
- identify which design decision each black box hides from its neighbors;
- prefer high cohesion and low cross-owner change propagation over smaller file/class counts;
- extend existing registries, interfaces, schemas, DAGs, transactions, and adapters before adding parallel machinery;
- keep stable semantics closed where appropriate and use the designated extension axis for new implementations;
- prefer one authority with small projections/adapters over synchronized independent copies;
- state invariants, acceptance surfaces, execution envelopes, and stop conditions before expensive implementation;
- use current official engine/tool documentation and the exact installed engine source when a native capability or behavior is material.

Do not add a framework, compatibility layer, alternate transaction path, second authority, or generic abstraction without a concrete failed invariant that the existing boundary cannot satisfy.

## Implementation

When the boundary is already accepted:

- revalidate the owner and contract; do not redesign by default;
- touch only the smallest coherent set of expected owners;
- preserve declared UNTOUCHED owners;
- prefer existing project helpers and native engine APIs over bespoke wrappers;
- keep production policy out of reusable logic when a profile/data owner already exists;
- keep generated authority, authored overlays, runtime state, and test data in their existing ownership domains;
- avoid unrelated cleanup and "while I am here" edits;
- keep the diff reversible until the explicit durable-authority boundary.

If a new requirement invalidates the accepted boundary, stop implementation and return to architecture/decomposition rather than forcing the feature through the wrong interface.

## Debugging

Use normal focused debugging for obvious local failures.

Immediately route to `docs/agents/scientific_debugging.md` when any of these occurs:

- reality contradicts a green gate;
- the measured acceptance surface is not what the test actually exercised;
- the execution envelope may differ from the real/shipping path;
- two reasonable implementation hypotheses are falsified;
- repeated patches move the symptom without explaining the mechanism.

Then prefer:

```text
smallest exact reproduction
-> exact acceptance surface
-> exact execution envelope
-> known-good control
-> one hypothesis at a time
-> installed source / native instrumentation when semantics remain unclear
```

Separate creation defects, persistence defects, recovery defects, and presentation defects unless evidence proves they share a cause.

Do not patch around an unexplained engine/tool behavior when the exact native implementation can be inspected.

## Performance and streaming

Do not optimize from an Editor symptom alone.

Before changing performance architecture:

1. identify the real runtime envelope;
2. measure actual residency/streaming behavior;
3. classify Game/CPU, Render/Draw, GPU, memory, or streaming pressure;
4. use native profiling/streaming tools before custom instrumentation;
5. change only the owner responsible for the measured bottleneck.

Do not tune Nanite, materials, proxy counts, batching, HLOD-like mechanisms, or custom streaming machinery merely because they are plausible optimizations.

If the measured runtime already meets the current prototype/acceptance budget, stop optimizing.

## Proof discipline

Use `docs/testing/world_pipeline_layers.md` to choose proof depth. Do not encode its current labels into this skill.

Always distinguish:

- ACCEPTANCE evidence: proves a stated invariant on its actual acceptance surface;
- DIAGNOSTIC / NON-AUTHORITATIVE evidence: helps investigate but gates nothing.

For acceptance:

- every invariant needs a final proof;
- every acceptance proof must map to an invariant;
- the execution envelope must authenticate the boundary being accepted;
- a proof in a materially different envelope is diagnostic unless the SOT explicitly says otherwise.

Use the cheapest focused proof during iteration. Run expensive integration, rendered, durable-authority, or release proofs only when their boundary is actually crossed.

Do not rerun a broad gate merely because another edit happened; rerun when the changed code/data/authority invalidates what that gate previously authenticated.

## Operator and authority boundaries

Treat explicit operator approval and durable authority as hard stops.

Do not:

- commit or push for the operator;
- silently promote/enroll production authority;
- turn diagnostic screenshots/traces into authority;
- hand-edit generated production output to make an acceptance check pass;
- bypass project wrappers that own locking, rollback, manifests, or active authority.

When the current SOT requires operator visual approval, production enrollment, or another human decision, stop with the evidence needed for that decision.

## Independent review mode

Use the same skill for architecture/implementation review, but the reviewer must be a fresh non-author agent/session.

The reviewer should reconstruct the boundary independently from current SOTs, then inspect the plan/diff/evidence for:

- correct owner and public contract;
- expected CHANGED versus UNTOUCHED locality;
- unnecessary coupling or parallel machinery;
- native/current capability reuse;
- invariant-to-proof traceability;
- acceptance surface and execution envelope correctness;
- diagnostic evidence incorrectly presented as acceptance;
- authority/operator boundaries;
- scope expansion and unnecessary complexity.

Return `PASS`, `PATCH`, or `BLOCKER` according to the current repository review contract. Do not author the implementation fix while acting as the independent reviewer; the same reviewer may recheck the implementer's correction.

## Context and maintenance discipline

This skill is intentionally stable and small.

It should change only when the reusable World engineering workflow changes. Current architecture and operational facts belong in their owning SOTs.

Prefer routing to an existing stable document over adding detail here. If a branch of this skill starts accumulating substantial specialized procedure, first ask whether that procedure is already owned by a repository SOT. Create another skill only after a genuinely separate repeatable workflow is proven in practice.

## Success condition

A good World change should be understandable as:

```text
one clear owner
-> one explicit contract / extension point
-> localized implementation
-> focused evidence
-> independent review
```

If a supposedly local change repeatedly propagates through unrelated black boxes, treat that as evidence to improve the architecture boundary before continuing feature work.
