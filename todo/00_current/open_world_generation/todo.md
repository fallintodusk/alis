# Open World Generation Research Audit

Status: audited and reviewer-corrected on 2026-07-21; first-slice decisions
and implementation not started.

This document audits `research_main.md`, `research_agentic.md`,
`research_ue.md`, and `research_impl.md` against the current repository and
primary sources. It is a comprehensive audit record and decision register,
not the active implementation queue or a standing architecture source of
truth (SOT). The bounded execution queue is in `first_slice.md`. Any accepted
decision must move to the owning stable documentation before implementation is
considered complete.

## Audit Verdict

The research has a strong common direction:

- Keep canonical geospatial and semantic data outside Unreal.
- Treat Unreal assets as disposable, reproducible projections.
- Separate static geography, generated render data, and dynamic simulation.
- Compile deterministically by spatial cell and preserve authored overrides.
- Put agents in the authoring and validation plane, never in the shipped
  runtime authority path.

The reports are not ready to approve as an implementation plan. They blur
planned ALIS architecture with implemented capability, assume UE 5.8 while
the project is pinned to UE 5.7, conflict on HLOD and content delivery, and do
not yet define the identity, licensing, determinism, or acceptance contracts
needed for a safe first slice.

Do not begin an Earth-scale platform, introduce production infrastructure, or
install additional editor mutation tools until the critical gates below are
closed.

## Research Evidence Coverage

| Report | Source coverage in the file | Audit result |
|---|---:|---|
| `research_main.md` | 42 linked claim citations recovered from PDF | Useful strategy, but several citations point to mutable repository pages or broad product docs and still need claim-level validation. |
| `research_agentic.md` | 48 linked claim citations recovered from PDF | Useful automation model, but tool maturity and security claims still need current-source validation and repo gap analysis. |
| `research_ue.md` | 41 linked claim citations recovered from PDF | Citation trail is restored, but several claims are current only for UE 5.8 and cannot be treated as present UE 5.7 capability. |
| `research_impl.md` | 25 URL occurrences | Best-supported report, but source quality, license consequences, version pinning, and ALIS implementation assumptions still need correction. |

The first three reports now retain the numbered links embedded in their source
PDFs. Restoration makes the evidence traceable; it does not make every claim
correct, current, or sufficiently supported. The audit findings below still
govern their use.

## Repository Evidence for Critical Assertions

These references record the audited repository state. Line numbers may drift;
the named declarations, comments, and keys are the durable lookup points.

- UE is pinned to 5.7 in `Alis.uproject:3` and the launcher-engine selection is
  recorded in `scripts/config/ue_path.conf`.
- City17 documents an 8192 m region and 256 m gameplay cell in
  `Plugins/World/City17/README.md:169` and
  `Plugins/World/City17/README.md:174`, with 32 cells per region axis at
  `Plugins/World/City17/README.md:177`.
- Existing editor-control surfaces are declared as `blueprint-mcp` and
  `ue-mcp` in `.mcp.json:3` and `.mcp.json:13`. Local executable paths are
  intentionally not reproduced here.
- `Plugins/World/PCG/ProjectPCG/Source/ProjectPCG/Private/ProjectPCGModule.cpp:7`
  explicitly identifies the module as a stub awaiting PCG engine integration.
- `Plugins/Boot/Orchestrator/Source/OrchestratorCore/Private/OrchestratorIoStore.cpp:43`
  marks actual IoStore mounting as TODO, and line 58 reports mounting as not
  implemented.
- City17 has no tracked schema, manifest, JSON, or Structurizr DSL data. The
  repeatable check is
  `rg --files Plugins/World/City17 | rg -i '(\.json$|\.dsl$|schema|manifest)'`;
  it returned no matches at audit time.

## Critical Findings

### C-01 - Validate recovered claim-level evidence before accepting decisions

Problem:

- The PDF citation maps are restored, but some links target mutable `main`
  branch pages, older documentation routes, or sources broader than the claim
  they accompany.
- Product maturity, engine-version support, standard versions, and data
  licenses are time-sensitive.
- A recovered link is evidence provenance, not automatic verification.

Required actions:

- [x] Restore the PDF's numbered claim links in all three reports.
- [x] Remove PDF-export tracking parameters from the restored URLs.
- [ ] Confirm each citation directly supports the complete adjacent claim.
- [ ] Prefer official engine docs, standards bodies, project repositories,
  and data-owner terms over secondary summaries.
- [ ] Record an access date and applicable version for volatile sources.
- [ ] Separate verified fact, repo observation, inference, and recommendation.
- [ ] Add a link check for this research folder.

Exit condition: a reviewer can trace each engine, tool, standard, and license
claim to a primary source without access to the original research session.

### C-02 - Correct the implemented baseline

Problem:

