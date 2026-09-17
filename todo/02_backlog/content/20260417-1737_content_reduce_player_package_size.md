# Reduce Player Package Size

Status: uninvestigated

## Current gap

The current Windows player payload is approximately 5 GiB and contains a large
default chunk. The dominant dependency roots must be measured from the current
IoStore output before exclusions or asset rewrites are selected.

## Investigation boundary

- Produce a fresh per-container and largest-entry report from the supported
  package route.
- Trace Motion Matching, Mutable, MetaHuman, sample, map, and default-chunk
  roots through the Asset Registry and cook rules.
- Remove only unused dependencies with reference and packaged-runtime proof.
- Preserve Kazan, Manhattan, City17, character, and developer payload routes.
- Measure size and startup/runtime behavior before and after each candidate.

Archive splitting is a transport concern and does not reduce installed size.
