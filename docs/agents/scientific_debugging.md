# Scientific Debugging

Standing method for ALIS. Read this BEFORE changing behavior whenever reality contradicts
a green gate, an acceptance surface turns out to be the wrong surface, or two
implementation hypotheses have been falsified.

Landed 2026-08-17 after the Kazan flat-terrain incident, in which every structural gate
was green while the shipped terrain was completely flat. The rules below are generalized
from that failure; none of them are terrain-specific.

Router: [docs/agents/canonical.md](canonical.md#evidence-driven-debugging).

## 0. Current-engine native tools first

Before building custom diagnostics, or declaring an engine limitation, inspect
the EXACT installed engine version:

1. installed engine source under the resolved engine root;
2. current-version documentation and release notes;
3. built-in Insights workflows, console commands and automation APIs.

Older-version documentation is historical evidence only until verified against
the installed version. A feature introduced in an older release is not thereby
wrong; what matters is that its CURRENT implementation is confirmed. But never
stop researching at an old doc page when the installed version ships newer
tooling for the same problem.

Two incidents motivate this, both 2026-08-18:

- Screenshot capture was hand-rolled (console camera moves, focus stealing,
  sleeps, hash diffing) before checking that
  `Engine/Source/Developer/FunctionalTesting/` has shipped
  `TakeHighResScreenshot`, `AScreenshotFunctionalTest` and
  `TakeAutomationScreenshotAtCamera` since 4.27.
- A World Partition streaming question was approached through screenshots and
  `obj list` when UE 5.8 ships `Engine/Plugins/WorldStreamingInsights` -- a
  spatial profiler for cell state, priority and memory -- plus
  `wp.Runtime.DumpStreamingSources` and the `wp.Runtime.ToggleDraw*` family.

For performance and streaming problems specifically: **inspect the exact
runtime execution envelope before varying world-generation data.** An Editor
world with every actor resident is a visual diagnostic environment, not runtime
performance authority. Measuring it and reporting the result as a product
signal is an execution-envelope error of the same class as rail 6.

## 1. Evidence rails

Five rules. Each one is the direct cause of a real multi-hour failure in which every gate
was green while the shipped artifact was wrong. They generalize past terrain; apply them
to any generated artifact.

1. **Acceptance measures the representation consumed at the boundary being accepted.**
   Test the OUTPUT of the transformation under test, never one of its inputs. Kazan's
   Generated Base edit layer matched canonical on all 215,040 samples while the final
   composed heightmap - the surface that renders, collides, and sets bounds - was flat at
   raw height 0, max error 395.8 m. Correct input to a transform proves nothing about its
   output. The boundary sets the surface: for Landscape L1/L2 it is the final heightmap;
   at L4 it may be the cooked/rendered result. Everything upstream is diagnostic.
   Corollary: never persist a generated artifact before its acceptance surface has passed
   its invariant. Verify, then save; save/reload is a separate persistence proof.

2. **No `*_verified` booleans in evidence. Emit metrics.**
   A boolean cannot distinguish "measured" from "correct". `terrain_final_surface_verified`
   was wired to "did the read succeed" and reported `true` on a 100%-wrong artifact. Emit
   sample/expected counts, mismatch count, max error, tolerance, min/max, and an identity
   hash, then derive acceptance from those so a validator can reject independently.

3. **A determinism hash is not a correctness proof.**
   Hashing what the engine produced answers "did we produce the same thing again", not
   "did we produce what the input specified". An all-zero heightmap hashes perfectly
   stably. Correctness requires comparison against the specifying input; keep the hash for
   reconstruction/no-op identity only.

4. **One dirtiness authority. Caches are hints, never vetoes.**
   The layer planner selected `final_dirty_units = ["*"]`, then a per-component
   `ProjectWorld.TerrainInput=<hash>` tag suppressed all 210 cells, so
   `updated_landscape_components = 0` and broken bytes re-authenticated themselves
   forever. An identity/cache hint must never cancel work an explicit planner selected
   without validating the realized output. Selected-dirty means reevaluate; a zero-write
   result is fine only when the realized artifact is proven already correct.

5. **A gate nobody has watched fail is not a gate.**
   Land every new correctness check with an observed RED - either against the broken
   artifact or a deliberately sabotaged fixture - before trusting its GREEN. Skipping the
   RED is what let a check that measured the wrong surface look like proof.


6. **The execution envelope is part of the input.**
   Before varying data, scale, algorithms, or architecture, reproduce the smallest known
   fixture under the EXACT failing execution envelope: executable, commandlet vs editor
   mode, command-line flags, RHI/headless/render mode, platform and build configuration,
   and relevant environment/config. Then run the same fixture in the known-good envelope
   as a control. If `same input + different execution envelope -> different acceptance
   output`, localize the environment difference before touching domain logic. A fixture
   proven in an interactive Editor does NOT authenticate the same code path under
   `-NullRHI`, a cooked runtime, CI, a dedicated server, or any other materially different
   mode. This incident cost hours because a correct Editor result was treated as proof for
   a `-NullRHI` commandlet; the cheap discriminator was always "same tiny fixture, exact
   production flags".

Corollary, scoped: when correctness depends on third-party/engine behavior that our own
tests do not already prove, inspect that exact installed version's source/API contract and
quote `file:line` before proposing implementation semantics. Do not go engine-source
reading for ordinary ALIS bugs. In this incident the generic "merge never completes"
hypothesis died in minutes against `LandscapeDataAccess.cpp` plus one fixture - but the
`-NullRHI` hypothesis was dismissed too early on that same evidence and turned out to be
the actual root cause. Falsifying a hypothesis in ONE execution envelope does not falsify
it in another.

## 2. Scientific debugging protocol

The rails above are lessons. This is the method. It is the DEFAULT behavior whenever a
nontrivial bug survives an apparently green test or gate.

**Contradiction means diagnose, not patch.** The moment evidence conflicts - tests green
but reality wrong, inputs correct but output wrong - stop changing behavior and localize.

```text
 1. State the expected invariant.
 2. Name the acceptance surface (the representation consumed at this boundary).
 3. Preserve and MEASURE the broken artifact before mutating it. Evidence first.
 4. Reproduce the SMALLEST known fixture under the EXACT failing execution
       envelope (see rule 6). Then run the same fixture in the known-good
       envelope as a control. Do this BEFORE varying any domain input.
 5. Draw the shortest observable chain:
       input -> transform -> intermediate -> acceptance artifact -> derivative
 6. Find the FIRST edge where actual != expected.
 7. State ONE falsifiable hypothesis.
 8. Design the cheapest experiment that DISTINGUISHES it from the alternatives.
 9. Change one variable only.
10. If falsified: discard the hypothesis and any hypothesis-specific
       behavior/workaround. KEEP independently useful diagnostics, acceptance
       probes, and regression infrastructure.
11. Patch only the boundary proven wrong, only after localization.
12. Observe RED on the real acceptance surface.
13. Make that SAME test GREEN.
14. Prove persistence, no-op, and recovery.
```

Keep a throwaway hypothesis ledger while diagnosing - hypothesis, prediction, experiment,
result, status. Once a hypothesis is FALSIFIED it must not drive further implementation.

**Two-strike reset.** After two falsified implementation hypotheses, or the moment a
previously trusted acceptance surface is found to be the wrong surface, enter
diagnosis-only mode. Allowed: inspect, instrument, read installed source, focused
experiments, sabotage fixtures. Forbidden: architecture changes, compatibility work, broad
test runs, speculative fixes, new abstractions. Exit only when one failing boundary is
experimentally localized.

**Creation defects and recovery defects are separate hypotheses.** Never infer the
historical cause of bad state from discovering why current recovery cannot repair it.
Kazan proved that a stale artifact could not self-heal (cache identity vetoed planner
dirtiness); that proved nothing about what originally produced the flat terrain.

Correctness tests never ship quarantined or expected-failure. RED is something you
observe during the session; the final tree is green.

## 3. Worked example (the incident that produced this document)

```text
invariant   canonical elevation must survive into the Landscape that renders
surface     final/base heightmap  (NOT the Generated Base edit layer)
chain       canonical -> Generated Base -> UE blend -> final heightmap -> bounds/collision

H1  generic edit-layer merge failure
    experiment  fresh twin in normal-RHI Editor, measure SOURCE and FINAL
    result      both correct                                          FALSIFIED
    kept        the final-surface reader built for it - it became the acceptance probe

H2  -NullRHI prevents Landscape edit-layer composition
    dismissed early on H1's Editor-only evidence  <- THE MISTAKE
    experiment  same twin, exact production commandlet envelope
    result      Editor  FINAL 0 mismatches, relief 10.10 m
                NullRHI FINAL 8192 mismatches, relief 0, min = max = -256 m
    source      LandscapeEditLayers.cpp PrepareTextureResources:
                  if (Info == nullptr || !FApp::CanEverRender()) return false;
                -> RDG batched merge never runs -> final heightmap stays flat
    status      CONFIRMED - root cause

H3  the Kazan territory is a stale pre-fix artifact
    experiment  Delete to absence, full rebuild, 210 components, dirty ["*"]
    result      still flat, byte-identical outcome                    FALSIFIED
```

H1 is why rule 10 keeps diagnostics: the hypothesis was wrong, the instrument it produced
is now permanent acceptance evidence. H2 is why rule 6 exists: it was dismissed on
evidence gathered in the WRONG execution envelope, which cost the entire investigation.


H1 is the reason rule 9 says to keep diagnostics: the hypothesis was wrong, the
instrument it produced is now permanent acceptance evidence.