- The reports repeatedly describe a world loader, tile JSON pipeline,
  deterministic builder, query service, diffing, and PCG services as existing
  ALIS foundations.
- The current `ProjectWorld` source contains manifest and definition-host
  primitives, but its loader, world tile types, query subsystem, validator,
  diff, file watcher, and tests remain unchecked work in
  [ProjectWorld TODO](../../../Plugins/World/ProjectWorld/TODO.md).
- `ProjectWorld/README.md` contains examples using types that do not exist in
  source, including `UProjectWorldLoader`, `UWorldBuilder`, and
  `UProjectWorldDiff`.
- [ProjectPCG](../../../Plugins/World/PCG/ProjectPCG/README.md) is a module
  stub. Its runtime subsystem and integration remain TODOs.
- No tracked City17 world manifests, tile schemas, or generated world-data
  pipeline were found.

Required actions:

- [ ] Add an "implemented", "documented design", or "research proposal"
  label to every ALIS capability discussed by the reports.
- [ ] Update the research wording from "extend the existing world pipeline"
  to "build the first world compiler/runtime-adapter slice" where applicable.
- [ ] Decide whether stale examples in `ProjectWorld/README.md` should become
  an explicit proposed API section or be removed from the stable README.
- [ ] Do not design later federation or global streaming around APIs that have
  not passed a local vertical slice.

Exit condition: the baseline inventory is accurate enough that the first
implementation milestone contains no hidden prerequisite systems.

### C-03 - Resolve the UE 5.7 versus UE 5.8 capability boundary

Problem:

- `Alis.uproject` and `scripts/config/ue_path.conf` pin the project to UE 5.7.
- The local UE 5.7 install contains PCG, GeoReferencing,
  GeometryScripting, ChunkDownloader, and ScriptableTools.
- It does not contain the UE 5.8 ModelContextProtocol, ToolsetRegistry, Mesh
  Terrain, or Mesh Partition plugins described as current foundations in
  `research_ue.md`.
- Epic labels Unreal MCP, PCG Editor Mode, Mesh Terrain, and related UE 5.8
  surfaces Experimental. Unreal MCP also documents incomplete APIs, serial
  execution on the game thread, and a loopback HTTP endpoint.

Required actions:

- [ ] Choose and record one branch of the decision: stay on UE 5.7 for the
  first slice, or approve an independently validated UE 5.8 upgrade.
- [ ] If staying on UE 5.7, remove official Unreal MCP, Toolset Registry, Mesh
  Terrain, and Mesh Partition from P0 requirements.
- [ ] If upgrading, run project build, packaging, plugin compatibility, and
  exact smoke/integration gates before adopting any new experimental plugin.
- [ ] Keep every Experimental feature behind an adapter and feature flag.
- [ ] Maintain a non-experimental fallback for generation and validation.

Exit condition: no milestone depends on an engine feature absent from the
selected engine, and experimental dependencies have rollback paths.

