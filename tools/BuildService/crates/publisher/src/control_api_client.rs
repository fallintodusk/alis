//! CDN Control API client

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use reqwest::header::AUTHORIZATION;

/// Control API client
pub struct ControlApiClient {
    base_url: String,
    client: reqwest::Client,
    auth_header: Option<String>,
}

/// Verify and promote request
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VerifyPromoteRequest {
    pub build_id: String,
    pub channel: String,
}

/// Verify and promote response (matches CDN Control API schema)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VerifyPromoteResponse {
    pub status: String,
    pub build_id: String,
    pub manifest_version: u32,
    pub plugins_count: usize,
    pub bundles_verified: usize,
    pub bundles_copied: usize,
    pub promoted_at: String,
}

/// Rollback request
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RollbackRequest {
    pub channel: String,
    pub target_version: Option<String>,
}

/// Rollback response
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RollbackResponse {
    pub success: bool,
    pub rolled_back_to: String,
    pub message: String,
}

impl ControlApiClient {
    /// Create new Control API client with timeout support
    pub fn new(base_url: String, auth_token: Option<String>) -> Self {
        let client = reqwest::Client::builder()
            .timeout(std::time::Duration::from_secs(120))
            .build()
            .expect("Failed to create reqwest client");
        let auth_header = auth_token.map(|token| format!("Bearer {}", token));
        Self { base_url, client, auth_header }
    }

    fn apply_auth(&self, builder: reqwest::RequestBuilder) -> reqwest::RequestBuilder {
        if let Some(ref header_value) = self.auth_header {
            builder.header(AUTHORIZATION, header_value)
        } else {
            builder
        }
    }

    /// Verify and promote build to channel
    pub async fn verify_and_promote(
        &self,
        build_id: &str,
        channel: &str,
    ) -> Result<VerifyPromoteResponse> {
        let url = format!("{}/builds/{}/verify-and-promote", self.base_url, build_id);

        tracing::info!("Calling Control API: POST {}", url);

        let request = VerifyPromoteRequest {
            build_id: build_id.to_string(),
            channel: channel.to_string(),
        };

        let response = self
            .apply_auth(self.client.post(&url))
            .json(&request)
            .send()
            .await
            .with_context(|| format!("Failed to call Control API: {}", url))?;

        if !response.status().is_success() {
            return Err(anyhow::anyhow!(
                "Control API returned error: {}",
                response.status()
            ));
        }

        let result: VerifyPromoteResponse = response
            .json()
            .await
            .with_context(|| "Failed to parse Control API response")?;

        if result.status == "promoted" {
            tracing::info!("Build {} verified and promoted successfully: {} bundles copied",
                build_id, result.bundles_copied);
        } else {
            tracing::warn!("Build {} verify status: {}", build_id, result.status);
        }

        Ok(result)
    }

    /// Rollback to previous version
    pub async fn rollback(&self, channel: &str, target_version: Option<String>) -> Result<RollbackResponse> {
        let url = format!("{}/releases/rollback", self.base_url);

        tracing::info!("Calling Control API: POST {}", url);

        let request = RollbackRequest {
            channel: channel.to_string(),
            target_version,
        };

        let response = self
            .apply_auth(self.client.post(&url))
            .json(&request)
            .send()
            .await
            .with_context(|| format!("Failed to call Control API: {}", url))?;

        if !response.status().is_success() {
            return Err(anyhow::anyhow!(
                "Control API returned error: {}",
                response.status()
            ));
        }

        let result: RollbackResponse = response
            .json()
            .await
            .with_context(|| "Failed to parse Control API response")?;

        if result.success {
            tracing::info!("Rolled back to version: {}", result.rolled_back_to);
        } else {
            tracing::error!("Rollback failed: {}", result.message);
        }

        Ok(result)
    }

    /// Health check
    pub async fn health_check(&self) -> Result<bool> {
        let url = format!("{}/healthz", self.base_url);

        tracing::debug!("Health check: GET {}", url);

        let response = self
            .apply_auth(self.client.get(&url))
            .send()
            .await
            .with_context(|| "Failed to call health check")?;

        Ok(response.status().is_success())
    }

