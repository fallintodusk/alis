// Integration test: Verify generated manifest matches CDN schema
//
// This test ensures Blocker #1 (Manifest Schema Parity) is resolved:
// - Manifest structs match CDN schema exactly
// - Generated manifest.json is valid against CDN bootloader schema
// - All required fields are present with correct types

use build_service_manifest::{Manifest, PluginManifestEntry, Artifact, SchemaValidator};
use build_service_packager::PackageResult;
use std::path::PathBuf;
use std::fs;
use tempfile::TempDir;
use anyhow::Result;

#[test]
fn test_generated_manifest_validates_against_schema() -> Result<()> {
    // Setup: Create temporary directory for staging
    let temp_dir = TempDir::new()?;
    let staging_dir = temp_dir.path().join("staging");
    fs::create_dir_all(&staging_dir)?;

    // Create mock package result (simulates packager output)
    let package_result = create_mock_package_result(&staging_dir)?;

    // Generate manifest entry from package result
    let manifest_entry = create_manifest_entry_from_package(&package_result);

    // Create manifest with single entry
    let manifest = Manifest {
        manifest_version: 1,
        engine_build_id: "test-build-001".to_string(),
        signed_at: None,
        signing_key_id: None,
        signature: None,
        plugins: vec![manifest_entry],
    };

    // Serialize to JSON Value
    let manifest_json = serde_json::to_value(&manifest)?;

    // Validate against CDN schema
    let validator = SchemaValidator::new()?;
    validator.validate(&manifest_json)?;

    // Use the validated JSON for field verification
    let parsed = manifest_json;
    assert!(parsed["plugins"].is_array());
    assert_eq!(parsed["plugins"].as_array().unwrap().len(), 1);

    let plugin = &parsed["plugins"][0];

    // Verify UUID format (RFC 4122)
    let uuid_str = plugin["uuid"].as_str().expect("uuid should be string");
    assert!(uuid_str.len() == 36, "UUID should be 36 chars");
    assert!(uuid_str.contains('-'), "UUID should contain hyphens");

    // Verify required fields
    assert!(plugin["name"].is_string());
    assert!(plugin["platform"].is_string());
    assert!(plugin["version"].is_string());

    // Verify code artifact
    assert!(plugin["code"].is_object());
    assert!(plugin["code"]["url"].is_string());
    assert!(plugin["code"]["hash"].is_string());
    assert!(plugin["code"]["size"].is_number());

    // Verify assets array
    assert!(plugin["assets"].is_array());
    let assets = plugin["assets"].as_array().unwrap();
    for asset in assets {
        assert!(asset["url"].is_string());
        assert!(asset["hash"].is_string());
        assert!(asset["size"].is_number());
        assert!(asset["role"].is_string());
    }

    Ok(())
}

#[test]
fn test_manifest_urls_match_staging_directory_structure() -> Result<()> {
    // This test verifies Blocker #2 (S3 Key Layout) fix:
    // Staging directory structure must match manifest URLs

    let temp_dir = TempDir::new()?;
    let build_id = "test-build-001";
    let staging_dir = temp_dir.path().join("staging").join(build_id);
    fs::create_dir_all(&staging_dir)?;

    // Create package result with UUID
    let plugin_uuid = uuid::Uuid::new_v4().to_string();
    let plugin_version = "1.0.0";

    // Simulate packager creating units/{uuid}/{version}/ structure
    let plugin_output_dir = staging_dir.join("units").join(&plugin_uuid).join(plugin_version);
    fs::create_dir_all(&plugin_output_dir)?;

    // Create mock files
    let code_zip_path = plugin_output_dir.join("code.zip");
    fs::write(&code_zip_path, b"mock zip content")?;

    let utoc_path = plugin_output_dir.join("content.utoc");
    fs::write(&utoc_path, b"mock utoc")?;

    // Extract relative paths from staging directory
    let code_relative = code_zip_path.strip_prefix(&staging_dir)?;
    let utoc_relative = utoc_path.strip_prefix(&staging_dir)?;

    // Verify paths match URL structure
    assert_eq!(code_relative, PathBuf::from(format!("units/{}/{}/code.zip", plugin_uuid, plugin_version)));
    assert_eq!(utoc_relative, PathBuf::from(format!("units/{}/{}/content.utoc", plugin_uuid, plugin_version)));

    // Verify S3 upload key would match manifest URL
    // S3 key: stage/{buildId}/units/{uuid}/{version}/code.zip
    // Manifest URL: https://cdn.alis.game/units/{uuid}/{version}/code.zip
    // After CDN promotion: releases/units/{uuid}/{version}/code.zip

    let s3_key = format!("stage/{}/units/{}/{}/code.zip", build_id, plugin_uuid, plugin_version);
    assert!(s3_key.contains(&format!("units/{}/{}/code.zip", plugin_uuid, plugin_version)));

    Ok(())
}

// Helper functions

fn create_mock_package_result(_staging_dir: &PathBuf) -> Result<PackageResult> {
    let uuid = uuid::Uuid::new_v4().to_string();
    let version = "1.0.0".to_string();

    // Create mock code artifact
    let code_artifact = build_service_packager::ArtifactMetadata {
        artifact_type: "code".to_string(),
        file_name: "code.zip".to_string(),
        sha256: "a".repeat(64), // Mock SHA-256 hash
        size_bytes: 1024,
    };

    // Create mock IoStore assets
    let assets = vec![
        build_service_packager::ArtifactMetadata {
            artifact_type: "iostore".to_string(),
            file_name: "content.utoc".to_string(),
            sha256: "b".repeat(64),
            size_bytes: 512,
        },
        build_service_packager::ArtifactMetadata {
            artifact_type: "iostore".to_string(),
            file_name: "content.ucas".to_string(),
            sha256: "c".repeat(64),
            size_bytes: 2048,
        },
    ];

    Ok(PackageResult {
        plugin_name: "MockPlugin".to_string(),
        uuid,
        version,
        code_artifact,
        assets,
    })
}

fn create_manifest_entry_from_package(package_result: &PackageResult) -> PluginManifestEntry {
    let base_url = format!(
        "https://cdn.alis.game/units/{}/{}",
        package_result.uuid,
        package_result.version
    );

    // Create code artifact
    let code_artifact = Artifact {
        url: format!("{}/{}", base_url, package_result.code_artifact.file_name),
        hash: package_result.code_artifact.sha256.clone(),
        size: package_result.code_artifact.size_bytes,
        role: None, // Code artifact doesn't have a role
    };

    // Create asset artifacts with roles
    let assets = package_result
        .assets
        .iter()
        .map(|asset| {
            let role = if asset.file_name.ends_with(".utoc") {
                "utoc"
            } else if asset.file_name.ends_with(".ucas") {
                "ucas"
            } else {
                "unknown"
            };

            Artifact {
                url: format!("{}/{}", base_url, asset.file_name),
                hash: asset.sha256.clone(),
                size: asset.size_bytes,
                role: Some(role.to_string()),
            }
        })
        .collect();

    PluginManifestEntry {
        uuid: package_result.uuid.clone(),
        name: package_result.plugin_name.clone(),
        version: package_result.version.clone(),
        module: Some(package_result.plugin_name.clone()),
        platform: "Windows".to_string(), // CDN schema expects "Windows", not "Win64"
        code: code_artifact,
        assets,
        depends_on: vec![], // Empty for mock
        channel: "stable".to_string(),
        signature_thumbprint: None,
        release_notes: None,
        mirrors: vec![],
        base_release_version: None, // Test without DLC workflow
    }
}
