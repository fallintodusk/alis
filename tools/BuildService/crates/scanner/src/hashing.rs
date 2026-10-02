use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::path::Path;
use walkdir::{DirEntry, WalkDir};

/// Skip noisy directories that don't affect build outputs
fn should_skip_dir(entry: &DirEntry) -> bool {
    if !entry.file_type().is_dir() {
        return false;
    }

    if let Some(name) = entry.file_name().to_str() {
        matches!(
            name,
            "Intermediate" | "Saved" | ".git" | ".vs" | "DerivedDataCache" | ".vscode"
        )
    } else {
        false
    }
}

pub fn compute_code_hash(plugin_path: &Path) -> Result<String> {
    let mut hasher = Sha256::new();
    // Hash only source inputs that define behavior/versioning
    // Exclude compiled outputs (Binaries/) to keep hashes stable across builds
    let interesting = ["Source", "Config"];

    // Collect all matching files first, then sort for deterministic hashing
    let mut files: Vec<_> = WalkDir::new(plugin_path)
        .follow_links(false)
        .into_iter()
        .filter_entry(|e| !should_skip_dir(e))
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter(|e| is_code_file(plugin_path, e.path(), &interesting))
        .map(|e| e.into_path())
        .collect();
    files.sort(); // Lexicographic path order for deterministic hashing

    for path in files {
        // Include relative path in hash to detect renames
        if let Ok(rel) = path.strip_prefix(plugin_path) {
            let rel_str = rel.to_string_lossy().replace('\\', "/");
            hasher.update(rel_str.as_bytes());
        }

        let data = std::fs::read(&path)
            .with_context(|| format!("Failed to read {}", path.display()))?;
        hasher.update(&data);
    }

    // Always include the .uplugin definition if present
    if let Some(name) = plugin_path.file_name().and_then(|s| s.to_str()) {
        let uplugin = plugin_path.join(format!("{}.uplugin", name));
        if uplugin.exists() {
            let data = std::fs::read(&uplugin)
                .with_context(|| format!("Failed to read {}", uplugin.display()))?;
            hasher.update(&data);
        }
    }

    Ok(format!("{:x}", hasher.finalize()))
}

pub fn compute_content_hash(plugin_path: &Path) -> Result<String> {
    let mut hasher = Sha256::new();
    let content_root = plugin_path.join("Content");

    if !content_root.exists() {
        return Ok(String::new());
    }

    // Collect all content files, then sort for deterministic hashing
    let mut files: Vec<_> = WalkDir::new(&content_root)
        .follow_links(false)
        .into_iter()
        .filter_entry(|e| !should_skip_dir(e))
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .map(|e| e.into_path())
        .collect();
    files.sort(); // Lexicographic path order for deterministic hashing

    for path in files {
        // Include relative path in hash to detect renames
        if let Ok(rel) = path.strip_prefix(&content_root) {
            let rel_str = rel.to_string_lossy().replace('\\', "/");
            hasher.update(rel_str.as_bytes());
        }

        let data = std::fs::read(&path)
            .with_context(|| format!("Failed to read {}", path.display()))?;
        hasher.update(&data);
    }

    Ok(format!("{:x}", hasher.finalize()))
}

fn is_code_file(plugin_root: &Path, path: &Path, prefixes: &[&str]) -> bool {
    if let Ok(relative) = path.strip_prefix(plugin_root) {
        if let Some(first) = relative.components().next() {
            if let Some(name) = first.as_os_str().to_str() {
                if prefixes.contains(&name) {
                    return true;
                }
            }
        }

        if let Some(ext) = relative.extension().and_then(|s| s.to_str()) {
            return ext == "Build.cs";
        }

        if let Some(os_str) = relative.as_os_str().to_str() {
            return os_str.ends_with(".uplugin");
        }
    }

    false
}