    /// Upload manifest for a build with retry support
    /// PUT /builds/{buildId}/manifest
    pub async fn upload_manifest(
        &self,
        build_id: &str,
        manifest_bytes: Vec<u8>,
    ) -> Result<()> {
        let url = format!("{}/builds/{}/manifest", self.base_url, build_id);

        tracing::info!("Uploading manifest: PUT {}", url);

        // Retry logic: 3 attempts with exponential backoff (1s, 2s, 4s)
        let mut last_err = None;
        for attempt in 1..=3 {
            let response = self
                .apply_auth(self.client.put(&url))
                .header("Content-Type", "application/json")
                .body(manifest_bytes.clone())
                .send()
                .await;

            match response {
                Ok(response) if response.status().is_success() => {
                    tracing::info!("Manifest uploaded successfully for build: {} (attempt {})", build_id, attempt);
                    return Ok(());
                }
                Ok(response) => {
                    let status = response.status();
                    let error_body = response.text().await.unwrap_or_default();
                    last_err = Some(anyhow::anyhow!(
                        "Control API upload manifest failed: {} - {}",
                        status,
                        error_body
                    ));
                }
                Err(e) => {
                    last_err = Some(e.into());
                }
            }

            if attempt < 3 {
                let sleep_secs = 2_u64.pow(attempt - 1);
                tracing::warn!(
                    "Manifest upload attempt {} failed, retrying in {}s: {}",
                    attempt,
                    sleep_secs,
                    last_err.as_ref().unwrap()
                );
                tokio::time::sleep(std::time::Duration::from_secs(sleep_secs)).await;
            }
        }

        Err(last_err.unwrap_or_else(|| anyhow::anyhow!("Manifest upload failed after 3 attempts")))
    }

    /// Upload artifact for a build with retry support
    /// PUT /builds/{buildId}/artifacts/{uuid}/{version}/{filename}
    pub async fn upload_artifact(
        &self,
        build_id: &str,
        uuid: &str,
        version: &str,
        filename: &str,
        bytes: Vec<u8>,
        hash: &str,
        size: u64,
    ) -> Result<()> {
        let url = format!(
            "{}/builds/{}/artifacts/{}/{}/{}",
            self.base_url, build_id, uuid, version, filename
        );

        tracing::info!("Uploading artifact: PUT {} ({} bytes)", url, size);

        // Retry logic: 3 attempts with exponential backoff (1s, 2s, 4s)
        let mut last_err = None;
        for attempt in 1..=3 {
            let response = self
                .apply_auth(self.client.put(&url))
                .header("Content-Type", "application/octet-stream")
                .header("X-Artifact-Hash", hash)
                .header("X-Artifact-Size", size.to_string())
                .body(bytes.clone())
                .send()
                .await;

            match response {
                Ok(response) if response.status().is_success() => {
                    tracing::info!("Artifact uploaded: {}/{}/{} (attempt {})", uuid, version, filename, attempt);
                    return Ok(());
                }
                Ok(response) => {
                    let status = response.status();
                    let error_body = response.text().await.unwrap_or_default();
                    last_err = Some(anyhow::anyhow!(
                        "Control API upload artifact failed: {} - {}",
                        status,
                        error_body
                    ));
                }
                Err(e) => {
                    last_err = Some(e.into());
                }
            }

            if attempt < 3 {
                let sleep_secs = 2_u64.pow(attempt - 1);
                tracing::warn!(
                    "Upload attempt {} failed, retrying in {}s: {}",
                    attempt,
                    sleep_secs,
                    last_err.as_ref().unwrap()
                );
                tokio::time::sleep(std::time::Duration::from_secs(sleep_secs)).await;
            }
        }

        Err(last_err.unwrap_or_else(|| anyhow::anyhow!("Upload failed after 3 attempts")))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_client_creation() {
        let client = ControlApiClient::new("http://localhost/api".to_string(), None);
        assert_eq!(client.base_url, "http://localhost/api");
    }

    #[test]
    fn test_verify_promote_request() {
        let request = VerifyPromoteRequest {
            build_id: "test-build-123".to_string(),
            channel: "stable".to_string(),
        };

        assert_eq!(request.build_id, "test-build-123");
        assert_eq!(request.channel, "stable");
    }

    #[test]
    fn test_rollback_request() {
        let request = RollbackRequest {
            channel: "beta".to_string(),
            target_version: Some("1.0.0".to_string()),
        };

        assert_eq!(request.channel, "beta");
        assert!(request.target_version.is_some());
    }
}
