// CDN Service Component Relationships
// Shows internal CDN flow and external interactions
// ASCII only (project rule)

// Internal CDN flow (storage access patterns)
cdnService.cdnInfrastructure.cdnControlApi -> cdnService.cdnInfrastructure.cdnObjectStorage "Write staged objects during upload, copy stage -> releases during promote/rollback, rotate manifests after verify" "S3 API"
cdnService.cdnInfrastructure.cdnEdge -> cdnService.cdnInfrastructure.cdnObjectStorage "Serve manifest.json plus dual artifacts (code.zip, content.utoc, content.ucas) with entitlement-aware routing" "HTTPS/S3"

// External: Build Service -> CDN (single HTTPS plane)
buildService.buildTool.cdnPublisher -> cdnService.cdnInfrastructure.cdnControlApi "Step 6: stream POST /builds/{buildId}/upload (artifacts+manifest) then POST /builds/{buildId}/verify-and-promote / POST /releases/rollback" "HTTPS"

// External: Launcher -> CDN (download manifest + bundles)
launcher.launcherApp.launcherManifestUpdater -> cdnService.cdnInfrastructure.cdnEdge "GET /manifest.json (current release)" "HTTPS"
launcher.launcherApp.launcherDownloadManager -> cdnService.cdnInfrastructure.cdnEdge "GET /units/{uuid}/{version}/{code.zip|content.utoc|content.ucas} (download IoStore artifacts)" "HTTPS"

// External: CDN -> Claims Service (optional: only when launcher provides auth token)
cdnService.cdnInfrastructure.cdnEdge -> claimsService "Optional: verify auth token from launcher; retrieve player's owned plugins/entitlements (when token provided)" "HTTPS"

// CDN implementation details (workflow, design decisions, manifest schema, API endpoints):
// See: <cdn-repo>\README.md
