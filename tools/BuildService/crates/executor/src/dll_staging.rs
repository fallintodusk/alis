// DLL staging verification and fallback copy
//
// CRITICAL: Ensures plugin DLLs are staged before packaging into code.zip
//
// UE's BuildCookRun with -skipbuild doesn't always stage binaries reliably.
// This module verifies staged DLLs exist and copies them from source if missing.

use anyhow::{Context, Result};
use std::path::{Path, PathBuf};

/// Verify plugin DLLs are staged, copy from source if missing
///
/// # Problem
/// When running incremental builds followed by DLC cook with `-skipbuild`, UE may
/// not stage plugin binaries properly. This leaves the staged Binaries/ directory
/// empty, resulting in code.zip without DLLs.
///
/// # Solution
/// 1. Check staged path: `Saved/StagedBuilds/Windows/<Plugin>/Binaries/Win64/*.dll`
/// 2. If empty, copy from source: `Plugins/<category>/<Plugin>/Binaries/Win64/*.dll`
/// 3. Verify at least one DLL was staged/copied (fail if zero)
///
/// # Arguments
/// * `plugin_name` - Plugin name (e.g., "ProjectMenuMain")
/// * `staged_root` - Root of staged build (e.g., "Saved/StagedBuilds/Windows/Alis")
/// * `project_path` - Project root to find source binaries
///
/// # Returns
/// * Ok(count) - Number of DLLs verified/copied
/// * Err(...) - If no DLLs found in source or copy failed
pub fn verify_and_stage_dlls(
    plugin_name: &str,
    staged_root: &Path,
    project_path: &Path,
) -> Result<usize> {
    // Staged binaries path (where DLLs should be after cook)
    let staged_binaries = staged_root
        .join(plugin_name)
        .join("Binaries")
        .join("Win64");

    // Check if staged binaries exist
    let mut staged_dlls = Vec::new();
    if staged_binaries.exists() {
        for entry in std::fs::read_dir(&staged_binaries)
            .with_context(|| format!("Failed to read staged binaries: {}", staged_binaries.display()))?
        {
            let entry = entry?;
            let path = entry.path();
            if path.extension().and_then(|s| s.to_str()) == Some("dll") {
                staged_dlls.push(path);
            }
        }
    }

    // If DLLs already staged, we're done
    if !staged_dlls.is_empty() {
        tracing::info!(
            "[OK] DLL staging verified: {} (found {} DLLs in staged path)",
            plugin_name,
            staged_dlls.len()
        );
        return Ok(staged_dlls.len());
    }

    // DLLs not staged - find source binaries and copy
    tracing::warn!(
        "DLL staging issue: {} binaries not in staged path, searching source...",
        plugin_name
    );

    let source_binaries = find_plugin_binaries(project_path, plugin_name)?;

    if !source_binaries.exists() {
        return Err(anyhow::anyhow!(
            "Plugin binaries not found for {}\n\
            \n\
            Staged path: {} (empty)\n\
            Source path: {} (doesn't exist)\n\
            \n\
            TROUBLESHOOTING:\n\
            - Verify incremental build ran successfully\n\
            - Check that plugin module compiled (check logs)\n\
            - Ensure LinkType = Modular in .Target.cs\n\
            - Look for linker errors in build log",
            plugin_name,
            staged_binaries.display(),
            source_binaries.display()
        ));
    }

    // Copy DLLs from source to staged
    std::fs::create_dir_all(&staged_binaries)
        .with_context(|| format!("Failed to create staged binaries dir: {}", staged_binaries.display()))?;

    let mut copied_count = 0;
    for entry in std::fs::read_dir(&source_binaries)
        .with_context(|| format!("Failed to read source binaries: {}", source_binaries.display()))?
    {
        let entry = entry?;
        let path = entry.path();

        // Copy DLLs, PDBs, and .modules files
        let ext = path.extension().and_then(|s| s.to_str());
        if matches!(ext, Some("dll") | Some("pdb") | Some("modules")) {
            let dest = staged_binaries.join(entry.file_name());
            std::fs::copy(&path, &dest)
                .with_context(|| format!("Failed to copy {} -> {}", path.display(), dest.display()))?;

            if ext == Some("dll") {
                copied_count += 1;
                tracing::info!("  Copied: {} -> staged", entry.file_name().to_string_lossy());
            }
        }
    }

    if copied_count == 0 {
        return Err(anyhow::anyhow!(
            "No DLLs found in source binaries for {}\n\
            Source path: {}\n\
            \n\
            Plugin may not have compiled, or binaries are elsewhere.",
            plugin_name,
            source_binaries.display()
        ));
    }

    tracing::info!(
        "[OK] DLL staging fallback: {} ({} DLLs copied from source)",
        plugin_name,
        copied_count
    );

    Ok(copied_count)
}

/// Find plugin source binaries directory
///
/// Searches plugin categories to locate Binaries/Win64 directory
fn find_plugin_binaries(project_path: &Path, plugin_name: &str) -> Result<PathBuf> {
    let categories = [
        "UI",
        "World",
        "Gameplay",
        "Systems",
        "Boot",
        "Foundation",
        "GameFeatures",
    ];

    for category in &categories {
        let binaries_path = project_path
            .join("Plugins")
            .join(category)
            .join(plugin_name)
            .join("Binaries")
            .join("Win64");

        if binaries_path.exists() {
            tracing::debug!("Found plugin binaries: {}", binaries_path.display());
            return Ok(binaries_path);
        }
    }

    // Return path even if doesn't exist (caller will handle error)
    Ok(project_path
        .join("Plugins")
        .join(plugin_name)
        .join("Binaries")
        .join("Win64"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::TempDir;

    #[test]
    fn test_find_plugin_binaries() {
        let temp = TempDir::new().unwrap();
        let project_path = temp.path();

        // Create mock plugin structure
        let binaries_path = project_path
            .join("Plugins")
            .join("UI")
            .join("TestPlugin")
            .join("Binaries")
            .join("Win64");
        fs::create_dir_all(&binaries_path).unwrap();

        let result = find_plugin_binaries(project_path, "TestPlugin").unwrap();
        assert_eq!(result, binaries_path);
    }

    #[test]
    fn test_verify_staged_dlls() {
        let temp = TempDir::new().unwrap();
        let staged_root = temp.path().join("Staged");
        let project_path = temp.path().join("Project");

        // Create staged binaries with DLL
        let staged_binaries = staged_root.join("TestPlugin").join("Binaries").join("Win64");
        fs::create_dir_all(&staged_binaries).unwrap();
        fs::write(staged_binaries.join("Test.dll"), b"mock dll").unwrap();

        let result = verify_and_stage_dlls("TestPlugin", &staged_root, &project_path).unwrap();
        assert_eq!(result, 1);
    }
}
