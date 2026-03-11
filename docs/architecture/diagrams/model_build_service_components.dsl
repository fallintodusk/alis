// Build Service Component Definitions
// This file is included inside the buildTool container block in model.dsl
// Build tool runs in CI/CD (Docker) - builds changed plugins/project and publishes to CDN
// ASCII only (project rule)

// Entry point and orchestration
buildCli = component "Cli" "CLI entry point - parses args (branch, build-id, channel), orchestrates scan -> order -> build -> publish pipeline" "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Descriptor builder (Step 0 baseline)
buildUnitBuilder = component "BuildUnitBuilder" "Step 0 helper invoked by CLI to auto-create BuildUnit.yaml files for the root project (detected via .uproject) and any plugins missing descriptors. Computes code/config + content hashes without running UE." "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Change detection (descriptor-only)
changeScanner = component "ChangeScanner" "Reads BuildUnit.yaml descriptors (code/config + content hashes) to detect changes. If a descriptor is missing, the builder generates it before compiling the plugin (no Git diff fallback)." "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Descriptor files (auto-generated baseline + optional metadata)
buildUnitDescriptors = component "BuildUnitDescriptors" "BuildUnit.yaml per plugin plus one at the project root (auto-created before the build runs). Stores uuid/module metadata plus last code+config hash and last content hash (future utoc/ucas). Designers can extend with custom globs and rollout rules." "YAML files" {
    tags "Tier:CI,Domain:Platform,Data"
}

// Order planning (topological sort of changed units only)
buildOrderPlanner = component "OrderPlanner" "Consumes the CLI-held changed plugin list, reads dependency graph (.uplugin + manifest), and returns a dependency-safe order (no implicit rebuilds unless configured in BuildUnit)" "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Unreal build backend (delegates to UAT/BuildGraph)
ueBuildExecutor = component "UeBuildExecutor" "Invokes Unreal Automation Tool / BuildGraph (RunUAT BuildCookRun) to compile + cook + package selected units into per-unit artifacts (non-monolithic build)" "Shell/UAT" {
    tags "Tier:CI,Domain:Platform"
}

// Artifact normalization and hashing
artifactPackager = component "ArtifactPackager" "Bundles code.zip and collects standalone IoStore/loose assets (utoc/ucas/locres/etc.), computes SHA-256 for each, prepares manifest asset metadata" "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Manifest management (Git source of truth)
manifestStore = component "ManifestStore" "Reads and writes manifest.json from Git repo (single source of truth), updates versions + hashes + CDN paths for changed units only" "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}

// Build state tracking (DLC-based workflow)
buildState = component "BuildState" "Tracks base release versions, build history, and plugin changes. Persisted to YAML at tools/BuildService/state/build_state.yaml. Enables base release reuse and DLC workflow decisions." "Rust (YAML persistence)" {
    tags "Tier:CI,Domain:Platform,Data"
}

// CDN publishing (stream + verify/promote API)
cdnPublisher = component "CdnPublisher" "Streams artifacts/manifest to CDN Control API (single HTTPS plane: POST /builds/{buildId}/upload, POST /builds/{buildId}/verify-and-promote, POST /releases/rollback); no direct S3 access" "Go/Python/Rust" {
    tags "Tier:CI,Domain:Platform"
}
