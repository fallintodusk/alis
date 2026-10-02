//! ZIP archive builder for code artifacts

use anyhow::{Context, Result};
use std::fs::File;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use zip::write::{FileOptions, ZipWriter};
use zip::CompressionMethod;

/// ZIP builder for creating code.zip
pub struct ZipBuilder {
    base_path: PathBuf,
}

impl ZipBuilder {
    pub fn new<P: AsRef<Path>>(base_path: P) -> Self {
        Self {
            base_path: base_path.as_ref().to_path_buf(),
        }
    }

    /// Create code.zip from list of files (deterministic - sorted, fixed timestamps)
    pub fn create_archive<P: AsRef<Path>>(
        &self,
        files: &[PathBuf],
        output_path: P,
    ) -> Result<()> {
        let output = output_path.as_ref();

        // Create parent directory if needed
        if let Some(parent) = output.parent() {
            std::fs::create_dir_all(parent)?;
        }

        let file = File::create(output)
            .with_context(|| format!("Failed to create zip file: {}", output.display()))?;

        let mut zip = ZipWriter::new(file);

        // Files should already be sorted by caller for deterministic builds
        for file_path in files {
            if !file_path.exists() {
                tracing::warn!("Skipping non-existent file: {}", file_path.display());
                continue;
            }

            // Calculate relative path from base
            let relative_path = file_path
                .strip_prefix(&self.base_path)
                .unwrap_or(file_path);

            // Normalize path separators to forward slashes (deterministic across platforms)
            let name = relative_path
                .to_str()
                .ok_or_else(|| anyhow::anyhow!("Invalid UTF-8 in path"))?
                .replace('\\', "/");

            // Per-file compression strategy: binaries = Stored, text = Deflated
            let ext = relative_path
                .extension()
                .and_then(|e| e.to_str())
                .unwrap_or("")
                .to_ascii_lowercase();

            let compression = match ext.as_str() {
                "dll" | "pdb" | "modules" | "exe" => CompressionMethod::Stored, // Binaries: no compression
                _ => CompressionMethod::Deflated, // Text files: compress
            };

            // Fixed timestamp for reproducible builds (1980-01-01 00:00:00)
            let fixed_time = zip::DateTime::from_date_and_time(1980, 1, 1, 0, 0, 0)
                .map_err(|_| anyhow::anyhow!("Failed to create fixed timestamp"))?;

            let options = FileOptions::default()
                .compression_method(compression)
                .last_modified_time(fixed_time)
                .unix_permissions(0o755);

            tracing::debug!("Adding to zip: {} (compression: {:?})", name, compression);

            zip.start_file(&name, options)?;

            let mut f = File::open(file_path)
                .with_context(|| format!("Failed to open file: {}", file_path.display()))?;

            std::io::copy(&mut f, &mut zip)?;
        }

        zip.finish()?;

        tracing::info!("Created deterministic code.zip with {} files at: {}", files.len(), output.display());

        Ok(())
    }

    /// Add single file to existing archive
    pub fn add_file<P1: AsRef<Path>, P2: AsRef<Path>>(
        &self,
        zip: &mut ZipWriter<File>,
        file_path: P1,
        archive_name: P2,
    ) -> Result<()> {
        let path = file_path.as_ref();
        let name = archive_name.as_ref()
            .to_str()
            .ok_or_else(|| anyhow::anyhow!("Invalid UTF-8 in archive name"))?;

        let options = FileOptions::default()
            .compression_method(CompressionMethod::Deflated)
            .unix_permissions(0o755);

        zip.start_file(name, options)?;

        let mut f = File::open(path)
            .with_context(|| format!("Failed to open file: {}", path.display()))?;

        let mut buffer = Vec::new();
        f.read_to_end(&mut buffer)?;
        zip.write_all(&buffer)?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_zip_builder_creation() {
        let builder = ZipBuilder::new(".");
        assert!(!builder.base_path.to_string_lossy().is_empty());
    }

    #[test]
    fn test_create_archive() {
        let temp_dir = env::temp_dir();
        let test_file = temp_dir.join("test_zip_input.txt");
        let zip_output = temp_dir.join("test_output.zip");

        // Create test file
        {
            let mut file = File::create(&test_file).unwrap();
            file.write_all(b"test content").unwrap();
        }

        let builder = ZipBuilder::new(&temp_dir);
        let files = vec![test_file.clone()];

        let result = builder.create_archive(&files, &zip_output);
        assert!(result.is_ok());
        assert!(zip_output.exists());

        // Cleanup
        let _ = std::fs::remove_file(test_file);
        let _ = std::fs::remove_file(zip_output);
    }
}
