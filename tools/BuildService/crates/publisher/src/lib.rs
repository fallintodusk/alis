//! CdnPublisher crate - Uploads artifacts to CDN via Control API
//!
//! Consumed by: BuildCli
//! Consumes: CDN Control API (nginx /api endpoints)
//!
//! Responsibilities:
//! - Upload artifacts via Control API PUT /builds/{buildId}/artifacts/{uuid}/{version}/{filename}
//! - Upload manifest via Control API PUT /builds/{buildId}/manifest
//! - Call CDN Control API verify-and-promote endpoint
//! - Handle rollback via Control API

mod control_api_client;

pub use control_api_client::{ControlApiClient, RollbackResponse, VerifyPromoteResponse};

use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::path::{Path, PathBuf};

/// Publisher configuration
pub struct PublisherConfig {
    pub control_api_url: String,
    pub control_api_token: Option<String>,
}

/// CdnPublisher - Publishes to CDN via Control API
pub struct CdnPublisher {
    control_api: ControlApiClient,
}

impl CdnPublisher {
    pub fn new(config: PublisherConfig) -> Self {
        let control_api = ControlApiClient::new(
            config.control_api_url,
            config.control_api_token,
        );

        Self { control_api }
    }

    /// Upload artifacts via Control API
    /// Expects artifact paths in format: {staging_dir}/units/{uuid}/{version}/{filename}
    pub async fn upload_artifacts(&self, build_id: &str, staging_dir: &Path, artifact_paths: &[PathBuf]) -> Result<()> {
        tracing::info!("Uploading {} artifacts for build {} via Control API", artifact_paths.len(), build_id);

        for path in artifact_paths {
            if !path.exists() {
                return Err(anyhow::anyhow!("Artifact not found: {}", path.display()));
            }

            // Read artifact bytes
            let bytes = std::fs::read(path)
                .with_context(|| format!("Failed to read artifact: {}", path.display()))?;

            // Compute SHA-256 hash
            let mut hasher = Sha256::new();
            hasher.update(&bytes);
            let hash = format!("{:x}", hasher.finalize());
            let size = bytes.len() as u64;

            // Extract uuid, version, filename from path
            // Expected structure: {staging_dir}/units/{uuid}/{version}/{filename}
            let relative_path = path
                .strip_prefix(staging_dir)
                .with_context(|| format!("Path {} is not within staging dir {}", path.display(), staging_dir.display()))?;

            let components: Vec<&str> = relative_path
                .iter()
                .filter_map(|c| c.to_str())
                .collect();

            // Expect at least: units/{uuid}/{version}/{filename}
            if components.len() < 4 || components[0] != "units" {
                return Err(anyhow::anyhow!(
                    "Invalid artifact path structure. Expected: units/{{uuid}}/{{version}}/{{filename}}, got: {}",
                    relative_path.display()
                ));
            }

            let uuid = components[1];
            let version = components[2];
            let filename = components[3];

            tracing::debug!("Uploading artifact: {}/{}/{} ({} bytes, hash: {})", uuid, version, filename, size, &hash[..8]);

            // Upload via Control API
            self.control_api
                .upload_artifact(build_id, uuid, version, filename, bytes, &hash, size)
                .await
                .with_context(|| format!("Failed to upload artifact: {}/{}/{}", uuid, version, filename))?;
        }

        tracing::info!("Successfully uploaded {} artifacts via Control API", artifact_paths.len());

        Ok(())
    }

    /// Upload manifest via Control API
    pub async fn upload_manifest(&self, build_id: &str, manifest_path: &PathBuf) -> Result<()> {
        if !manifest_path.exists() {
            return Err(anyhow::anyhow!("Manifest not found: {}", manifest_path.display()));
        }

        tracing::info!("Uploading manifest for build {} via Control API", build_id);

        // Read manifest JSON bytes
        let manifest_bytes = std::fs::read(manifest_path)
            .with_context(|| format!("Failed to read manifest: {}", manifest_path.display()))?;

        // Upload via Control API
        self.control_api
            .upload_manifest(build_id, manifest_bytes)
            .await
            .with_context(|| "Failed to upload manifest via Control API")?;

        tracing::info!("Successfully uploaded manifest via Control API");

        Ok(())
    }

    /// Call Control API to verify and promote build.
    ///
    /// NON-BYPASSABLE SOURCE GATE: promotion requires a
    /// [`engine_config::VerifiedReceipt`], whose ONLY constructor is
    /// `Receipt::verify` against the CURRENT repo/worktree/uproject/
    /// artifact state. There is no "publish existing artifact" path that
    /// skips the source gate (docs/ue_engine/version_update.md).
    pub async fn verify_and_promote(
        &self,
        receipt: &engine_config::VerifiedReceipt,
        build_id: &str,
        channel: &str,
    ) -> Result<VerifyPromoteResponse> {
        tracing::info!(
            "Verifying and promoting build {} to channel {} (source gate receipt: engine {} {}, commit {})",
            build_id,
            channel,
            receipt.receipt().source_build_version.line(),
            receipt.receipt().package_config,
            receipt.receipt().repo_commit
        );

        let response = self.control_api.verify_and_promote(build_id, channel).await?;

        if response.status != "promoted" {
            return Err(anyhow::anyhow!(
                "Verify and promote failed: status={}",
                response.status
            ));
        }

        tracing::info!("Build {} successfully promoted to {}: {} bundles verified, {} copied",
            build_id, channel, response.bundles_verified, response.bundles_copied);

        Ok(response)
    }

    /// Rollback to previous release
    pub async fn rollback(&self, channel: &str, target_version: Option<String>) -> Result<RollbackResponse> {
        tracing::info!("Rolling back channel {}", channel);

        if let Some(ref version) = target_version {
            tracing::info!("Target version: {}", version);
        }

        let response = self.control_api.rollback(channel, target_version).await?;

        if !response.success {
            return Err(anyhow::anyhow!("Rollback failed: {}", response.message));
        }

        tracing::info!("Successfully rolled back to version {}", response.rolled_back_to);

        Ok(response)
    }

    /// Check CDN health
    pub async fn health_check(&self) -> Result<bool> {
        self.control_api.health_check().await
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_publisher_creation() {
        let config = PublisherConfig {
            control_api_url: "http://localhost/api".to_string(),
            control_api_token: None,
        };
        let _publisher = CdnPublisher::new(config);
        // Publisher created successfully
    }

    #[tokio::test]
    async fn test_health_check_unavailable() {
        let config = PublisherConfig {
            control_api_url: "http://localhost:9999/api".to_string(), // Non-existent endpoint
            control_api_token: None,
        };
        let publisher = CdnPublisher::new(config);

        // Should return error or false when service unavailable
        let result = publisher.health_check().await;
        assert!(result.is_err() || result.unwrap() == false);
    }

    #[test]
    fn test_artifact_path_parsing() {
        // Test that artifact path parsing correctly extracts uuid/version/filename
        let staging_dir = std::path::PathBuf::from("/tmp/staging");
        let artifact_path = staging_dir.join("units/c30b4272-14de-42d8-ad38-eaf0cb7ed600/1.4.7/code.zip");

        let relative_path = artifact_path.strip_prefix(&staging_dir).unwrap();
        let components: Vec<&str> = relative_path
            .iter()
            .filter_map(|c| c.to_str())
            .collect();

        assert_eq!(components.len(), 4);
        assert_eq!(components[0], "units");
        assert_eq!(components[1], "c30b4272-14de-42d8-ad38-eaf0cb7ed600");
        assert_eq!(components[2], "1.4.7");
        assert_eq!(components[3], "code.zip");
    }
}
