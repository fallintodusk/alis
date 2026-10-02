// Common test utilities for BuildService integration and smoke tests
//
// Provides shared configuration, health checks, and test helpers
// for tests that require external services (containers).

use anyhow::{Context, Result};
use build_service_publisher::PublisherConfig;
use sha2::{Digest, Sha256};
use std::env;

/// Get publisher configuration for container-based tests
///
/// Reads configuration from environment variables with sensible defaults
/// for local Docker containers (CDN Control API via nginx).
///
/// Environment variables:
/// - TEST_API_URL: Control API URL (default: http://localhost/api)
/// - TEST_API_TOKEN: Optional Control API token for auth
///
/// Note: Build Service communicates ONLY with CDN Control API, never directly with S3/MinIO.
pub fn get_test_publisher_config() -> PublisherConfig {
    PublisherConfig {
        control_api_url: env::var("TEST_API_URL")
            .unwrap_or_else(|_| "http://localhost/api".to_string()),
        control_api_token: env::var("TEST_API_TOKEN").ok(),
    }
}

/// Check if required containers are running and accessible
///
/// Verifies health endpoint for CDN Control API via nginx.
/// Build Service only needs Control API access (S3/MinIO is managed by CDN internally).
///
/// Returns error if Control API is unavailable.
pub async fn check_containers_ready() -> Result<()> {
    let base_url = env::var("PUBLIC_CDN_URL")
        .unwrap_or_else(|_| "http://localhost".to_string());

    // Check CDN health (uses /healthz endpoint, not /api/health)
    let health_url = format!("{}/healthz", base_url);
    let health_response = reqwest::get(&health_url)
        .await
        .with_context(|| format!("Failed to connect to CDN at {}", base_url))?;

    if !health_response.status().is_success() {
        anyhow::bail!("CDN health check failed: {}", health_response.status());
    }

    Ok(())
}

/// Print container configuration for debugging
pub fn print_test_config() {
    println!("=== Container Test Configuration ===");
    println!("Control API URL: {}",
        env::var("TEST_API_URL").unwrap_or_else(|_| "http://localhost/api".to_string()));
    println!("Has API Token: {}",
        env::var("TEST_API_TOKEN").is_ok());
    println!("====================================");
}

/// Get public CDN base URL for testing
///
/// Returns the public CDN URL where artifacts are served after promotion.
/// For local testing, this is typically nginx on http://localhost.
///
/// Environment variable:
/// - PUBLIC_CDN_URL: Public CDN base URL (default: http://localhost)
pub fn get_public_cdn_url() -> String {
    env::var("PUBLIC_CDN_URL")
        .unwrap_or_else(|_| "http://localhost".to_string())
}

/// Compute SHA-256 hash of bytes
///
/// Returns lowercase hex string (64 characters)
pub fn compute_sha256(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("{:x}", hasher.finalize())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_test_publisher_config() {
        let config = get_test_publisher_config();
        assert!(!config.control_api_url.is_empty());
    }

    #[test]
    fn test_config_uses_defaults() {
        env::remove_var("TEST_API_URL");
        env::remove_var("TEST_API_TOKEN");

        let config = get_test_publisher_config();
        assert_eq!(config.control_api_url, "http://localhost/api");
        assert!(config.control_api_token.is_none());
    }
}
