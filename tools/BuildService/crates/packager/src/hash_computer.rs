//! SHA-256 hash computation for artifacts

use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::Read;
use std::path::Path;

/// Compute SHA-256 hash for a file
pub fn compute_file_hash<P: AsRef<Path>>(file_path: P) -> Result<String> {
    let path = file_path.as_ref();

    let mut file = File::open(path)
        .with_context(|| format!("Failed to open file for hashing: {}", path.display()))?;

    let mut hasher = Sha256::new();
    let mut buffer = [0u8; 8192];

    loop {
        let bytes_read = file.read(&mut buffer)?;
        if bytes_read == 0 {
            break;
        }
        hasher.update(&buffer[..bytes_read]);
    }

    let result = hasher.finalize();
    Ok(format!("{:x}", result))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::env;

    #[test]
    fn test_compute_hash() {
        let temp_dir = env::temp_dir();
        let test_file = temp_dir.join("test_hash.txt");

        {
            let mut file = File::create(&test_file).unwrap();
            file.write_all(b"test content").unwrap();
        }

        let hash = compute_file_hash(&test_file).unwrap();
        assert_eq!(hash.len(), 64); // SHA-256 produces 64 hex characters

        // Cleanup
        let _ = std::fs::remove_file(test_file);
    }
}
