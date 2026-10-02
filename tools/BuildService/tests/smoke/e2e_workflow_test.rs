// End-to-End Smoke Test - Full Build Service Pipeline
//
// Tests the complete workflow:
// 1. Scan for changed plugins (mock change via test data)
// 2. Determine build order
// 3. Build plugin (mock UE build output)
// 4. Package artifacts
// 5. Update manifest
// 6. Publish via CDN Control API
// 7. Verify artifacts are accessible
//
// Requirements:
// - Running containers (nginx, CDN Control API)
// - UE installation (for real build testing)
//
// Run with: cargo test --test e2e_workflow_test -- --ignored
//
// For quick smoke test without UE:
// Run with: cargo test --test e2e_workflow_test test_publish_pipeline_with_mock_artifacts -- --ignored

#[path = "../common/mod.rs"]
mod common;

use anyhow::{Context, Result};
use build_service_packager::{ArtifactPackager, PackageResult};
use build_service_manifest::{ManifestStore, Manifest, PluginManifestEntry, Artifact};
use build_service_publisher::CdnPublisher;
use std::fs;
use tempfile::TempDir;

/// Test the publish pipeline with mock artifacts (no UE build)
///
/// This is the fastest smoke test that verifies:
/// - Artifact packaging
/// - Manifest generation
/// - Upload via CDN Control API
/// - Manifest upload via CDN Control API
#[tokio::test]
#[ignore] // Requires containers
async fn test_publish_pipeline_with_mock_artifacts() -> Result<()> {
    println!("\n=== E2E Smoke Test: Publish Pipeline ===\n");

    // Check containers are ready
    println!("[1/6] Checking containers...");
    common::print_test_config();
    common::check_containers_ready().await?;
    println!("✓ Containers ready\n");

    // Setup test environment
    let temp_dir = TempDir::new()?;
    let build_id = format!("smoke-{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
    let plugin_name = "TestPlugin";
    let plugin_uuid = uuid::Uuid::new_v4().to_string();
    let plugin_version = "1.0.0";

    println!("[2/6] Creating mock build artifacts...");
    println!("  Build ID: {}", build_id);
    println!("  Plugin: {} ({})", plugin_name, plugin_uuid);

    // Create mock staged plugin output (simulates UE build output)
    let staged_plugin_dir = temp_dir.path().join("staged").join(plugin_name);
    fs::create_dir_all(&staged_plugin_dir)?;

    // Mock DLL binaries
    let binaries_dir = staged_plugin_dir.join("Binaries").join("Win64");
    fs::create_dir_all(&binaries_dir)?;
    fs::write(binaries_dir.join("ProjectTestPlugin.dll"), b"mock DLL binary content")?;

    // Mock IoStore content files
    let content_dir = staged_plugin_dir.join("Content");
    fs::create_dir_all(&content_dir)?;
    fs::write(content_dir.join("content.utoc"), b"mock utoc content with metadata")?;
    fs::write(content_dir.join("content.ucas"), b"mock ucas content with asset data")?;

    println!("✓ Mock artifacts created\n");

    // Step 1: Package artifacts
    println!("[3/6] Packaging artifacts...");
    let staging_dir = temp_dir.path().join("staging").join(&build_id);
    let packager = ArtifactPackager::new(staging_dir.clone());

    let package_result = packager.package_plugin(
        plugin_name,
        &plugin_uuid,
        plugin_version,
        &staged_plugin_dir,
        &staged_plugin_dir, // iostore_path (same as code for test)
    )?;

    println!("✓ Packaged {} artifacts:", package_result.plugin_name);
    println!("  Code: {} ({} bytes, hash: {}...)",
        package_result.code_artifact.file_name,
        package_result.code_artifact.size_bytes,
        &package_result.code_artifact.sha256[..16]
    );
    println!("  Assets: {}", package_result.assets.len());
    println!();

    // Step 2: Generate manifest
    println!("[4/6] Generating manifest...");
    let manifest_file = temp_dir.path().join("manifest.json");
    let manifest_store = ManifestStore::new(manifest_file.clone())?;

    let mut manifest = Manifest {
        manifest_version: 1,
        engine_build_id: format!("UE5.5-{}", build_id),
        signed_at: None,
        signing_key_id: None,
        signature: None,
        plugins: Vec::new(),
    };

    // Create manifest entry from package result
    let base_url = format!("https://cdn.alis.game/units/{}/{}", plugin_uuid, plugin_version);

    let code_artifact = Artifact {
        url: format!("{}/code.zip", base_url),
        hash: package_result.code_artifact.sha256.clone(),
        size: package_result.code_artifact.size_bytes,
        role: None,
    };

    let assets: Vec<Artifact> = package_result.assets.iter()
        .map(|a| {
            let role = if a.file_name.ends_with(".utoc") {
                Some("utoc".to_string())
            } else if a.file_name.ends_with(".ucas") {
                Some("ucas".to_string())
            } else {
                None
            };
            Artifact {
                url: format!("{}/{}", base_url, a.file_name),
                hash: a.sha256.clone(),
                size: a.size_bytes,
                role,
            }
        })
        .collect();

    let plugin_entry = PluginManifestEntry {
        uuid: plugin_uuid.clone(),
        name: plugin_name.to_string(),
        version: plugin_version.to_string(),
        module: Some(format!("Project{}", plugin_name)),
        platform: "Windows".to_string(),
        code: code_artifact,
        assets,
        depends_on: Vec::new(),
        channel: "experimental".to_string(),
        signature_thumbprint: None,
        release_notes: None,
        mirrors: Vec::new(),
        base_release_version: None, // Test without DLC workflow
    };

    manifest_store.update_plugin(&mut manifest, plugin_entry)?;
    manifest_store.save(&manifest)?;

    println!("✓ Manifest generated:");
    println!("  File: {}", manifest_file.display());
    println!("  Plugins: {}", manifest.plugins.len());
    println!();

    // Step 3: Create publisher (no initialization needed with Control API)
    println!("[5/6] Publishing to CDN via Control API...");
    let config = common::get_test_publisher_config();
    let publisher = CdnPublisher::new(config);

    // Step 4: Upload artifacts via Control API
    let mut artifact_paths = Vec::new();
    let units_dir = staging_dir.join("units").join(&plugin_uuid).join(plugin_version);

    for entry in fs::read_dir(&units_dir)? {
        let entry = entry?;
        if entry.path().is_file() {
            artifact_paths.push(entry.path());
        }
    }

    publisher.upload_artifacts(&build_id, &staging_dir, &artifact_paths).await?;
    println!("✓ Uploaded {} artifact files", artifact_paths.len());

    // Step 5: Upload manifest
    publisher.upload_manifest(&build_id, &manifest_file).await?;
    println!("✓ Uploaded manifest");
    println!();

    // Step 6: Verify and Promote
    println!("[6/7] Calling verify-and-promote...");
    let channel = "experimental";
    let receipt = make_verified_receipt(temp_dir.path(), &artifact_paths)
        .context("Failed to build test receipt")?;
    let verify_response = publisher.verify_and_promote(&receipt, &build_id, channel).await
        .context("Verify-and-promote failed")?;

    // Assert verification details
    assert_eq!(verify_response.status, "promoted", "Verify-and-promote should succeed");
    println!("✓ Verify-and-promote succeeded");
    println!("  Build ID: {}", verify_response.build_id);
    println!("  Bundles verified: {}", verify_response.bundles_verified);
    println!("  Bundles copied: {}", verify_response.bundles_copied);

    // Health check
    let is_healthy = publisher.health_check().await?;
    assert!(is_healthy, "CDN should be healthy after promotion");
    println!("✓ CDN health check passed");
    println!();

    // Step 7: Download from Public CDN
    println!("[7/7] Verifying public CDN download...");
    let public_cdn_url = common::get_public_cdn_url();

    // Download promoted manifest
    println!("  Downloading manifest from public CDN...");
    let manifest_url = format!("{}/manifest.json", public_cdn_url);
    let manifest_response = reqwest::get(&manifest_url)
        .await
        .with_context(|| format!("Failed to download manifest from {}", manifest_url))?;

    assert!(
        manifest_response.status().is_success(),
        "Manifest should be publicly accessible at {}",
        manifest_url
    );

    let downloaded_manifest: serde_json::Value = manifest_response
        .json()
        .await
        .context("Failed to parse downloaded manifest")?;

    println!("✓ Manifest downloaded from public CDN");
    println!("  Plugins in manifest: {}", downloaded_manifest["plugins"].as_array().unwrap().len());

    // Download each artifact and verify hash
    println!("  Downloading artifacts from public CDN...");

    for entry in fs::read_dir(&units_dir)? {
        let entry = entry?;
        let path = entry.path();

        if !path.is_file() {
            continue;
        }

        let filename = path.file_name().unwrap().to_str().unwrap();

        // Construct public CDN URL
        let artifact_url = format!(
            "{}/units/{}/{}/{}",
            public_cdn_url, plugin_uuid, plugin_version, filename
        );

        println!("  Downloading: {}", artifact_url);

        // Download artifact
        let artifact_response = reqwest::get(&artifact_url)
            .await
            .with_context(|| format!("Failed to download artifact from {}", artifact_url))?;

        assert!(
            artifact_response.status().is_success(),
            "Artifact should be publicly accessible: {}",
            artifact_url
        );

        let downloaded_bytes = artifact_response.bytes().await?;

        // Compute hash of downloaded artifact
        let downloaded_hash = common::compute_sha256(&downloaded_bytes);

        // Read original artifact and compute hash
        let original_bytes = fs::read(&path)?;
        let original_hash = common::compute_sha256(&original_bytes);

        // Verify hashes match
        assert_eq!(
            downloaded_hash,
            original_hash,
            "Downloaded artifact hash mismatch for {}: expected {}, got {}",
            filename,
            original_hash,
            downloaded_hash
        );

        // Verify sizes match
        assert_eq!(
            downloaded_bytes.len(),
            original_bytes.len(),
            "Downloaded artifact size mismatch for {}: expected {} bytes, got {} bytes",
            filename,
            original_bytes.len(),
            downloaded_bytes.len()
        );

        println!("    ✓ Hash verified: {}...", &downloaded_hash[..16]);
        println!("    ✓ Size verified: {} bytes", downloaded_bytes.len());
    }

    println!("✓ All artifacts verified on public CDN");
    println!("  Artifacts: {}", artifact_paths.len());
    println!();

    println!("=== Smoke Test PASSED ===");
    println!("Build ID: {}", build_id);
    println!("Plugin UUID: {}", plugin_uuid);
    println!("Uploaded via Control API: PUT /builds/{}/artifacts/{}/{}/...", build_id, plugin_uuid, plugin_version);
    println!("Promoted to: {}/units/{}/{}/", public_cdn_url, plugin_uuid, plugin_version);
    println!("Verified: {} artifacts downloaded and hash-checked", artifact_paths.len());
    println!();

    Ok(())
}

/// Test manifest schema validation during pipeline
///
/// Ensures generated manifest validates against CDN schema
#[tokio::test]
#[ignore] // Requires containers
async fn test_manifest_validation_in_pipeline() -> Result<()> {
    use build_service_manifest::SchemaValidator;

    println!("\n=== Smoke Test: Manifest Validation ===\n");

    let temp_dir = TempDir::new()?;
    let manifest_file = temp_dir.path().join("manifest.json");

    // Create a manifest
    let manifest = Manifest {
        manifest_version: 1,
        engine_build_id: "UE5.5-test".to_string(),
        signed_at: None,
        signing_key_id: None,
        signature: None,
        plugins: vec![
            PluginManifestEntry {
                uuid: uuid::Uuid::new_v4().to_string(),
                name: "TestPlugin".to_string(),
                version: "1.0.0".to_string(),
                module: Some("ProjectTestPlugin".to_string()),
                platform: "Windows".to_string(),
                code: Artifact {
                    url: "https://cdn.alis.game/units/test-uuid/1.0.0/code.zip".to_string(),
                    hash: "a".repeat(64),
                    size: 1024,
                    role: None,
                },
                assets: vec![],
                depends_on: vec![],
                channel: "dev".to_string(),
                signature_thumbprint: None,
                release_notes: None,
                mirrors: vec![],
                base_release_version: None, // Test without DLC workflow
            }
        ],
    };

    // Save manifest
    let manifest_store = ManifestStore::new(manifest_file.clone())?;
    manifest_store.save(&manifest)?;
    println!("✓ Manifest created");

    // Validate against schema
    let validator = SchemaValidator::new()?;
    let manifest_json = serde_json::to_value(&manifest)?;
    validator.validate(&manifest_json)?;
    println!("✓ Manifest validated against schema");

    println!("\n=== Manifest Validation PASSED ===\n");

    Ok(())
}

/// Helper to create realistic package result for testing
#[allow(dead_code)]
fn create_package_result(plugin_name: &str, uuid: &str, version: &str) -> PackageResult {
    use build_service_packager::ArtifactMetadata;

    PackageResult {
        plugin_name: plugin_name.to_string(),
        uuid: uuid.to_string(),
        version: version.to_string(),
        code_artifact: ArtifactMetadata {
            artifact_type: "code".to_string(),
            file_name: "code.zip".to_string(),
            sha256: "a".repeat(64),
            size_bytes: 1024,
        },
        assets: vec![
            ArtifactMetadata {
                artifact_type: "iostore".to_string(),
                file_name: "content.utoc".to_string(),
                sha256: "b".repeat(64),
                size_bytes: 512,
            },
            ArtifactMetadata {
                artifact_type: "iostore".to_string(),
                file_name: "content.ucas".to_string(),
                sha256: "c".repeat(64),
                size_bytes: 2048,
            },
        ],
    }
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
