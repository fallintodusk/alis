# Finish historical source replay and durability qualification

**Status:** BACKLOG - PARTIALLY IMPLEMENTED. Raw retention and recovery exist;
exact historical Kazan replay and independent storage/access qualification remain
unverified. This is not a prerequisite for completed Terrain development or
World-only validation from accepted canonical authority.

## Goal

Recover and verify the exact accepted historical raw inputs when available, and
qualify their durable recovery/access without silently changing source identity.
Do not implement another archive system: extend the current owner only if a
remaining requirement demonstrates a missing capability.

## Retained authority

- **D1:** Recover/download the Kazan raw file and close the loss gap properly,
  including regeneration. Source: operator request, 2026-10-04.
- **D2:** Create a reviewable investigation todo for external review before
  implementing the raw-source change. Source: operator request, 2026-10-04.
- **D3:** Keep downloaded public vendor files in a clean hierarchy outside
  disposable `tmp/` and `Saved/`. Source: operator request, 2026-10-04.
- **Q1 [UNRESOLVED]:** If exact pinned historical bytes cannot be recovered,
  stop or propose a new source identity and complete source/canonical/World
  acceptance? This still gates substitution, not canonical-authority World work.
- **Q2 [UNRESOLVED FOR EXTERNAL DISTRIBUTION]:** Which independent archive target
  and developer distribution/access policy is authorized if external storage is
  required? Existing local retention does not authorize cloud writes or prove
  independent hardware/backup durability. Reassess against the actual remaining
  recovery requirement before proposing another transport.

## Implemented - retain, do not rebuild

- SourceIngestion already has primary/replica retention outside the checkout,
  pinned-size/hash verification, per-object locks, staging/atomic replacement,
  recovery before the provider URL, and rejection of overlapping archive/cache
  roots. Both verified copies precede successful provider-ingestion publication.
- The CLI preflights storage and preserves accepted result markers on failure.
  Unknown storage I/O failures propagate; corrupt copies cannot satisfy recovery.
- The focused retention suite passes 9 tests (2026-11-16, 0.183 s), including
  cache-loss/replica recovery without network and refusal before accepted result
  publication. This is fixture proof, not historical replay or disaster recovery.
- The coherent Ground acquisition has a retained two-store receipt in the
  completed Terrain execution record. It does not recover the missing historical
  Kazan PBF or prove physically independent backup failure domains.

## Remaining evidence and stop conditions

The production [Kazan source profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json)
owns the historical PBF date, expected bytes, hashes and rights. The 2026-10-04
investigation recorded a provider 404 and missing exact cached bytes. Current
availability has not been rechecked in this backlog cleanup; do not convert that
historical observation into a current global-absence claim.

- [ ] When selected, inventory authorized existing retained copies/backups for
  the exact profile identity. Accept only matching bytes; a nearby date or OSM
  history reconstruction is not exact historical replay. No new download now.
- [ ] If found, exercise current recovery and full source/canonical replay;
  compare with promoted authority before any World mutation. If not found,
  resolve Q1 before preparing a separately reviewed new-source migration.
- [ ] Qualify actual independent failure domains, backup restore and authorized
  developer access. Use current retention first; resolve Q2 before external
  writes/access changes only if that requirement remains unsatisfied.
- [ ] Prove disposable-cache loss and wrong/missing/unreadable copies on the
  chosen deployment, source rights and unchanged authority on refusal. Add only
  regression coverage missing for a demonstrated implementation change.

No provider payload enters public Git merely because local retention exists.
Do not change canonical/source identity or claim historical recovery from a
successful World-only Matrix. External writes, acquisition budgets and authority
promotion require their own bounded authorization.

## Owners and verification routes

- [Raw retention and recovery](../../../tools/World/SourceIngestion/retention/README.md).
- [Acquisition and source admission](../../../tools/World/SourceIngestion/README.md).
- [Focused retention regressions](../../../tools/World/SourceIngestion/tests/test_retention.py).
- [World proof layers and canonical-authority reuse](../../../docs/testing/world_pipeline_layers.md).

Close only after the chosen historical-source disposition, recovery/access proof
and accurate rights/provenance are recorded. The implemented retention mechanism
is complete within its own tested scope; this remaining task must not recreate it.
