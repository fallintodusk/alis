//! Artifact discovery for IoStore and loose files

use anyhow::Result;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

/// Discovered artifacts from staged build
#[derive(Debug, Clone)]
pub struct DiscoveredArtifacts {
    pub iostore_files: Vec<PathBuf>,
    pub loose_files: Vec<PathBuf>,
    pub code_files: Vec<PathBuf>,
}

/// Artifact discoverer
pub struct ArtifactDiscoverer;

impl ArtifactDiscoverer {
    pub fn new() -> Self {
        Self
    }

    /// Discover all artifacts in staged directory
    pub fn discover<P: AsRef<Path>>(&self, staged_path: P) -> Result<DiscoveredArtifacts> {
        let path = staged_path.as_ref();

        let mut iostore_files = Vec::new();
        let mut loose_files = Vec::new();
        let mut code_files = Vec::new();

        for entry in WalkDir::new(path)
            .into_iter()
            .filter_entry(|e| {
                // CRITICAL: Exclude Content/ directory to prevent leaking content into code.zip
                // When code_path = staged plugin root, we only want .uplugin, Binaries/, Config/, Resources/
                // Content/** and IoStore files are handled separately via iostore_path
                if e.file_type().is_dir() {
                    let dir_name = e.file_name().to_string_lossy();
                    // Skip Content, Intermediate, Saved directories
                    !matches!(dir_name.as_ref(), "Content" | "Intermediate" | "Saved")
                } else {
                    true
                }
            })
            .filter_map(|e| e.ok())
        {
            if !entry.file_type().is_file() {
                continue;
            }

            let file_path = entry.path().to_path_buf();

            if let Some(ext) = file_path.extension().and_then(|s| s.to_str()) {
                match ext {
                    // IoStore container files (.pak contains loose files, .utoc/.ucas contain assets)
                    "pak" | "utoc" | "ucas" => {
                        iostore_files.push(file_path);
                    }
                    // Code artifacts (will go into code.zip)
                    "uplugin" | "uproject" | "dll" | "pdb" | "exe" | "json" | "ini" | "txt" | "modules" => {
                        code_files.push(file_path);
                    }
                    // Loose files (UE assets, configs, etc.)
                    _ => {
                        loose_files.push(file_path);
                    }
                }
            }
        }

        tracing::info!(
            "Discovered: {} IoStore, {} code, {} loose files",
            iostore_files.len(),
            code_files.len(),
            loose_files.len()
        );

        Ok(DiscoveredArtifacts {
            iostore_files,
            loose_files,
            code_files,
        })
    }

    /// Check if file should be included in code.zip
    pub fn is_code_artifact<P: AsRef<Path>>(&self, file_path: P) -> bool {
        let path = file_path.as_ref();

        if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
            matches!(ext, "uplugin" | "uproject" | "dll" | "pdb" | "exe" | "json" | "ini" | "txt" | "modules")
        } else {
            false
        }
    }

    /// Check if file is IoStore container
    pub fn is_iostore_file<P: AsRef<Path>>(&self, file_path: P) -> bool {
        let path = file_path.as_ref();

        if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
            matches!(ext, "pak" | "utoc" | "ucas")
        } else {
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_discoverer_creation() {
        let discoverer = ArtifactDiscoverer::new();
        assert!(discoverer.is_code_artifact("test.dll"));
        assert!(discoverer.is_iostore_file("test.utoc"));
        assert!(!discoverer.is_code_artifact("test.uasset"));
    }

    #[test]
    fn test_is_code_artifact() {
        let discoverer = ArtifactDiscoverer::new();

        assert!(discoverer.is_code_artifact("Plugin.uplugin"));
        assert!(discoverer.is_code_artifact("Module.dll"));
        assert!(discoverer.is_code_artifact("Debug.pdb"));
        assert!(!discoverer.is_code_artifact("Asset.uasset"));
    }

    #[test]
    fn test_is_iostore_file() {
        let discoverer = ArtifactDiscoverer::new();

        assert!(discoverer.is_iostore_file("pakchunk0.pak"));
        assert!(discoverer.is_iostore_file("global.utoc"));
        assert!(discoverer.is_iostore_file("chunk0.ucas"));
        assert!(!discoverer.is_iostore_file("test.dll"));
    }
}
