// CDN Service Component Definitions
// This file is included inside the cdnInfrastructure container block in model.dsl
// CDN handles staged builds, verification, manifest promotion/rollback, and static serving
// ASCII only (project rule)

// Object storage (S3/MinIO compatible)
cdnObjectStorage = component "Cdn.ObjectStorage" "Versioned object storage (MinIO/S3). Stores: /stage/{buildId}/* (staged uploads), /releases/units/{uuid}/{version}/{code.zip|content.utoc|content.ucas} (immutable IoStore artifacts), /releases/manifests/manifest*.json (current + 2 backups). Access limited to CDN components (ControlApi + Edge)" "MinIO/S3" {
    tags "Tier:Infra,Domain:Platform"
}

// Control API (ingest + verify + promote + rollback + auto-fallback)
cdnControlApi = component "Cdn.ControlApi" "Single ingress surface for Build Service. Streams uploads (multipart/chunked), writes staged objects internally, validates manifest structure/hashes, promotes to prod (atomic manifest.json overwrite + best-effort history rotation), handles rollback, performs auto-fallback if current manifest is missing/invalid" "Rust + Axum" {
    tags "Tier:Infra,Domain:Platform,Access:Private"
}

// Edge serving (Axum router with optional CDN/Nginx front)
cdnEdge = component "Cdn.Edge" "Axum edge service (optionally fronted by Nginx/CDN). Serves GET /manifest.json (current release) with optional Claims-based filtering and GET /units/{uuid}/{version}/{code.zip|content.utoc|content.ucas} with entitlement checks + cache rules" "Rust + Axum (+ optional Nginx/CDN)" {
    tags "Tier:Infra,Domain:Platform,Access:Public"
}
