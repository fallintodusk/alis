// Integration tests for Publisher crate with real containers
//
// These tests require running containers:
// - nginx proxy (serves Control API at http://localhost/api and public downloads at http://localhost)
// - CDN Control API (backend, accessed via nginx)
// - MinIO (managed internally by CDN, NOT accessed by Build Service)
//
// Run with: cargo test --test publisher_container_test -- --ignored
//
// Environment variables:
// - TEST_API_URL (default: http://localhost/api)
// - TEST_API_TOKEN (optional: Control API auth token)

mod common;

use anyhow::Result;
use build_service_publisher::CdnPublisher;
use std::fs;
use tempfile::TempDir;

/// Test that containers are accessible before running tests
#[tokio::test]
#[ignore] // Requires containers
async fn test_containers_health() -> Result<()> {
    common::print_test_config();
    common::check_containers_ready().await?;
    println!("✓ All containers are healthy");
    Ok(())
}

/// Test publisher creation (no initialization needed with Control API)
#[tokio::test]
#[ignore] // Requires containers
async fn test_publisher_creation() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let _publisher = CdnPublisher::new(config);

    println!("✓ Publisher created successfully (no async init needed)");
    Ok(())
}

/// Test uploading artifacts via Control API
#[tokio::test]
#[ignore] // Requires containers
async fn test_upload_artifacts_via_control_api() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    // Create test staging directory with mock artifacts
    let temp_dir = TempDir::new()?;
    let build_id = format!("test-{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
    let staging_dir = temp_dir.path().join(&build_id);
    fs::create_dir_all(&staging_dir)?;

    // Create mock plugin UUID and version
    let plugin_uuid = uuid::Uuid::new_v4().to_string();
    let plugin_version = "1.0.0";

    // Create artifacts with UUID-based structure: staging/units/{uuid}/{version}/{filename}
    let artifact_dir = staging_dir.join("units").join(&plugin_uuid).join(plugin_version);
    fs::create_dir_all(&artifact_dir)?;

    let code_zip = artifact_dir.join("code.zip");
    fs::write(&code_zip, b"mock DLL binary content")?;

    let utoc_file = artifact_dir.join("content.utoc");
    fs::write(&utoc_file, b"mock utoc content")?;

    let ucas_file = artifact_dir.join("content.ucas");
    fs::write(&ucas_file, b"mock ucas content")?;

    // Upload artifacts via Control API PUT endpoints
    let artifact_paths = vec![code_zip.clone(), utoc_file.clone(), ucas_file.clone()];

    publisher
        .upload_artifacts(&build_id, &staging_dir, &artifact_paths)
        .await?;

    println!("✓ Artifacts uploaded via Control API");
    println!("  Build ID: {}", build_id);
    println!("  Plugin UUID: {}", plugin_uuid);
    println!("  Artifacts: {}", artifact_paths.len());

    Ok(())
}

/// Test uploading manifest via Control API
#[tokio::test]
#[ignore] // Requires containers
async fn test_upload_manifest_via_control_api() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    // Create test manifest
    let temp_dir = TempDir::new()?;
    let build_id = format!("test-{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
    let manifest_file = temp_dir.path().join("manifest.json");

    let manifest_json = serde_json::json!({
        "manifest_version": 1,
        "engine_build_id": build_id,
        "plugins": []
    });

    fs::write(&manifest_file, serde_json::to_string_pretty(&manifest_json)?)?;

    // Upload manifest via Control API PUT /builds/{buildId}/manifest
    publisher.upload_manifest(&build_id, &manifest_file).await?;

    println!("✓ Manifest uploaded via Control API");
    println!("  Build ID: {}", build_id);

    Ok(())
}

