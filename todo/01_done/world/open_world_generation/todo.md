# Open World Generation Execution Router

Status: complete. The bounded P0 path passes source ingestion, canonical
compilation, Unreal realization, clean bootstrap, and package validation.

## Demonstration Envelope Decision

- [x] Target the Republic of Tatarstan as the generated-coverage demonstration
  envelope, with Kazan city as an intermediate compiler milestone and a small
  Kazan slice as the separately measured game-ready Unreal proof.

The Tatarstan claim is permitted only after admitted terrain, road, and
building-massing inputs compile completely with hashes, rejection and
provenance results, and representative-cell evidence. It does not claim final
art, interiors, navigation, gameplay placement, or Unreal realization across
the republic. All-Russia generation remains a later batch and heterogeneity
stress test, not a prototype content-quality claim.

## Goal

Prove the contracts first with one synthetic two-cell fixture, then run the
same path on a minimal pinned Kazan snapshot. Canonical JSON remains
authoritative and Unreal content remains disposable.

## Execution Order

| Order | Responsibility | Owning todo | Status |
|---|---|---|---|
| 1 | Acquire, admit, verify, decode, and snapshot provider data | [01 Source Ingestion](01_source_ingestion.md) | Complete |
| 2 | Compile source snapshots into canonical ALIS JSON and cell manifests | [02 Canonical World Compilation](02_canonical_world_compilation.md) | Complete |
| 3 | Realize canonical cells through the narrow UE adapter | [03 Unreal World Realization](03_unreal_world_realization.md) | Complete |
| 4 | Prove the complete two-cell reconstruction and failure behavior | [04 End-to-End Validation](04_end_to_end_validation.md) | Complete |

Each file owns only its concern. Cross-layer acceptance belongs to step 4;
implementation details must not be copied back into this router.

## Current Slice

The P0 fixture is deliberately bounded to terrain, one boundary-crossing road,
small building massing, one authored override, one invalid feature,
provenance, and deterministic regeneration. A minimal Kazan proof follows
only after the synthetic contract passes.

After those contract gates, the first baked prototype targets the full intended
Kazan Landscape envelope and generated coverage, while the small Kazan slice
remains the separate game-ready quality proof. This does not add content
delivery or claim final city-wide art and gameplay quality.

Steps 1 and 2 are engine-independent. The separately owned
[engine upgrade](../../tools/engine_version_update_sot.md) has passed,
so step 3 now waits only for its canonical inputs. P0 requires no Experimental
feature.

The slice does not require Overture, paid software, Fab or Marketplace
content, hosted geospatial services, native content delivery, or Earth-scale
infrastructure. Water, vegetation, foliage, and rendering-profile work begin
only at the representative-region gate.

## Standing Routes

- [Working system map](system_map.md) - visual layer relationships.
- [Research audit](research_audit.md) - evidence, corrections, and deferred
  sequence; not an implementation queue.
- [World data and Unreal asset policy](../../../../docs/legal/world_data_and_asset_policy.md)
  - input admission, provenance, generated artifacts, and dependency boundary.
- [Component license policy](../../../../docs/legal/component_license_policy.md)
  - external compiler, UE adapter, protocol, and asset license classes.
- [Research materials](README.md#research-materials) - original reports and
  evidence routes.

## Completion

This task is closed. Later representative-region, scale, delivery, and
unattended-agent work remains deferred in the
[research audit](research_audit.md#deferred-sequence).
