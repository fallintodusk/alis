//! ArtifactPackager crate - Packages build outputs and computes hashes
//!
//! Consumed by: BuildCli
//! Consumes: Staged build outputs from UeBuildExecutor
//!
//! Responsibilities:
//! - Create code.zip (uplugin + Binaries + Config + other non-UFS files)
//! - Copy IoStore outputs (*.utoc, *.ucas) as standalone artifacts
//! - Discover extra staged loose files
//! - Compute SHA-256 for each artifact
//! - Sign DLLs with Authenticode (placeholder for now)
//! - Emit metadata for ManifestStore

mod artifact_discoverer;
mod hash_computer;
mod zip_builder;

pub use artifact_discoverer::{ArtifactDiscoverer, DiscoveredArtifacts};
pub use hash_computer::compute_file_hash;
pub use zip_builder::ZipBuilder;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

/// Artifact metadata for CDN manifest
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ArtifactMetadata {
    pub artifact_type: String, // "code", "iostore", "loose"
    pub file_name: String,
    pub sha256: String,
    pub size_bytes: u64,
}

/// Package result with all artifacts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackageResult {
    pub plugin_name: String,
    pub uuid: String,      // Plugin UUID (RFC 4122 format)
    pub version: String,   // Plugin version (from build_id)
    pub code_artifact: ArtifactMetadata,
    pub assets: Vec<ArtifactMetadata>,
}

/// ArtifactPackager - Packages artifacts
pub struct ArtifactPackager {
    staging_dir: PathBuf,
    discoverer: ArtifactDiscoverer,
}

impl ArtifactPackager {
    pub fn new(staging_dir: PathBuf) -> Self {
        Self {
            staging_dir,
            discoverer: ArtifactDiscoverer::new(),
        }
    }

    /// Package a plugin's build outputs into CDN-compatible structure
    /// Creates: staging/{buildId}/units/{uuid}/{version}/code.zip
    pub fn package_plugin(
        &self,
        plugin_name: &str,
        uuid: &str,
        version: &str,
        code_path: &Path,
        iostore_path: &Path,
    ) -> Result<PackageResult> {
        tracing::info!("Packaging plugin: {} (UUID: {}, version: {})", plugin_name, uuid, version);

        if !code_path.exists() {
            return Err(anyhow::anyhow!(
                "Code output path does not exist: {}",
                code_path.display()
            ));
        }
        if !iostore_path.exists() {
            return Err(anyhow::anyhow!(
                "IoStore output path does not exist: {}",
                iostore_path.display()
            ));
        }

        // CRITICAL: Ensure code_path is staged plugin root (contains .uplugin)
        let expected_uplugin = code_path.join(format!("{}.uplugin", plugin_name));
        if !expected_uplugin.exists() {
            return Err(anyhow::anyhow!(
                "code_path must be staged plugin root (missing {}):\n\
                \n\
                Expected: {}\n\
                Actual code_path: {}\n\
                \n\
                TROUBLESHOOTING:\n\
                - Verify executor passes dlc_staging root, not dlc_staging/Binaries\n\
                - Check that DLC cook staged .uplugin file correctly\n\
                - Ensure BuildCookRun -stage ran successfully",
                expected_uplugin.file_name().unwrap().to_string_lossy(),
                expected_uplugin.display(),
                code_path.display()
            ));
        }

        // Optional hardener: Validate iostore_path ends with Content/Paks
        let iostore_str = iostore_path.to_string_lossy();
        if !iostore_str.ends_with("Content/Paks") && !iostore_str.ends_with("Content\\Paks") {
            tracing::warn!(
                "iostore_path does not end with Content/Paks - may be misrouted: {}",
                iostore_path.display()
            );
        }

        // Discover artifacts in code and IoStore directories separately
        let code_discovered = self.discoverer.discover(code_path)?;
        let iostore_discovered = self.discoverer.discover(iostore_path)?;

        // Create units/{uuid}/{version}/ directory structure (matches CDN manifest URLs)
        let plugin_output_dir = self.staging_dir.join("units").join(uuid).join(version);
        std::fs::create_dir_all(&plugin_output_dir)?;

        // Create code.zip in correct location
        let code_zip_path = plugin_output_dir.join("code.zip");
        self.create_code_zip(code_path, &code_zip_path, &code_discovered.code_files)?;

        // Compute hash for code.zip
        let code_hash = compute_file_hash(&code_zip_path)?;
        let code_size = std::fs::metadata(&code_zip_path)?.len();

        let code_artifact = ArtifactMetadata {
            artifact_type: "code".to_string(),
            file_name: "code.zip".to_string(),
            sha256: code_hash,
            size_bytes: code_size,
        };

        // Process IoStore files - copy to plugin_output_dir
        let mut assets = Vec::new();
        let mut iostore_files = code_discovered.iostore_files;
        iostore_files.extend(iostore_discovered.iostore_files);

        for iostore_file in &iostore_files {
            let file_name = iostore_file
                .file_name()
                .and_then(|n| n.to_str())
                .ok_or_else(|| anyhow::anyhow!("Invalid file name"))?;

            let dest_path = plugin_output_dir.join(file_name);
            std::fs::copy(iostore_file, &dest_path)?;

            let metadata = self.create_artifact_metadata(&dest_path, "iostore")?;
            assets.push(metadata);
        }

        // Process loose files (if any) - copy to plugin_output_dir
        for loose_file in &code_discovered.loose_files {
            let file_name = loose_file
                .file_name()
                .and_then(|n| n.to_str())
                .ok_or_else(|| anyhow::anyhow!("Invalid file name"))?;

            let dest_path = plugin_output_dir.join(file_name);
            std::fs::copy(loose_file, &dest_path)?;

            let metadata = self.create_artifact_metadata(&dest_path, "loose")?;
            assets.push(metadata);
        }

        tracing::info!(
            "Packaged plugin '{}': 1 code artifact, {} asset files",
            plugin_name,
            assets.len()
        );

        Ok(PackageResult {
            plugin_name: plugin_name.to_string(),
            uuid: uuid.to_string(),
            version: version.to_string(),
            code_artifact,
            assets,
        })
    }

