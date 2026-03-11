// C3d - Component view: CDN Service
// Shows CDN flow: Upload staged -> Verify -> Promote (manifest rotation) -> Serve static

component cdnService.cdnInfrastructure {
    title "C3 - CDN Service"

    // External systems (for context)
    include buildService.buildTool
    include launcher.launcherApp

    // CDN components (3 total - minimal KISS design)
    include cdnService.cdnInfrastructure.cdnObjectStorage
    // Private control plane: internal ingress exposed via nginx but locked down
    include cdnService.cdnInfrastructure.cdnControlApi
    // Public delivery plane: launcher-facing nginx/CDN edge
    include cdnService.cdnInfrastructure.cdnEdge

    // Relationships auto-rendered from model_cdn_relationships.dsl
    // Shows flow: Build streams artifacts/manifest through ControlApi (single HTTPS plane; private ingress) -> ControlApi writes staged objects, validates manifest structure + hashes, promotes (atomic manifest.json overwrite, best-effort history rotation) -> Edge serves static (Launcher downloads /manifest.json + /units/{uuid}/{version}/bundle.zip) via the public nginx/CDN layer
    // Key: Immutable bundles with UUID-based versioned paths, manifest-based blue/green with 3-slot history (current + 2 backups), auto-fallback to prev1 if current is invalid
    // MVP: Edge is public (no per-user auth), CI serializes promotions, dependency validation in Build Service (not CDN)

    // autolayout lr
}
