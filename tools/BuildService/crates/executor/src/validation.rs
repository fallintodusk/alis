// DLC isolation validation
//
// Ensures DLC containers only contain plugin-scoped assets (no cross-contamination)
// Uses UnrealPak.exe -List to inspect .utoc files

use anyhow::{Context, Result};
use std::path::Path;

/// Validate DLC isolation for a plugin
///
/// Ensures the plugin's IoStore container only contains assets from the plugin directory.
/// This prevents cross-contamination where base game or other plugin assets leak into
/// the DLC package, which would cause packaging failures or runtime issues.
///
/// # Arguments
/// * `plugin_name` - Name of the plugin to validate
/// * `iostore_path` - Path to the IoStore container directory (e.g., Saved/StagedBuilds/.../Content/Paks)
/// * `unrealpak_exe` - Path to UnrealPak.exe
///
/// # Returns
/// * Ok(()) if validation passes (all files are plugin-scoped)
/// * Err(...) if validation fails or UnrealPak execution fails
pub fn validate_dlc_isolation(
    plugin_name: &str,
    iostore_path: &Path,
    unrealpak_exe: &Path,
) -> Result<()> {
    tracing::info!("Validating DLC isolation for plugin: {}", plugin_name);

    // Find the plugin's .utoc file
    let utoc_pattern = format!("{}-Windows.utoc", plugin_name);
    let utoc_file = iostore_path.join(&utoc_pattern);

    if !utoc_file.exists() {
        return Err(anyhow::anyhow!(
            "IoStore .utoc file not found: {}\n\
            \n\
            Expected file: {}\n\
            Directory contents:",
            utoc_file.display(),
            utoc_pattern
        ));
    }

    // Execute UnrealPak.exe -List to get container contents
    let output = std::process::Command::new(unrealpak_exe)
        .arg(&utoc_file)
        .arg("-List")
        .output()
        .with_context(|| format!("Failed to execute UnrealPak.exe: {}", unrealpak_exe.display()))?;

    if !output.status.success() {
        return Err(anyhow::anyhow!(
            "UnrealPak.exe -List failed for {}\nStderr: {}",
            utoc_file.display(),
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    // Parse output and validate all paths are plugin-scoped
    let stdout = String::from_utf8_lossy(&output.stdout);
    let expected_prefix = format!("/Plugins/{}/", plugin_name);

    let mut total_files = 0;
    let mut violations = Vec::new();

    for line in stdout.lines() {
        // UnrealPak -List output format: "  filename" (indented with spaces)
        let trimmed = line.trim();
        if trimmed.is_empty() || !trimmed.starts_with('/') {
            continue;
        }

        total_files += 1;

        // Check if path is plugin-scoped
        if !trimmed.starts_with(&expected_prefix) {
            violations.push(trimmed.to_string());
        }
    }

    if total_files == 0 {
        return Err(anyhow::anyhow!(
            "DLC container is empty or UnrealPak output parsing failed\n\
            \n\
            Container: {}\n\
            \n\
            TROUBLESHOOTING:\n\
            - Verify DLC cooking completed successfully\n\
            - Check that -CookDir parameter was used (forces unreferenced content cooking)\n\
            - Manually run: {} {} -List",
            utoc_file.display(),
            unrealpak_exe.display(),
            utoc_file.display()
        ));
    }

    if !violations.is_empty() {
        return Err(anyhow::anyhow!(
            "DLC isolation violation: {} contains non-plugin assets\n\
            \n\
            Expected all paths to start with: {}\n\
            Found {} violations (showing first 10):\n{}\n\
            \n\
            CRITICAL: DLC must only contain plugin assets.\n\
            This usually indicates:\n\
            - Base game assets leaked into DLC\n\
            - Plugin references assets from other plugins\n\
            - .uproject sanitization failed\n\
            \n\
            See: docs/architecture/dlc_workflow.md",
            plugin_name,
            expected_prefix,
            violations.len(),
            violations.iter().take(10).map(|v| format!("  - {}", v)).collect::<Vec<_>>().join("\n")
        ));
    }

    tracing::info!(
        "[OK] DLC isolation validated: {} (all {} files are plugin-scoped)",
        plugin_name,
        total_files
    );

    Ok(())
}