    /// Create code.zip from staged files
    pub fn create_code_zip(
        &self,
        code_root: &Path,
        output_path: &PathBuf,
        code_files: &[PathBuf],
    ) -> Result<()> {
        tracing::info!("Creating code.zip with {} files", code_files.len());

        // CRITICAL: Enforce staged DLL presence before zipping
        let has_dll = code_files.iter().any(|p| {
            p.extension()
                .and_then(|e| e.to_str())
                .map(|e| e.eq_ignore_ascii_case("dll"))
                .unwrap_or(false)
        });

        if !has_dll {
            return Err(anyhow::anyhow!(
                "No DLLs found in staged code files - cannot create code.zip\n\
                \n\
                Expected: Saved/StagedBuilds/.../Plugins/<Plugin>/Binaries/Win64/*.dll\n\
                \n\
                TROUBLESHOOTING:\n\
                - Verify incremental build completed successfully\n\
                - Check dll_staging module ran (should see DLL staging log)\n\
                - Ensure plugin has Binaries/Win64/*.dll in source or staged path"
            ));
        }

        // Sort files for deterministic zip (reproducible builds)
        let mut sorted_files = code_files.to_vec();
        sorted_files.sort();

        let zip_builder = ZipBuilder::new(code_root);
        zip_builder.create_archive(&sorted_files, output_path)?;

        Ok(())
    }

    /// Create artifact metadata for a file
    fn create_artifact_metadata<P: AsRef<Path>>(
        &self,
        file_path: P,
        artifact_type: &str,
    ) -> Result<ArtifactMetadata> {
        let path = file_path.as_ref();

        let file_name = path
            .file_name()
            .and_then(|n| n.to_str())
            .ok_or_else(|| anyhow::anyhow!("Invalid file name"))?
            .to_string();

        let sha256 = compute_file_hash(path)?;
        let size_bytes = std::fs::metadata(path)?.len();

        Ok(ArtifactMetadata {
            artifact_type: artifact_type.to_string(),
            file_name,
            sha256,
            size_bytes,
        })
    }

    /// Compute SHA-256 hash for a file (convenience wrapper)
    pub fn compute_sha256(&self, file_path: &PathBuf) -> Result<String> {
        compute_file_hash(file_path)
    }

    /// Sign DLL with Authenticode (placeholder)
    pub fn sign_dll<P: AsRef<Path>>(&self, dll_path: P) -> Result<String> {
        let path = dll_path.as_ref();

        tracing::warn!("DLL signing not implemented, skipping: {}", path.display());

        // Placeholder - would integrate with signtool.exe on Windows
        // For now, return empty thumbprint
        Ok(String::new())
    }

    /// Copy IoStore file to staging
    pub fn copy_iostore_file<P1: AsRef<Path>, P2: AsRef<Path>>(
        &self,
        source: P1,
        dest: P2,
    ) -> Result<()> {
        let src = source.as_ref();
        let dst = dest.as_ref();

        if let Some(parent) = dst.parent() {
            std::fs::create_dir_all(parent)?;
        }

        std::fs::copy(src, dst)
            .with_context(|| format!("Failed to copy {} to {}", src.display(), dst.display()))?;

        tracing::debug!("Copied IoStore file: {} -> {}", src.display(), dst.display());

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use std::fs::File;
    use std::io::Write;

    #[test]
    fn test_packager_creation() {
        let packager = ArtifactPackager::new(PathBuf::from("staging"));
        assert!(!packager.staging_dir.to_string_lossy().is_empty());
    }

    #[test]
    fn test_compute_sha256() {
        let temp_dir = env::temp_dir();
        let test_file = temp_dir.join("test_hash_packager.txt");

        {
            let mut file = File::create(&test_file).unwrap();
            file.write_all(b"test content").unwrap();
        }

        let packager = ArtifactPackager::new(temp_dir.clone());
        let hash = packager.compute_sha256(&test_file).unwrap();

        assert_eq!(hash.len(), 64); // SHA-256 = 64 hex chars

        // Cleanup
        let _ = std::fs::remove_file(test_file);
    }

    #[test]
    fn test_create_artifact_metadata() {
        let temp_dir = env::temp_dir();
        let test_file = temp_dir.join("test_metadata.txt");

        {
            let mut file = File::create(&test_file).unwrap();
            file.write_all(b"test").unwrap();
        }

        let packager = ArtifactPackager::new(temp_dir.clone());
        let metadata = packager.create_artifact_metadata(&test_file, "test").unwrap();

        assert_eq!(metadata.artifact_type, "test");
        assert_eq!(metadata.file_name, "test_metadata.txt");
        assert_eq!(metadata.sha256.len(), 64);
        assert_eq!(metadata.size_bytes, 4);

        // Cleanup
        let _ = std::fs::remove_file(test_file);
    }
}
