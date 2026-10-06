# Durable Provider Snapshot Recovery and Replay

**Status:** REVIEW REQUIRED. Investigation only; implementation is not approved by this todo.
**Owner:** World SourceIngestion. **Priority:** full raw replay before source/compiler acceptance.

## Goal

An authorized developer can recover and verify accepted production raw inputs after an origin expires, and replay Kazan from an explicitly accepted source identity.

## Authority register

### Operator decisions

- **D1:** Recover/download the Kazan raw file and close the loss gap properly, including regeneration. Reason: prevent this gap from recurring. Source: operator request, 2026-10-04.
- **D2:** Create a reviewable investigation todo for external review before implementing the raw-source change. Reason: do it properly. Source: operator request, 2026-10-04.
- **D3:** Keep downloaded public vendor files in a clean hierarchy outside disposable `tmp/` and `Saved/`, so accidental cleanup does not remove the only copy and developers can obtain them. Reason: avoid another loss. Source: operator request, 2026-10-04.

### Operator gates

- **Q1 [OPEN - BLOCKING]:** If the exact pinned 2026-08-02 PBF cannot be recovered, should work stop or should a new source identity and full canonical/World acceptance be prepared? Gates substitution of Kazan geography. The operator was asked.
- **Q2 [OPEN - BLOCKING]:** Which durable archive target and distribution/access policy will the operator authorize for raw provider bytes? Gates external archive writes and developer access setup; local candidate design can proceed.

### Working assumptions

- **A1 [ACTIVE]:** Prefer an optional, versioned external artifact archive addressed by SHA-256, plus a verified project-local cache. This does not authorize any cloud write or select a vendor.
- **A2 [ACTIVE]:** Normal World-only development continues from promoted canonical authority without raw downloads.

## Non-goals

- Replacing promoted canonical authority merely to make a World-only Matrix run.
- Putting every public vendor raw file into the normal public Git checkout. Source rights and size differ.
- Treating a nearby date, OSM history reconstruction, or similar extract as the historical PBF's exact bytes.

## Verified evidence