Primary checks: [Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor),
[Toolset Registry](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ToolsetRegistry),
[PCG Editor Mode](https://dev.epicgames.com/documentation/unreal-engine/pcg-editor-mode-in-unreal-engine),
and [PCG Mesh Terrain](https://dev.epicgames.com/documentation/unreal-engine/pcg-and-mesh-terrain-in-unreal-engine).

### C-04 - Define stable identity, coordinates, and separate spatial grids

Problem:

- The reports correctly reject one universal grid, but they do not define the
  contract connecting source features, compile cells, render tiles, World
  Partition cells, simulation partitions, and delivery bundles.
- `ProjectWorld` currently proposes one file per 8192 m tile and 256 m World
  Partition cells. Those may be valid City17 choices, but are not a global
  Earth addressing standard.
- Overture GERS can seed identity for Overture features, but cannot be the
  only identity scheme for every source, authored object, or derived artifact.

Required actions:

- [ ] Record canonical feature ID, source ID, source release, revision ID,
  artifact ID, and tombstone semantics in the owning stable architecture docs.
- [ ] Record source CRS, canonical CRS, vertical datum, axis order, units,
  precision, origin rebasing, and UE transform conversion in the owning stable
  architecture docs.
- [ ] Define explicit mappings among source partitions, compiler cells,
  render tiles, World Partition cells, simulation cells, and delivery packs.
- [ ] Treat "one file per tile" as one logical tile manifest that may reference
  multiple typed artifacts, not one giant physical JSON payload.
- [ ] Keep City17 tile sizes in its world contract until measurements justify
  a reusable default.
- [ ] Specify split, merge, collision, and cross-border rules for stable IDs.

Exit condition: the same feature can be traced across releases and products
without deriving identity from mutable coordinates, array order, or UE actor
names.

Primary check: [Overture GERS](https://docs.overturemaps.org/gers/).

### C-05 - Replace the HLOD versus Nanite contradiction with a measured policy

Problem:

- `ProjectWorld/README.md` first calls HLOD a key concept and later declares a
  no-HLOD policy.
- `research_main.md` prefers Nanite and streaming over HLOD.
- `research_ue.md` makes HLOD P0 and mandatory.
- Nanite and HLOD solve different problems. Nanite manages geometry detail for
  loaded meshes; World Partition HLOD can represent unloaded actors and reduce
  draw calls. One is not a blanket replacement for the other.
- PCG GPU static mesh spawning has documented gaps including persistence,
  collision, navigation, ray tracing, and HLOD support.

Required actions:

- [ ] Define at least three content profiles: far-field visualization,
  playable district, and immediate gameplay area.
- [ ] Benchmark no HLOD, Instancing plus Nanite, and HLOD variants on one
  representative cell.
- [ ] Measure disk amplification, build time, memory, draw calls, streaming
  latency, collision, navigation, and regeneration cost.
- [ ] Move the accepted profile-specific policy to the owning world/rendering
  SOT and remove contradictory blanket statements.
- [ ] Do not use GPU PCG output for gameplay-authoritative geometry until its
  persistence and gameplay subsystem gaps are closed.

Exit condition: HLOD is enabled or disabled by measured content profile, not
by a global slogan.

Primary checks: [World Partition HLOD](https://dev.epicgames.com/documentation/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine)
and [PCG GPU processing](https://dev.epicgames.com/documentation/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine).

### C-06 - Preserve one native-content manifest and trust authority

Problem:

- `research_ue.md` proposes Asset Manager, chunks, and ChunkDownloader as a P0
  distribution stack.
- Current ALIS documentation assigns authenticated artifact download and
  signed IoStore installation to Launcher, content mounting to ProjectLoading,
  and code/module lifecycle to Orchestrator.
- Related loading documents were not fully consistent about whether Launcher
  merely installs IoStore containers or also owns mounting them. The stable
  documents now assign installation to Launcher and runtime mounting to
  ProjectLoading.
- Runtime IoStore mounting still contains incomplete or placeholder behavior,
  so the current path is a design plus partial implementation, not a proven
  production capability.
- The intended ownership split is not fully implemented. At audit time,
  `FOrchestratorCoreModule::ApplyHotUpdates` still downloads and extracts
  content, and the unused `FOrchestratorIoStore` placeholder reports success
  without mounting.
- The pre-planning baseline already makes ProjectLoading skip an empty mount
  request and fail an explicit content-pack request closed with error 200.
  Real mounting and isolated regression coverage remain Delivery-track work;
  this planning round does not change that implementation.
- Adding ChunkDownloader now could create two manifests, two trust paths, two
  retry models, and two ownership boundaries for the same content.

Required actions:

- [ ] Record the delivery decision in the owning loading architecture docs,
  comparing the existing component flow with ChunkDownloader and any 3D Tiles
  streaming path.
- [ ] Preserve one signed native-content manifest graph and trust chain while
  keeping focused component ownership: CDN entitlement filtering; Launcher
  download, verification, and installation; Orchestrator code/module
  lifecycle; and ProjectLoading runtime content mounting.
- [ ] Preserve one authoritative selected-release and last-known-good chain.
  Components may keep derived transaction, checkpoint, cache, and retry state,
  but must not independently select competing releases or trust roots.
- [ ] Keep coarse geospatial streaming distinct from native gameplay pack
  delivery; document if both are required.
- [ ] Complete and test real IoStore mounting before calling hot-mounted
  native region delivery production-ready.
- [ ] In the first Delivery implementation patch, add isolated regression
  coverage for the existing fail-closed baseline before adding real mounting:
  one helper/scenario per test, empty requests skip successfully, explicit
  requests fail with error 200 and stop the pipeline, retries stay disabled,
  and the expected error log is declared only by the failure test.
- [ ] Define offline, update, partial-download, corruption, rollback, and
  version-skew behavior.

Exit condition: one signed manifest graph and trust policy covers every native
content artifact from publish through mount, with clear component boundaries
and no parallel downloader ambiguity. This is an independent Delivery-track
concern and does not block World-track compiler or regional work.

Primary check: [Epic ChunkDownloader guide](https://dev.epicgames.com/documentation/unreal-engine/implementing-chunkdownloader-in-your-gameplay-in-unreal-engine).

### C-07 - Make licensing and provenance a compiler gate

Problem:

- Source, tool, database, generated-content, Unreal, and hosted-service terms
  are separate legal layers. The stable policy and current source audit live
  in [Component Licensing](../../../docs/legal/component_license_policy.md)
  and [World Data and Unreal Asset Licensing](../../../docs/legal/world_data_and_asset_policy.md).
- The compiler has no implemented way to enforce that policy yet.

Required actions:

- [x] Apply the canonical UE-facing, standalone, protocol, and authored-content
  assignments through the root `LICENSE`.
- [ ] Define separate ODbL base-world and ALIS-authored overlay boundaries.
- [ ] Define a machine-readable provenance graph for datasets, authored
  content, procedural rules, tools, databases, artifacts, and releases.
- [ ] Make every canonical feature and generated artifact traceable through
  `derived_from`, `embeds`, `references`, `generated_by`, and `packages` edges.
- [ ] Begin with synthetic committed fixtures; admit real payloads only after
  exact redistribution and commercial-use review.
- [ ] Generate attribution, source/alteration offers, and third-party notices
  from the same graph.
- [ ] Add dependency inventory, SBOM, and boundary compatibility validation
  for the compiler and Unreal adapter.
- [ ] Fail compilation and publication when any license, ownership,
  attribution, source offer, or distribution result is unresolved.
- [ ] Store credentials outside source data, manifests, assets, logs, and
  client builds; reject Google-derived reconstruction and unknown sources.

Exit condition: every shipped cell has reproducible provenance and an approved
distribution policy, and no generated artifact can silently lose attribution.

### C-08 - Specify determinism before building visuals

Problem:

- "Deterministic" is a goal in all four reports but not yet a level-specific
  contract.
- A seed based on an engine hash, mutable name representation, input order, or
  dependency defaults may change across machines and versions.
- Geometry libraries, coordinate transforms, floating-point behavior, and
  topology repair can produce different output even with the same logical
  inputs.

Required actions:

- [ ] Declare a required determinism level for every output and test it at that
  level:
  - D0 semantic: equivalent normalized features, identity, topology, and
    provenance regardless of serialization.
  - D1 canonical-manifest byte: byte-identical canonical manifests and
    structured reports.
  - D2 selected engine-independent artifact byte: byte-identical declared
    compiler artifacts under a pinned toolchain and platform contract.
  - D3 Unreal semantic reproducibility: equivalent imported assets, actor
    ownership, spatial placement, references, collision/navigation intent, and
    validation results. Unreal package byte identity is optional and may be
    promised only after a dedicated proof.
- [ ] Define canonical input serialization, field ordering, numeric
  quantization, null handling, and Unicode normalization at the data boundary.
- [ ] Select and version a stable hash and seed-derivation algorithm.
- [ ] Pin compiler, GDAL, PROJ, GEOS, PDAL, and schema versions in a lock or
  reproducible container/toolchain manifest.
- [ ] Sort all unordered source collections before compilation.
- [ ] Record compiler version, ruleset version, dependency versions, input
  hashes, and output hashes in every cell manifest.
- [ ] Add golden-cell tests that delete outputs, rebuild twice, and compare
  semantic and byte-level results where byte identity is promised.
- [ ] Define acceptable platform-specific variance where byte identity is not
  feasible.

Exit condition: deleting generated output and rebuilding from an immutable
source snapshot produces the declared deterministic result on CI and a second
machine.

### C-09 - Threat-model every agent and editor mutation surface

Problem:

- The repository already configures `blueprint-mcp` and `ue-mcp`; adding the
  same or overlapping servers can duplicate mutation paths.
- The configured `ue-mcp` appears to cover the ChiR24 tool family already
  recommended by the research.
- Official Unreal MCP is Experimental, loopback HTTP, and serial on the game
  thread. Third-party servers can also execute broad editor or Python actions.
- More tools do not automatically produce a safer or more deterministic
  pipeline.

Required actions:

- [ ] Inventory existing MCP commands, write scope, authentication, network
  binding, audit logging, timeouts, and rollback behavior.
- [ ] Build a capability-gap matrix before installing another server or CLI.
- [ ] Bind editor-control services to loopback only and keep them out of
  Shipping targets.
- [ ] Require explicit, narrow tool schemas for destructive or bulk mutation.
- [ ] Run generation agents in isolated workspaces with immutable inputs and
  disposable outputs.
- [ ] Require deterministic validators and tests to decide success; an agent's
  narrative verdict is not an acceptance gate.
- [ ] Preserve manual CLI/commandlet entry points for every automated skill.
- [ ] Define concurrency locks because Unreal editor mutation is not safely
  parallel by default.

Exit condition: each automated mutation is attributable, bounded, reversible,
validated, and cannot expose a remote editor control surface.

Primary checks: [Codex customization](https://developers.openai.com/codex/concepts/customization),
[Claude Code skills](https://code.claude.com/docs/en/slash-commands),
[Claude Code subagents](https://code.claude.com/docs/en/sub-agents), and
[Claude Code hooks](https://code.claude.com/docs/en/hooks).

### C-10 - Constrain the first slice and define measurable acceptance

Problem:

- Whole-Earth scope hides basic unresolved contracts and invites premature
  databases, services, dashboards, repositories, and runtime streaming.
- The proposed 10 km by 10 km region plus 1 km by 1 km playable district is
  still too large for the first contract and determinism proof.
- The reports list phases but do not set measurable budgets.

Required actions:

- [ ] Start with one immutable source snapshot and two adjacent compiler cells
  containing terrain, a road crossing the boundary, a small building set, a
  feature crossing the boundary, one authored override, and one deliberately
  invalid feature.
- [ ] Prove boundary ownership, clipping or reference policy, neighboring-cell
  consistency, and partial regeneration without seams, duplicates, or orphans.
- [ ] Define first-slice budgets only for deterministic compile time, partial
  rebuild time, canonical output size, UE import time, generated actor/asset
  counts, boundary mismatches, invalid-feature rejection, authored-override
  survival, and D0-D3 validation.
- [ ] Defer cook amplification, runtime memory, frame time, draw calls,
  streaming latency, navigation performance, replication counts, HLOD, and
  representative data-quality budgets to World Gate W3.
- [ ] Require zero-touch deletion and regeneration before expanding area.
- [ ] Expand to one representative City17 region only after the two-cell
  gate passes.
- [ ] Defer federation and Earth-scale service design until two materially
  different regions prove the contracts.

Exit condition: expansion is controlled by objective budgets and repeatable
quality gates, not by visual plausibility alone.

## Important Findings

### I-01 - Keep the geospatial compiler boundary outside Unreal

- [ ] Reuse `ProjectDefinitionGenerator` schema discipline and generated-asset
  conventions, but do not turn it into a heavy GIS compiler.
- [ ] Keep raw ingest, CRS normalization, topology repair, conflation, and
  cell compilation in a headless external tool.
- [ ] Give Unreal a narrow adapter that validates manifests and realizes
  engine-native assets.
- [ ] Keep generated `.uasset` files disposable unless an explicit exception
  documents why they are authoritative.

Reason: `ProjectDefinitionGenerator` is a real editor-only JSON-to-DataAsset
system with hashes and orphan cleanup, but its current responsibility is
resource definitions, not geospatial ETL.

### I-02 - Separate static, generated, authored, and dynamic state

- [ ] Define immutable source snapshots and compiled static cells.
- [ ] Store manual corrections as versioned patches or authored overlays, not
  edits inside generated output.
- [ ] Keep live simulation, player state, damage, traffic, and events in a
  separate persistence model keyed by stable feature IDs.
- [ ] Specify rebase/conflict behavior when a new source release changes a
  feature with authored or dynamic overlays.

### I-03 - Treat terrain classes and vertical datums explicitly

- [ ] Distinguish DSM, digital terrain model (DTM), bare-earth LiDAR, bathymetry,
  and gameplay-modified terrain in schemas.
- [ ] Do not feed a DSM directly into playable terrain without evaluating
  building and vegetation artifacts.
- [ ] Record horizontal CRS, vertical datum, geoid model, resolution, void
  fill, resampling method, and uncertainty per terrain artifact.
- [ ] Define how roads, foundations, tunnels, water, and authored terrain cuts
  modify the base surface.

### I-04 - Use open standards at boundaries, not as one universal model

- [ ] Use STAC 1.1.0 for source-asset catalog metadata where it fits.
- [ ] Evaluate CityJSON 2.0 for semantic city exchange, not as the runtime or
  canonical model by default.
- [ ] Use 3D Tiles 1.1 as the stable far-field streaming baseline; do not make
  proposed 3D Tiles 2.0 a hard dependency before adoption.
- [ ] Study OGC CDB 2.0 concepts for versionable simulation repositories, but
  do not implement full CDB compliance without an interoperability customer.
- [ ] Use OpenDRIVE 1.9 only when road-network interchange justifies its
  complexity; keep ALIS gameplay road semantics independent.
- [ ] Keep ALIS representation tiers distinct from legacy CityGML/CityJSON LoD
  labels unless exact conformance is required.

Primary checks: [STAC](https://www.ogc.org/standards/stac/),
[CityJSON](https://www.cityjson.org/specs/),
[OGC CDB](https://www.ogc.org/standards/cdb/),
[ASAM OpenDRIVE](https://www.asam.net/standards/detail/opendrive/), and
[3D Tiles 2.0 proposal status](https://www.ogc.org/requests/ogc-seeks-public-comment-on-proposed-3d-tiles-2-0-community-standard-work-item/).

### I-05 - Do not declare production infrastructure permanent before demand

- [ ] Start the proof with versioned local files, manifests, and a deterministic
  CLI where possible.
- [ ] Add GeoParquet for read-heavy interchange only after access patterns are
  known; its own project warns it is not a write-heavy database format.
- [ ] Introduce PostGIS when concurrent writes, spatial query, or conflation
  workloads prove the need.
- [ ] Introduce object storage when artifact volume, remote workers, retention,
  or delivery requirements prove the need.
- [ ] Avoid splitting the compiler, schemas, registry, browser, and adapters
  into multiple repositories during the first slice.
- [ ] Record each infrastructure adoption in the owning stable architecture
  docs with operating cost, backup, migration, and local-development
  consequences.

Primary check: [GeoParquet](https://github.com/opengeospatial/geoparquet).

### I-06 - Evaluate third-party Unreal tools through narrow proofs

- [ ] Evaluate Cesium for Unreal as an optional far-field/globe adapter, not
  as canonical storage. Its current code is Apache-2.0 and supports UE 5.7;
  hosted services and datasets require separate review.
- [ ] Evaluate PCG Extended Toolkit only for a representative graph operation,
  build compatibility, determinism, maintenance, and packaging impact.
- [ ] Evaluate `soft-ue-cli` only for a capability missing from current MCP
  tools or for headless CI; its own guidance prefers official MCP where
  supported.
- [ ] Treat Claireon as experimental: it reports beta status and has no stable
  release history sufficient for a P0 dependency.
- [ ] Do not install ChiR24 Unreal MCP again until the existing `ue-mcp`
  configuration is identified and gap-tested.
- [ ] Record version, commit, license, owner, update policy, and removal path
  for every accepted third-party dependency.

Primary checks: [Cesium for Unreal](https://github.com/CesiumGS/cesium-unreal),
[PCG Extended Toolkit](https://github.com/PCGEx/PCGExtendedToolkit),
[soft-ue-cli](https://github.com/softdaddy-o/soft-ue-cli),
[Claireon](https://github.com/believer-oss/claireon), and
[ChiR24 Unreal MCP](https://github.com/ChiR24/Unreal_mcp).

### I-07 - Make data quality and confidence first-class

- [ ] Attach provenance and confidence to every derived height, footprint,
  classification, facade, address, and road attribute.
- [ ] Preserve conflicting observations instead of silently selecting one.
- [ ] Define deterministic source precedence by feature class and region.
- [ ] Produce reject and conflict reports with geometry previews.
- [ ] Keep AI-generated geometry labeled as synthetic evidence, never as
  authoritative reconstruction.

### I-08 - Define authored override preservation before regeneration

- [ ] Put generated actors/assets in clearly owned paths and Data Layers.
- [ ] Keep authored hero content in separate layers or explicit override files.
- [ ] Define ownership at field, feature, and spatial-boundary levels.
- [ ] Make the compiler refuse to overwrite unknown or editor-owned content.
- [ ] Add a regeneration test that proves one authored override survives a
  changed source snapshot.

### I-09 - Specify generated-asset lifecycle and schema migration

- [ ] Version source schemas, compiler output schemas, and UE adapter support
  independently.
- [ ] Define forward/backward compatibility and full-rebuild triggers.
- [ ] Generate into a staging area, validate, then atomically promote.
- [ ] Detect and delete orphaned assets only inside compiler-owned roots.
- [ ] Emit an artifact manifest suitable for diff, rollback, and cleanup.

### I-10 - Separate runtime visualization from gameplay authority

- [ ] Treat coarse streamed 3D Tiles as visual context unless a validated
  collision/navigation/interaction derivative exists.
- [ ] Generate native Unreal representations for gameplay-critical terrain,
  buildings, roads, cover, navigation, and replication.
- [ ] Define handoff and visual-transition rules between far-field and native
  regions.
- [ ] Never infer gameplay authority from a visible streamed mesh alone.

### I-11 - Define command-line contracts before agent skills

- [ ] Provide deterministic commands for fetch, normalize, validate, compile,
  diff, publish, import, and verify.
- [ ] Give every command structured output, stable exit codes, dry-run support,
  explicit input/output roots, and an operation ID.
- [ ] Build skills and MCP tools as thin orchestration over those commands.
- [ ] Keep validation independent from the agent or tool that generated data.
- [ ] Add resumability and idempotence before parallel or unattended jobs.

### I-12 - Keep browser inspection useful but deferred

- [ ] Begin with structured validation reports and exported debug layers.
- [ ] Add a lightweight map inspector only when it closes a demonstrated review
  gap that Unreal and static reports cannot cover.
- [ ] Do not deploy TerriaJS or another full digital-twin portal in Phase 1.
- [ ] If introduced, make the inspector read the same immutable manifests and
  provenance rather than creating a second data authority.

### I-13 - Add operational and supply-chain controls

- [ ] Generate SBOM and license notices for compiler and Unreal dependencies.
- [ ] Pin downloads by version and checksum; do not fetch mutable latest URLs
  during deterministic builds.
- [ ] Define cache eviction, backup, disaster recovery, and source re-fetch
  expectations.
- [ ] Redact credentials and private endpoints from logs and generated reports.
- [ ] Define quotas and backoff for external APIs; mirror allowed source data
  rather than relying on availability during builds.

### I-14 - Correct report structure and terminology

- [ ] Give `research_impl.md` one H1 and demote its other top-level headings.
- [ ] Repair editorial artifacts such as `runtime-not` where punctuation was
  lost during ASCII conversion.
- [ ] Define source, canonical feature, compiled cell, render tile, World
  Partition cell, region pack, overlay, and simulation state once.
- [ ] Replace vague terms such as "open", "production-ready", "infinite", and
  "deterministic" with testable definitions.
- [ ] Remove volatile version claims from narrative text unless they are pinned
  and cited.

## Additional Findings

### A-01 - Strong claims that survived the audit

The following ideas are suitable as provisional design principles, subject to
the critical decisions above:

- [ ] Canonical data remains engine-independent and versioned.
- [ ] Unreal is a purpose-specific realization and runtime consumer.
- [ ] Static geography and dynamic world state are separate.
- [ ] Multiple spatial hierarchies are mapped explicitly.
- [ ] Generated output is disposable and reproducible.
- [ ] Authored work is preserved through explicit ownership boundaries.
- [ ] Agents automate bounded authoring tasks and do not become runtime
  dependencies or acceptance authorities.

### A-02 - Claims to downgrade from fact to proposal

- [ ] "ALIS already has a world tile loader and deterministic builder."
- [ ] "ProjectPCG already provides runtime PCG services."
- [ ] "UE 5.8 official MCP and Toolset Registry are available to the current
  project."
- [ ] "HLOD is always required" or "HLOD is always a trap."
- [ ] "ChunkDownloader is the correct ALIS native delivery authority."
- [ ] "PostGIS, MinIO, six repositories, and a browser twin are permanent Tier
  S requirements."
- [ ] "Any dataset described as open can be mixed and redistributed under one
  policy."
- [ ] "A 3D Tiles or Nanite visual automatically supports collision,
  navigation, gameplay queries, or replication."

### A-03 - Primary-source facts that need to remain versioned

- [ ] STAC Core is 1.1.0 and STAC API is 1.0.0 at audit time.
- [ ] CityJSON's published current specification is 2.0.2 at audit time.
- [ ] GeoParquet 1.1.0 plus patch 1 is the stable line at audit time; do not
  implement against an unapproved development 2.0 draft by accident.
- [ ] OGC CDB 2.0 is published; references that call 1.3 current need context.
- [ ] ASAM OpenDRIVE 1.9.0 is current at audit time.
- [ ] OGC 3D Tiles 2.0 is a proposed work item under public comment at audit
  time, while 1.1 remains the safe compatibility target.
- [ ] Tool and engine versions must be rechecked at the actual adoption gate.

### A-04 - Repository documentation follow-up

- [x] Replace ProjectWorld's global HLOD rule with a measured, profile-specific
  policy.
- [x] Mark ProjectWorld's implemented types separately from proposed API
  examples and its TODO state.
- [ ] Move accepted compiler/runtime contracts to the owning stable plugin or
  architecture docs.
- [ ] Do not link this temporary todo from living code or stable SOT docs.
- [ ] Keep all research and resulting documentation ASCII-only.

## Recommended Tracks and Decision Order

World generation and native delivery are independent tracks. Local compiler
outputs and representative-region work do not wait for remote delivery,
entitlements, or IoStore hot mounting.

### World Gate W0 - Architecture decisions

- [x] Use the repository's current UE 5.7 baseline for the first slice; treat
  UE 5.8 capabilities as upgrade-gated.
- [ ] Approve identity, CRS, units, vertical datum, and grid-mapping contracts.
- [ ] Approve compiler versus Unreal adapter ownership.
- [x] Approve component and authored-content licensing.
- [ ] Approve ODbL base/overlay separation, provenance graph, and
  forbidden-source policy.
- [ ] Approve the determinism contract and toolchain pinning strategy.

These are the five minimum first-slice decisions. Record accepted decisions in
the owning stable architecture or plugin docs; do not create a separate ADR
hierarchy unless the repository adopts that convention. Normal agent-assisted
coding and research can continue. Do not begin broad visual generation or
unattended editor mutation before the relevant gates close.

### World Gate W1 - Two-cell vertical slice

- [ ] Create the source ledger schema.
- [ ] Create canonical feature and cell-manifest schemas.
- [ ] Create an authored-overlay schema.
- [ ] Implement structural, provenance/license, and geometry/boundary
  validators as separate responsibilities.
- [ ] Compile a boundary-crossing fixture into two deterministic,
  engine-independent adjacent cells.
- [ ] Produce structured diff, reject, attribution, and provenance reports.
- [ ] Ingest one immutable terrain, road, building, cross-boundary feature,
  authored-override, and invalid-feature fixture.
- [ ] Compile both cells twice from a clean output directory.
- [ ] Import it through a narrow UE adapter.
- [ ] Produce only terrain, road, and building massing with stable source
  identity; defer runtime query, collision, and navigation subsystems.
- [ ] Preserve one authored override across a changed input snapshot.
- [ ] Capture the bounded first-slice metrics from C-10.

### World Gate W2 - Reproducibility and failure recovery

- [ ] Rebuild on CI and a second machine.
- [ ] Test interrupted compile, corrupt cache, missing source, invalid license,
  schema migration, and rollback.
- [ ] Confirm generated assets can be deleted and reconstructed.
- [ ] Confirm outputs never overwrite non-owned content.

### World Gate W3 - Representative region and rendering profiles

- [ ] Expand to one representative City17 region.
- [ ] Benchmark far-field, playable-district, and immediate-area profiles.
- [ ] Decide HLOD/Nanite/instancing/PCG policy from measurements.
- [ ] Measure cook amplification, runtime memory, frame time, draw calls,
  streaming latency, navigation performance, replication counts, and
  representative-region data quality.

### World Gate W4 - Scale and federation

- [ ] Test a second region with different data quality and CRS constraints.
- [ ] Add PostGIS, object storage, remote workers, 3D Tiles, or a browser
  inspector only where measured needs justify them.
- [ ] Design federation and Earth-scale addressing only after the two-region
  evidence validates the contracts.

### Delivery Gate D1 - Manifest and trust ownership

- [ ] Preserve one signed manifest graph and trust policy across CDN,
  Launcher, Orchestrator, and ProjectLoading responsibilities.
- [ ] Preserve one authoritative selected-release and last-known-good chain;
  allow derived local transaction/checkpoint state that cannot choose a
  competing release.
- [ ] Decide whether ChunkDownloader adds a required capability without
  creating a second manifest or trust path.
- [ ] Audit the release and downstream-use history of the public
  `OrchestratorAPI.h` surface. Preserve it until evidence supports either a
  deprecation/compatibility path or removal at a documented breaking-version
  boundary; do not infer external safety from repository references alone.

### Delivery Gate D2 - Real runtime mounting

- [ ] Add isolated regression coverage for the existing ProjectLoading
  fail-closed baseline before implementing real mounting. Each test must run
  only its named scenario, and only the failure test may expect the error log.
- [ ] Complete and test ProjectLoading IoStore mounting before claiming
  hot-mounted region delivery.

### Delivery Gate D3 - Failure recovery

- [ ] Test entitlement, offline, update, partial-download, corruption,
  rollback, and version-skew behavior.

### Automation Gate A1 - Unattended editor mutation

- [ ] Wrap proven deterministic commands as skills or MCP tools.
- [ ] Add audit logs, timeouts, locks, resumability, dry-run, and rollback.
- [ ] Prove a human can run and diagnose the same workflow without an agent.
- [ ] Run exact validation and integration gates after every agent mutation.

Normal agent-assisted implementation may be used in earlier gates. This gate
is specifically for unattended editor mutation and generation workflows.

## Definition of Done

### Research DoD

- [ ] Every consequential claim has a primary citation and version context.
- [ ] Current implementation and proposed architecture are visibly separated.
- [ ] The five World Gate W0 decisions are accepted or explicitly rejected and moved
  to stable owning docs.
- [ ] The two-cell fixture, budgets, determinism levels, and acceptance tests
  are specified.
- [ ] Accepted standing decisions are moved to stable owning docs.
- [ ] This todo remains an execution record and is not referenced as SOT.

### First-Slice DoD

- [ ] All tasks and acceptance checks in `first_slice.md` pass.
- [ ] Licensing and provenance checks are machine-enforced for both cells.
- [ ] D0 through the declared D1/D2 targets pass after clean regeneration.
- [ ] D3 Unreal semantic comparison passes; no `.uasset` byte guarantee is
  claimed without proof.
- [ ] Boundary ownership, authored overrides, rejection behavior, and partial
  regeneration pass without seams, duplicates, or orphans.

### Native-Delivery DoD

- [ ] One signed manifest graph and trust policy covers publish, entitlement,
  download, verification, installation, activation, mount, and rollback.
- [ ] CDN, Launcher, Orchestrator, and ProjectLoading retain their focused
  responsibilities without a second delivery authority.
- [ ] Real IoStore mounting and all Delivery D2/D3 failure modes pass automated
  tests.

Until these conditions are met, the research should guide experiments but
must not be treated as an approved production architecture.
