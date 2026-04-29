# Perf: Capture Unreal Insights Mutable Channels for Right-Sizing Working Memory

## Problem

`mutable.WorkingMemory` is currently set to 256 MiB (`Config/DefaultEngine.ini`) as a pragmatic cap over the observed City17 outdoor-entry pending demand of ~240 MiB. The original 100 MiB engine default and intermediate 210 MiB raise were both reached by trial-after-overflow, not measurement. The current 256 MiB value is verified holding (no `LogMutableCore: Failed to keep memory budget` entries in `ALIS_20260428_113909` Shipping log), but if a future content addition pushes the peak past 256 MiB, the cycle of "raise budget after overflow" repeats unless we right-size from real data.

Per [Epic Mutable runtime docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-resource-usage-at-runtime-in-unreal-engine), runtime generation cost is best understood via Unreal Insights with the Mutable channels enabled. The working-memory limit is a flush hint, not a hard cap.

## Expected

A documented Mutable peak demand for the worst-case content scenario (City17 + densest character spawn + worst Mutable instance update), with `mutable.WorkingMemory` sized from that measurement plus a small headroom margin. Includes a one-paragraph note in `docs/agents/canonical.md` or the Mutable plugin docs explaining the methodology so the next person facing a Mutable spill knows the playbook instead of bumping the int by guesswork.

## Reopen criteria (when this leaves backlog)

Promote from backlog to active only if any of these surface:

- `LogMutableCore: Failed to keep memory budget` reappears in any Shipping log with `Budget: 262144` (current 256 MiB cap).
- A new content drop (additional MetaHuman base, costume pack, character density bump) makes the worst-case Mutable instance update materially heavier.
- Memory pressure / OOM symptoms on minimum-spec hardware that aren't explained by other systems.

## Procedure (when activated)

1. Build editor with Insights instrumentation enabled (default in Editor / DebugGame).
2. Launch `UnrealInsights.exe` separately.
3. Run `Alis.exe` (or Editor PIE) with `-tracehost=127.0.0.1 -trace=default,mutable,memory,counters`.
4. Reproduce worst-case path: cold start, Continue from save, City17 outdoor entry, force a character mesh update (equipment swap or instance update tick).
5. In Insights, enable Mutable channels (CPU + memory). Identify peak Mutable allocation against time. Capture both `Current` and incoming `New` deltas.
6. Compare measured peak to current 256 MiB budget. Set `mutable.WorkingMemory` to `peak * 1.1` rounded to a clean power of 2.
7. Document the methodology + measured peak in the Mutable plugin docs (or canonical.md if it's policy-level guidance).

## Don't

- Don't bump `mutable.WorkingMemory` by guesswork on overflow without measuring first. The point of this task is to break that cycle.
- Don't enable Insights instrumentation in a Shipping build for normal users; this is dev/QA-only.

## Files

- `Config/DefaultEngine.ini` (line ~25, `mutable.WorkingMemory`)
- `docs/agents/canonical.md` or `Plugins/Local/Mutable/.../docs/...` (where the right-sizing methodology gets recorded)

## References

- Origin: `todo/01_done/perf_outdoor_lag_shipping.md` R4 + R6 sections (item closed pragmatically at 256 MiB on 2026-04-28; R6 spun out of that doc as forward-facing trigger).
- Epic docs: [Mutable Resource Usage at Runtime](https://dev.epicgames.com/documentation/en-us/unreal-engine/mutable-resource-usage-at-runtime-in-unreal-engine).
- Engine-backed guidance in `docs/agents/canonical.md` (Mutable section).