- The Kazan [source profile](../../../Plugins/World/ProjectWorldData/Data/Profiles/SourceIngestion/kazan_territory_v1.source.json) pins the Volga PBF to 769160200 bytes, SHA-256 `e2a149c36a3eb3b33bee50e986006519f9a8255eecedda9e6e0b67e93ee471b1`, MD5 `ce51282556047e18d9de1ef17a946bba`, and a dated Geofabrik URL. It also pins two Copernicus DEM tiles.
- `tools/World/SourceIngestion/app/acquisition.py` validates size and digest but downloads only from the source URL after a cache miss. The Kazan digest directory has no PBF locally. The URL returned HTTP 404 on 2026-10-04. The [Geofabrik region listing](https://download.geofabrik.de/russia/volga-fed-district.html) does not list 2026-08-02, and [Geofabrik's technical page](https://download.geofabrik.de/technical.html) says older files are deleted after a while. Global absence is unverified.
- The Copernicus source records set `admission.boundaries.public_repository_raw_payload` to `not_distributed`, while local verification cache use is approved. [OpenStreetMap's license page](https://www.openstreetmap.org/copyright) requires ODbL attribution and license identification for distributed OSM data.
- [SourceIngestion](../../../tools/World/SourceIngestion/README.md) owns acquisition and provider admission. [World pipeline layers](../../../docs/testing/world_pipeline_layers.md) separates full raw replay from World-only canonical-authority validation. `.gitattributes` tracks Unreal assets and canonical bundles through LFS, but not PBF or TIFF.

**Unknowns:** whether another authorized archive or developer backup has the exact PBF; which archive service, retention and access policy the operator will approve; whether a new extract would change canonical geography.

## Current architecture and problem

The production source profile owns source identity and rights. SourceIngestion owns download, verification, and its disposable content-addressed cache under `tmp/world/source_ingestion/cache/sha256/<digest>/<filename>`. CanonicalCompilation and World consume accepted output downstream. There is no independently retained exact copy of the accepted PBF, so full historical source replay currently stops at acquisition. The cause of the old cache loss is unverified. Promoted canonical authority remains a valid World-only starting point.

## Decision

### Premise / KISS gate

Extend the existing SourceIngestion owner with one optional verified archive transport and an archive-before-new-promotion gate. Keep the cache for speed. This adds no service, polling process, second byte identity, or generic data framework. Normal checkout knowingly gives up automatic raw replay without an optional fetch.

### Proposed flow

1. Search authorized existing backups and mirrors for the exact historical PBF. Accept only a copy whose byte count, SHA-256 and MD5 match the pinned profile. Do not re-encode or rename another date.
2. For each newly admitted production source, download to staging, verify against its profile, publish a durable object keyed by digest with provider/dataset/release metadata, and fetch it back for verification **before** canonical promotion.
3. On full replay, SourceIngestion checks the verified cache, then the configured archive, then the provider URL. Every route yields the same pinned digest or fails closed. A raw archive is optional for World-only canonical-authority runs.
4. The chosen archive owner defines retention and access per source rights. A versioned, deletion-protected object store with a second independent copy is preferred when available; an ignored local folder alone is one failure domain. [Object Lock](https://docs.aws.amazon.com/AmazonS3/latest/userguide/object-lock.html) and [checksum verification](https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity-upload.html) are primary design examples, not a decision to use AWS.
5. If exact historical bytes remain unavailable, resolve Q1. A new snapshot must get a new release/hash/profile and full source -> canonical -> World acceptance with a reviewed generated-authority migration. Its receipt cannot claim historical replay.

### Alternatives

| Option | Why it does not close this gap |
|---|---|
| Ignored local raw folder only | Cleanup can spare it, but another machine or disk loss cannot recover it. |
| Track all raw vendors in Git/LFS | Enlarges developer payload and conflicts with the current Copernicus `not_distributed` raw-publication boundary. [GitHub LFS](https://docs.github.com/en/repositories/working-with-files/managing-large-files/collaboration-with-git-large-file-storage) also has storage and access behavior to operate. |
| Provider URL only | The pinned Geofabrik URL already fails. |
| Silently repin 2026-08-01 | Changes source identity while claiming historical equivalence. |

## Required invariants

1. Profile byte count and SHA-256 remain authoritative; URLs and mirrors are transport only.
2. Wrong, truncated, missing, or unreadable archive objects refuse replay without changing canonical or generated authority.
3. At least one independently retained copy is fetch-verified before accepting any new production source.
4. Source rights decide archive access; current Copernicus raw tiles do not enter public tracked assets.
5. World-only Matrix does not read raw-cache or archive availability.
6. Exact historical replay and new-source regeneration have distinct provenance claims.

## Implementation tasks

- [ ] Resolve Q1/Q2 and inventory authorized exact-byte recovery candidates.
- [ ] Add acquisition regressions for provider 404 + valid archive, wrong archive bytes, interrupted transfer, existing cache, and concurrent fetch under the current lock.
- [ ] Extend SourceIngestion's acquisition owner with optional archive retrieval and archive-before-new-promotion verification. Use profile identity and portable configuration; do not add a parallel source registry.
- [ ] Recover the exact Kazan PBF if possible and run full source/canonical replay, comparing outputs with promoted authority before any World mutation. If Q1 chooses a new snapshot, prepare a separately reviewed full regeneration and migration.
- [ ] Update SourceIngestion operations and World pipeline gate-selection SOTs, then run the focused tests, source/compiler checks, full Kazan Matrix, authority audits, and clean-checkout fetch proof.

## Test-first and verification plan

| Case | Red evidence / defect | Green evidence |
|---|---|---|
| Historical Kazan raw | URL 404 and empty local digest directory | Exact pinned bytes verified, full replay receipt identifies historical source |
| Provider unavailable | No archive fallback in acquisition | Valid archive retrieved and reverified with provider unavailable |
| Corrupt mirror | No mirror branch exists | Size/hash refusal; canonical active set and generated bytes unchanged |
| Disposable cache removed | Current cache lives under `tmp/` | Authorized clean checkout retrieves the pinned archive object |
| Source rights | Copernicus raw-publication policy says `not_distributed` | No raw Copernicus payload in public tracked content |
| New snapshot, if authorized | Current authority is tied to historical identity | New pinned identity, full receipts, semantic diff and explicit migration |

## Documentation plan

- **Stable owners:** `tools/World/SourceIngestion/README.md` for acquisition/recovery and `docs/testing/world_pipeline_layers.md` for Matrix routing.
- **Router:** `tools/World/README.md` already routes to SourceIngestion; change only if its entrypoint changes.
- **Duplication avoided:** keep exact vendor hashes, releases and rights in source profiles, not copied into current-state prose.

## Rollout and rollback

Mirror retrieval is additive and must reject mismatched bytes. External archive writes require the operator-authorized target. A new Kazan identity, if chosen, uses guarded enrollment, audit, snapshot and recovery; a failed replay or comparison leaves promoted authority intact.

## Completion criteria

External review accepts the design and Q1/Q2 are resolved; the chosen source is byte-verified, replayed through the correct Matrix, durably retrievable by an authorized fresh developer, and provenance and rights are accurately reported.

## Review record

### 2026-10-04 - investigation for external review

- Materially expanded the existing mirror backlog into the one reviewable raw-source task. Rechecked provider URL, profile, rights and acquisition owner. Historical bytes remain unavailable locally.
