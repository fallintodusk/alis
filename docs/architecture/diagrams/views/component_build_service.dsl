// C3c - Component view: Build Service Pipeline
// Shows CI/CD build/publish flow: Scan -> Order -> Build -> Package -> Manifest -> Publish

component buildService.buildTool {
    title "C3 - Build Service Pipeline"

    include developer

    // External systems (CDN data plane + control plane)
    include cdnService.cdnInfrastructure.cdnObjectStorage
    include cdnService.cdnInfrastructure.cdnControlApi

    // Build components (8 total - linear pipeline + state tracking)
    include buildService.buildTool.buildCli
    include buildService.buildTool.buildUnitBuilder
    include buildService.buildTool.changeScanner
    include buildService.buildTool.buildUnitDescriptors
    include buildService.buildTool.buildOrderPlanner
    include buildService.buildTool.ueBuildExecutor
    include buildService.buildTool.artifactPackager
    include buildService.buildTool.manifestStore
    include buildService.buildTool.buildState
    include buildService.buildTool.cdnPublisher

    // Relationships auto-rendered from model_build_service_relationships.dsl
    // Shows DLC-based pipeline:
    // Step 0: BuildUnitBuilder reads build history, ensures BuildUnit.yaml exist for all plugins
    // Step 1: ChangeScanner reads descriptor hashes -> detect changes
    // Step 2: OrderPlanner orders changed plugins by dependencies
    // Step 3: UeBuildExecutor manages DLC workflow (OWNS STATE LIFECYCLE):
    //         - Loads state (base release version, build history)
    //         - Decides base refresh strategy (core plugins changed?)
    //         - Runs DLC build (base refresh or reuse existing base)
    //         - Saves state (updated build history, base release version)
    // Step 4: ArtifactPackager bundles code.zip + IoStore containers
    // Step 5: ManifestStore updates manifest (versions, hashes, CDN paths, base release version)
    // Step 6: CdnPublisher streams to CDN
    // Key: Manifest + BuildState live in Git (single source of truth), NOT learned from CDN

    // autolayout lr
}
