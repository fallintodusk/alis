//! Atomic file writing using temp + rename pattern

use anyhow::{Context, Result};
use std::fs::File;
use std::io::Write;
use std::path::Path;

/// Atomic file writer
pub struct AtomicWriter;

impl AtomicWriter {
    /// Write data to file atomically (write to .tmp, then rename)
    pub fn write_atomic<P: AsRef<Path>>(path: P, data: &[u8]) -> Result<()> {
        let target_path = path.as_ref();
        let tmp_path = target_path.with_extension("tmp");

        // Create parent directory if needed
        if let Some(parent) = target_path.parent() {
            std::fs::create_dir_all(parent)?;
        }

        // Write to temporary file
        {
            let mut file = File::create(&tmp_path)
                .with_context(|| format!("Failed to create temp file: {}", tmp_path.display()))?;

            file.write_all(data)
                .with_context(|| "Failed to write to temp file")?;

            file.flush()?;
        }

        // Atomic rename
        std::fs::rename(&tmp_path, target_path)
            .with_context(|| format!("Failed to rename {} to {}", tmp_path.display(), target_path.display()))?;

        tracing::debug!("Wrote file atomically: {}", target_path.display());

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_atomic_write() {
        let temp_dir = env::temp_dir();
        let test_file = temp_dir.join("test_atomic.txt");

        let data = b"test content";
        let result = AtomicWriter::write_atomic(&test_file, data);

        assert!(result.is_ok());
        assert!(test_file.exists());

        let contents = std::fs::read(&test_file).unwrap();
        assert_eq!(contents, data);

        // Cleanup
        let _ = std::fs::remove_file(test_file);
    }

    #[test]
    fn test_atomic_write_creates_parent() {
        let temp_dir = env::temp_dir();
        let nested_dir = temp_dir.join("atomic_test_nested");
        let test_file = nested_dir.join("test.txt");

        let result = AtomicWriter::write_atomic(&test_file, b"test");

        assert!(result.is_ok());
        assert!(test_file.exists());

        // Cleanup
        let _ = std::fs::remove_dir_all(nested_dir);
    }
}
