use anyhow::{Context, Result};
use std::fs;
use std::path::{Path, PathBuf};

pub fn detect_project(repo_path: &Path) -> Result<(String, PathBuf)> {
    for entry in fs::read_dir(repo_path)
        .with_context(|| format!("Failed to read {}", repo_path.display()))?
    {
        let path = entry?.path();
        if path.extension().and_then(|s| s.to_str()) == Some("uproject") {
            if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                return Ok((stem.to_string(), repo_path.to_path_buf()));
            }
        }
    }

    anyhow::bail!(
        "No .uproject file found under {} (required for BuildUnit detection)",
        repo_path.display()
    );
}
