// Build Service Component Relationships
// Shows internal flow within Build tool and external interactions
// ASCII only (project rule)

// CI/CD pipeline flow (linear orchestration by buildCli)
// Entry: CI trigger (branch push) -> Docker container starts buildCli with args (branch, build-id, channel)

// Step 0: Ensure BuildUnit descriptors exist
buildService.buildTool.buildCli -> buildService.buildTool.buildUnitBuilder "0) Baseline: invoke descriptor builder for project + plugins in scope" "Composition"
buildService.buildTool.buildUnitBuilder -> buildService.buildTool.buildState "Query: read build history to optimize descriptor creation" "File I/O"
buildService.buildTool.buildUnitBuilder -> buildService.buildTool.buildUnitDescriptors "Generate BuildUnit.yaml when missing (pre-build hashes only)" "File I/O"

// Step 1: Scan for changes
buildService.buildTool.buildCli -> buildService.buildTool.changeScanner "1) Scan: compute changed plugin list (descriptor hashes -> CLI memory)" "Composition"

// ChangeScanner dependencies (descriptor-only)
buildService.buildTool.changeScanner -> buildService.buildTool.buildUnitDescriptors "Read BuildUnit.yaml (auto baseline). Contains last code+config hash plus content hash (future utoc/ucas) and optional rollout metadata." "File I/O"
buildService.buildTool.changeScanner -> buildService.buildTool.buildUnitBuilder "Assumes descriptors exist (produced/refreshed in Step 0)" "File I/O"
developer -> buildService.buildTool.buildUnitDescriptors "Author optional BuildUnit.yaml metadata" "Git commits"
// (Step 0 arrow already defined above)

// Step 2: Plan build order (topological sort of changed set)
buildService.buildTool.buildCli -> buildService.buildTool.buildOrderPlanner "2) Order: consume stored changed list and return dependency-safe build order" "Composition"

// Step 3: Build via Unreal Automation Tool (DLC workflow - executor owns state lifecycle)
buildService.buildTool.buildCli -> buildService.buildTool.ueBuildExecutor "3) Build: run selected units via RunUAT/BuildGraph" "Composition"
buildService.buildTool.ueBuildExecutor -> buildService.buildTool.buildState "Load/query/save: manages build state lifecycle (base release decisions, build history)" "File I/O"

// Step 4: Package artifacts
buildService.buildTool.buildCli -> buildService.buildTool.artifactPackager "4) Package: assemble code.zip + IoStore/loose assets, compute SHA-256 per artifact" "Composition"

// Step 5: Update manifest
buildService.buildTool.buildCli -> buildService.buildTool.manifestStore "5) Manifest: update manifest.json in Git (versions, hashes, CDN paths)" "Composition"

// Step 6: Publish to CDN (API-only plane)
buildService.buildTool.buildCli -> buildService.buildTool.cdnPublisher "6) Publish: push artifacts/manifest via Control API upload + promote endpoints" "Composition"
// Note: CDN relationships now show cdnPublisher calling ControlApi only; ControlApi owns storage writes (staging + releases) internally in model_cdn_relationships.dsl

// Build workflow summary (DLC-based modular pipeline):
// 0. buildUnitBuilder ensures BuildUnit.yaml exists for project + all plugins
// 1. changeScanner detects changed units (BuildUnit.yaml descriptor hashes are authoritative; Git diff is never used)
// 2. buildOrderPlanner sorts changed list based on dependency graph (no implicit rebuilds unless BuildUnit requests it)
// 3. ueBuildExecutor manages DLC workflow (OWNS STATE LIFECYCLE):
//    a) Load buildState from YAML (base release version, build history)
//    b) Determine if base refresh needed (core plugins changed?)
//    c) If base refresh: sanitize .uproject, create base release, cook changed plugins as DLC
//       If reuse base: incremental build all modules, cook changed plugins as DLC using existing base
//    d) Save buildState with updated build history and base release version
// 4. artifactPackager bundles code.zip + IoStore/loose assets (utoc/ucas), computes SHA-256 for each artifact
// 5. manifestStore updates manifest.json in Git (source of truth: versions + hashes + paths + base release version)
// 6. cdnPublisher streams artifacts + manifest through CDN Control API, which stages and promotes internally (blue/green)
//
// Key design decisions (from best-practice analysis):
// - Manifest lives in Git (single source of truth), NOT learned from CDN
// - Docker image assumes engine + project sources mounted (no engine rebuild)
// - Uses UAT/BuildGraph (standard UE CI/CD backend) - no reinvented packaging
// - Per-unit artifacts (project-base, orchestrator, plugins as separate DLLs)
// - Blue/green CDN publishing (staging -> validate -> promote atomically)
// - Dependency chain tracking (tool parses .uplugin + manifest.json for graph)
// - Build outputs uploaded to CDN, launcher downloads from CDN (no direct client interaction)
//
// Future extensibility (not implemented now, but design supports):
// - Per-channel builds (dev/test/prod) - add channel arg to buildCli
// - Artifact signing - add signingService component between packager and publisher
// - Complex promotion rules - add promotionValidator component
// - Integration with claimsService - add claimValidator for access control rules