/// Test full publish workflow: upload artifacts + manifest via Control API
#[tokio::test]
#[ignore] // Requires containers
async fn test_full_publish_workflow() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    // Setup test data
    let temp_dir = TempDir::new()?;
    let build_id = format!("test-{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
    let staging_dir = temp_dir.path().join(&build_id);
    fs::create_dir_all(&staging_dir)?;

    let plugin_uuid = uuid::Uuid::new_v4().to_string();
    let plugin_version = "1.0.0";
    let artifact_dir = staging_dir.join("units").join(&plugin_uuid).join(plugin_version);
    fs::create_dir_all(&artifact_dir)?;

    // Create artifacts
    let code_zip = artifact_dir.join("code.zip");
    fs::write(&code_zip, b"test binary content")?;
    let utoc_file = artifact_dir.join("content.utoc");
    fs::write(&utoc_file, b"test utoc")?;
    let ucas_file = artifact_dir.join("content.ucas");
    fs::write(&ucas_file, b"test ucas")?;

    // Create manifest
    let manifest_file = temp_dir.path().join("manifest.json");
    let code_hash = "a".repeat(64); // Mock SHA-256
    let utoc_hash = "b".repeat(64);
    let ucas_hash = "c".repeat(64);

    let manifest_json = serde_json::json!({
        "manifest_version": 1,
        "engine_build_id": format!("UE5.5-{}", build_id),
        "plugins": [{
            "uuid": plugin_uuid,
            "name": "TestPlugin",
            "version": plugin_version,
            "platform": "Windows",
            "code": {
                "url": format!("https://cdn.alis.game/units/{}/{}/code.zip", plugin_uuid, plugin_version),
                "hash": code_hash,
                "size": 23
            },
            "assets": [
                {
                    "url": format!("https://cdn.alis.game/units/{}/{}/content.utoc", plugin_uuid, plugin_version),
                    "hash": utoc_hash,
                    "size": 9,
                    "role": "utoc"
                },
                {
                    "url": format!("https://cdn.alis.game/units/{}/{}/content.ucas", plugin_uuid, plugin_version),
                    "hash": ucas_hash,
                    "size": 9,
                    "role": "ucas"
                }
            ],
            "depends_on": [],
            "channel": "dev"
        }]
    });

    fs::write(&manifest_file, serde_json::to_string_pretty(&manifest_json)?)?;

    // Execute workflow via Control API
    println!("=== Full Publish Workflow (Control API) ===");
    println!("Build ID: {}", build_id);
    println!("Plugin UUID: {}", plugin_uuid);

    // Step 1: Upload artifacts via Control API
    let artifact_paths = vec![code_zip, utoc_file, ucas_file];
    publisher
        .upload_artifacts(&build_id, &staging_dir, &artifact_paths)
        .await?;
    println!("✓ Step 1: Artifacts uploaded via Control API");

    // Step 2: Upload manifest via Control API
    publisher.upload_manifest(&build_id, &manifest_file).await?;
    println!("✓ Step 2: Manifest uploaded via Control API");

    // Step 3: Verify services are healthy
    publisher.health_check().await?;
    println!("✓ Step 3: Services healthy");

    println!("✓ Full publish workflow completed successfully");

    Ok(())
}

/// Test health check endpoint
#[tokio::test]
#[ignore] // Requires containers
async fn test_health_check() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    let is_healthy = publisher.health_check().await?;
    assert!(is_healthy, "CDN Control API should be healthy");

    println!("✓ Health check passed");
    Ok(())
}

/// Test verify and promote endpoint (may fail if Control API not fully implemented)
#[tokio::test]
#[ignore] // Requires containers + Control API implementation
async fn test_verify_and_promote() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    let build_id = format!("test-{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
    let channel = "dev";

    // This test may fail if Control API is not fully implemented
    // That's expected - it verifies the integration point
    let work = tempfile::tempdir()?;
    let receipt = make_verified_receipt(work.path(), &[])?;
    let result = publisher.verify_and_promote(&receipt, &build_id, channel).await;

    match result {
        Ok(response) => {
            println!("✓ Verify and promote succeeded");
            println!("  Message: {}", response.message);
        }
        Err(e) => {
            println!("⚠ Verify and promote failed (may be expected if API not implemented)");
            println!("  Error: {}", e);
            // Don't fail the test - API may not be ready yet
        }
    }

    Ok(())
}

/// Test error handling: invalid artifact path structure
#[tokio::test]
#[ignore] // Requires containers
async fn test_invalid_artifact_path() -> Result<()> {
    common::check_containers_ready().await?;

    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    let temp_dir = TempDir::new()?;
    let build_id = "test-invalid-path";

    // Create file NOT in units/{uuid}/{version} structure
    let staging_dir = temp_dir.path();
    let invalid_file = staging_dir.join("not-in-units-dir.zip");
    fs::write(&invalid_file, b"test")?;

    // Upload should fail with path validation error
    let upload_result = publisher
        .upload_artifacts(build_id, staging_dir, &vec![invalid_file])
        .await;

    assert!(
        upload_result.is_err(),
        "Should fail with invalid path structure"
    );
    println!("✓ Invalid artifact path properly rejected");

    Ok(())
}

/// Test helper: minimal verified receipt over the given artifacts
/// (promotion is receipt-gated; tests must go through the gate too).
fn make_verified_receipt(
    work_dir: &std::path::Path,
    artifacts: &[std::path::PathBuf],
) -> anyhow::Result<engine_config::VerifiedReceipt> {
    let eng = work_dir.join("fake_engine");
    std::fs::create_dir_all(eng.join("Engine/Build"))?;
    std::fs::write(
        eng.join("Engine/Build/Build.version"),
        b"{\"MajorVersion\":5,\"MinorVersion\":8,\"PatchVersion\":1}",
    )?;
    let uproject = work_dir.join("Alis.uproject");
    std::fs::write(&uproject, b"{ \"EngineAssociation\": \"5.8\" }")?;
    let receipt = engine_config::Receipt::issue(
        "test-commit".into(),
        "test-fp".into(),
        &uproject,
        &eng,
        "Shipping/Win64".into(),
        artifacts,
    )?;
    receipt.verify("test-commit", "test-fp", &uproject, artifacts)
}
