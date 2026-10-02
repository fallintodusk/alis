//! Integration tests for CdnPublisher upload functionality
//! Tests artifact upload, manifest upload, and verify-promote flow

use anyhow::Result;
use build_service_publisher::{CdnPublisher, PublisherConfig};
use std::fs;
use std::path::PathBuf;
use tempfile::TempDir;

/// Helper to create mock staging directory with artifacts
fn create_mock_staging() -> Result<(TempDir, String, Vec<PathBuf>)> {
    let temp_dir = TempDir::new()?;
    let build_id = "test-build-123";

    let plugin_uuid = "abc-123-def";
    let plugin_version = "1.0.0";

    // Create units directory structure
    let units_dir = temp_dir.path()
        .join("units")
        .join(plugin_uuid)
        .join(plugin_version);
    fs::create_dir_all(&units_dir)?;

    // Create mock artifacts
    let code_zip = units_dir.join("code.zip");
    let utoc = units_dir.join("content.utoc");
    let ucas = units_dir.join("content.ucas");

    fs::write(&code_zip, b"mock DLL binary content")?;
    fs::write(&utoc, b"mock utoc metadata")?;
    fs::write(&ucas, b"mock ucas asset data")?;

    let artifact_paths = vec![code_zip, utoc, ucas];

    Ok((temp_dir, build_id.to_string(), artifact_paths))
}

/// Helper to create mock manifest file
fn create_mock_manifest(temp_dir: &TempDir) -> Result<PathBuf> {
    let manifest_path = temp_dir.path().join("manifest.json");
    let manifest_json = r#"{
        "manifest_version": 1,
        "engine_build_id": "UE5.5-test",
        "plugins": [{
            "uuid": "abc-123-def",
            "name": "TestPlugin",
            "version": "1.0.0",
            "platform": "Windows",
            "code": {
                "url": "https://cdn.test/units/abc-123-def/1.0.0/code.zip",
                "hash": "a".repeat(64),
                "size": 1024
            },
            "assets": [],
            "depends_on": [],
            "channel": "experimental"
        }]
    }"#;

    fs::write(&manifest_path, manifest_json)?;
    Ok(manifest_path)
}

#[tokio::test]
async fn test_upload_artifacts_path_parsing() -> Result<()> {
    let (staging_dir, build_id, artifact_paths) = create_mock_staging()?;

    // Test should fail with invalid Control API URL (no server running)
    let config = PublisherConfig {
        control_api_url: "http://localhost:19999/api".to_string(),
        control_api_token: None,
    };

    let publisher = CdnPublisher::new(config);

    // Should extract paths correctly, then fail on HTTP call (no server)
    let result = publisher
        .upload_artifacts(&build_id, staging_dir.path(), &artifact_paths)
        .await;

    // Expect connection/upload error (path parsing succeeded)
    assert!(result.is_err());
    let err_msg = result.unwrap_err().to_string();
    assert!(
        err_msg.contains("Failed to upload artifact") || err_msg.contains("Connection") || err_msg.contains("Failed to call"),
        "Expected upload/connection error, got: {}",
        err_msg
    );

    Ok(())
}

#[tokio::test]
async fn test_upload_manifest_file_not_found() -> Result<()> {
    let config = PublisherConfig {
        control_api_url: "http://localhost:19999/api".to_string(),
        control_api_token: None,
    };

    let publisher = CdnPublisher::new(config);
    let nonexistent_manifest = PathBuf::from("/nonexistent/manifest.json");

    let result = publisher
        .upload_manifest("test-build", &nonexistent_manifest)
        .await;

    assert!(result.is_err());
    let err_msg = result.unwrap_err().to_string();
    assert!(err_msg.contains("Manifest not found"));

    Ok(())
}

#[tokio::test]
async fn test_invalid_artifact_path_structure() -> Result<()> {
    let temp_dir = TempDir::new()?;

    // Create artifact in WRONG location (not under units/)
    let invalid_path = temp_dir.path().join("wrong").join("code.zip");
    fs::create_dir_all(invalid_path.parent().unwrap())?;
    fs::write(&invalid_path, b"test")?;

    let config = PublisherConfig {
        control_api_url: "http://localhost:19999/api".to_string(),
        control_api_token: None,
    };

    let publisher = CdnPublisher::new(config);

    let result = publisher
        .upload_artifacts("test-build", temp_dir.path(), &[invalid_path])
        .await;

    assert!(result.is_err());
    let err_msg = result.unwrap_err().to_string();
    assert!(
        err_msg.contains("Invalid artifact path structure"),
        "Expected path structure error, got: {}",
        err_msg
    );

    Ok(())
}

#[tokio::test]
#[ignore] // Requires CDN containers running
async fn test_upload_with_real_cdn() -> Result<()> {
    use std::env;

    // Check if CDN is accessible
    let api_url = env::var("TEST_API_URL")
        .unwrap_or_else(|_| "http://localhost/api".to_string());

    let config = PublisherConfig {
        control_api_url: api_url.clone(),
        control_api_token: env::var("TEST_API_TOKEN").ok(),
    };

    let publisher = CdnPublisher::new(config);

    // Health check first
    let is_healthy = publisher.health_check().await?;
    assert!(is_healthy, "CDN should be healthy");

    // Create staging with artifacts
    let (staging_dir, build_id, artifact_paths) = create_mock_staging()?;

    // Upload artifacts
    publisher
        .upload_artifacts(&build_id, staging_dir.path(), &artifact_paths)
        .await?;

    // Upload manifest
    let manifest_path = create_mock_manifest(&staging_dir)?;
    publisher.upload_manifest(&build_id, &manifest_path).await?;

    // Promotion requires a VerifiedReceipt (non-bypassable source gate):
    // issue + verify against the exact artifacts being promoted.
    let engine_root = staging_dir.path().join("fake_engine");
    fs::create_dir_all(engine_root.join("Engine/Build"))?;
    fs::write(
        engine_root.join("Engine/Build/Build.version"),
        b"{\"MajorVersion\":5,\"MinorVersion\":8,\"PatchVersion\":1}",
    )?;
    let uproject = staging_dir.path().join("Alis.uproject");
    fs::write(&uproject, b"{ \"EngineAssociation\": \"5.8\" }")?;
    let receipt = engine_config::Receipt::issue(
        "test-commit".into(),
        "test-worktree-fp".into(),
        &uproject,
        &engine_root,
        "Shipping/Win64".into(),
        &artifact_paths,
    )?;
    let verified = receipt.verify(
        "test-commit",
        "test-worktree-fp",
        &uproject,
        &artifact_paths,
    )?;

    // Verify and promote
    let response = publisher
        .verify_and_promote(&verified, &build_id, "experimental")
        .await?;

    assert_eq!(response.status, "promoted");
    assert_eq!(response.build_id, build_id);

    Ok(())
}
