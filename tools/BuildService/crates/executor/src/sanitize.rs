// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
//
// .uproject Plugin Sanitization
//
// Strips non-essential plugins from .uproject before builds to enforce
// the Orchestrator pattern: only boot/core plugins enabled at build time,
// all others load dynamically at runtime.
//
// Context: Developer workflow enables all plugins for Content Browser visibility,
// but builds must use only core plugins to generate proper per-plugin DLC containers.

use anyhow::{Context, Result};
use serde_json::Value;
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};
use tracing::{debug, info, warn};
use walkdir::WalkDir;

/// Project plugins that MUST remain enabled during builds
///
/// These are the only project plugins allowed in sanitized .uproject.
/// All other plugins from Plugins/ directory will be stripped and loaded via Orchestrator.
///
/// Boot plugins:
/// - Orchestrator: Runtime plugin loader
///
/// Temporary marketplace dependencies (will be moved to external):
/// - DialoguePlugin: Dialogue system (marketplace)
/// - InstanceArrayTool: Instancing tools (marketplace)
/// - LowEntryExtStdLib: Extended standard library (marketplace)
const ALLOWED_PROJECT_PLUGINS: &[&str] = &[
    "Orchestrator",
    "DialoguePlugin",
    "InstanceArrayTool",
    "LowEntryExtStdLib",
];

/// Discover all project plugins by scanning Plugins/ directory
///
/// Returns set of plugin names found in project's Plugins/ folder.
/// These are plugins managed by our project, as opposed to engine/external plugins.
fn discover_project_plugins(project_root: &Path) -> Result<HashSet<String>> {
    let plugins_dir = project_root.join("Plugins");

    if !plugins_dir.exists() {
        return Ok(HashSet::new());
    }

    let mut project_plugins = HashSet::new();

    for entry in WalkDir::new(&plugins_dir)
        .follow_links(false)
        .into_iter()
        .filter_map(|e| e.ok())
    {
        let path = entry.path();

        // Look for .uplugin files
        if path.extension().and_then(|s| s.to_str()) == Some("uplugin") {
            if let Some(plugin_name) = path.file_stem().and_then(|s| s.to_str()) {
                debug!("Discovered project plugin: {}", plugin_name);
                project_plugins.insert(plugin_name.to_string());
            }
        }
    }

    info!(
        "Discovered {} project plugins in {}",
        project_plugins.len(),
        plugins_dir.display()
    );

    Ok(project_plugins)
}

/// Sanitize .uproject by removing project plugins (except allowed ones)
///
/// **Auto-discovery approach:**
/// 1. Scans `Plugins/` directory for all `.uplugin` files
/// 2. Creates exclusion list: discovered plugins MINUS allowed ones
/// 3. Removes exclusion list from .uproject
/// 4. Keeps all engine/marketplace plugins NOT in `Plugins/` folder
///
/// This approach scales automatically - no hardcoded lists to maintain.
///
/// **IMPORTANT**: Creates NEW file at `output_path`, never modifies original.
///
/// # Arguments
/// * `project_root` - Path to project root (contains Plugins/ and .uproject)
/// * `uproject_path` - Path to original .uproject file
/// * `output_path` - Path where sanitized .uproject will be written
///
/// # Returns
/// * `Ok(Vec<String>)` - Names of project plugins that were stripped
/// * `Err(_)` - If file I/O or JSON parsing fails
pub fn sanitize_uproject(
    project_root: &Path,
    uproject_path: &Path,
    output_path: &Path,
) -> Result<Vec<String>> {
    info!(
        "Sanitizing .uproject: {} -> {}",
        uproject_path.display(),
        output_path.display()
    );

    // Discover all project plugins
    let project_plugins = discover_project_plugins(project_root)?;

    // Create exclusion set: project plugins EXCEPT allowed ones
    let to_exclude: HashSet<String> = project_plugins
        .iter()
        .filter(|name| !ALLOWED_PROJECT_PLUGINS.contains(&name.as_str()))
        .cloned()
        .collect();

    info!(
        "Exclusion list ({} plugins): {:?}",
        to_exclude.len(),
        to_exclude
            .iter()
            .collect::<Vec<_>>()
    );

    // Read and parse .uproject
    let content = fs::read_to_string(uproject_path)
        .with_context(|| format!("Failed to read .uproject: {}", uproject_path.display()))?;

    let mut json: Value = serde_json::from_str(&content)
        .with_context(|| format!("Failed to parse .uproject JSON: {}", uproject_path.display()))?;

    // Track stripped plugins for return
    let mut stripped_plugins = Vec::new();

    // Filter plugins array
    if let Some(plugins) = json.get_mut("Plugins").and_then(|p| p.as_array_mut()) {
        let original_count = plugins.len();

        plugins.retain(|plugin| {
            if let Some(name) = plugin.get("Name").and_then(|n| n.as_str()) {
                // Remove if in exclusion list
                if to_exclude.contains(name) {
                    debug!("Stripping project plugin: {}", name);
                    stripped_plugins.push(name.to_string());
                    return false;
                }

                // Keep all others (allowed project plugins + engine/marketplace)
                true
            } else {
                // Keep plugins without name field (malformed, let UE handle it)
                warn!("Found plugin entry without 'Name' field, keeping it");
                true
            }
        });

        let final_count = plugins.len();
        info!(
            "Plugin sanitization: {} total -> {} kept ({} stripped project plugins)",
            original_count,
            final_count,
            stripped_plugins.len()
        );
    } else {
        warn!("No 'Plugins' array found in .uproject, nothing to sanitize");
    }

    // Write sanitized version
    let output = serde_json::to_string_pretty(&json)
        .context("Failed to serialize sanitized .uproject JSON")?;

    fs::write(output_path, output)
        .with_context(|| format!("Failed to write sanitized .uproject: {}", output_path.display()))?;

    info!(
        "Sanitization complete: {} project plugins stripped",
        stripped_plugins.len()
    );

    Ok(stripped_plugins)
}

/// Validate .uproject against project plugin discovery
///
/// Checks if any non-allowed project plugins are enabled.
///
/// # Arguments
/// * `project_root` - Path to project root
/// * `uproject_path` - Path to .uproject file
///
/// # Returns
/// * `Ok(Vec<String>)` - Names of disallowed project plugins found (empty = valid)
pub fn validate_uproject(project_root: &Path, uproject_path: &Path) -> Result<Vec<String>> {
    let content = fs::read_to_string(uproject_path)
        .with_context(|| format!("Failed to read .uproject: {}", uproject_path.display()))?;

    let json: Value = serde_json::from_str(&content)
        .with_context(|| format!("Failed to parse .uproject JSON: {}", uproject_path.display()))?;

    // Discover project plugins
    let project_plugins = discover_project_plugins(project_root)?;

    let mut disallowed = Vec::new();

    if let Some(plugins) = json.get("Plugins").and_then(|p| p.as_array()) {
        for plugin in plugins {
            if let Some(name) = plugin.get("Name").and_then(|n| n.as_str()) {
                let is_enabled = plugin
                    .get("Enabled")
                    .and_then(|e| e.as_bool())
                    .unwrap_or(false);

                // Check if it's a project plugin and enabled but not allowed
                if is_enabled
                    && project_plugins.contains(name)
                    && !ALLOWED_PROJECT_PLUGINS.contains(&name)
                {
                    disallowed.push(name.to_string());
                }
            }
        }
    }

    if !disallowed.is_empty() {
        warn!(
            "Found {} non-allowed project plugins enabled: {:?}",
            disallowed.len(),
            disallowed
        );
    }

    Ok(disallowed)
}

/// Helper for build executor: sanitize -> run build -> cleanup
///
/// Wraps the sanitization workflow with automatic cleanup via RAII.
/// The temp file is automatically removed when SanitizedProject is dropped,
/// even if the build fails.
///
/// # Example
/// ```ignore
/// use build_service_executor::sanitize::SanitizedProject;
/// use std::path::Path;
///
/// let original = Path::new("Alis.uproject");
///
/// // Create sanitized temp (RAII guard)
/// let sanitized = SanitizedProject::create(original)?;
///
/// // Use temp file for build
/// run_build_with(sanitized.path())?;
///
/// // Temp file automatically deleted when 'sanitized' goes out of scope
/// # Ok::<(), anyhow::Error>(())
/// ```
pub struct SanitizedProject {
    temp_path: PathBuf,
    stripped_plugins: Vec<String>,
}

impl SanitizedProject {
    /// Create sanitized temp .uproject from original
    ///
    /// The temp file will be placed in the same directory as the original,
    /// with suffix "_sanitized.uproject".
    ///
    /// # Arguments
    /// * `project_root` - Path to project root (contains Plugins/)
    /// * `original_path` - Path to original .uproject file
    pub fn create(project_root: &Path, original_path: &Path) -> Result<Self> {
        let parent = original_path.parent()
            .context("Failed to get parent directory of .uproject")?;

        let temp_path = parent.join(format!(
            "{}_sanitized.uproject",
            original_path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("Alis")
        ));

        let stripped_plugins = sanitize_uproject(project_root, original_path, &temp_path)?;

        Ok(Self {
            temp_path,
            stripped_plugins,
        })
    }

    /// Get path to sanitized temp file
    pub fn path(&self) -> &Path {
        &self.temp_path
    }

    /// Get list of plugins that were stripped
    pub fn stripped_plugins(&self) -> &[String] {
        &self.stripped_plugins
    }
}

impl Drop for SanitizedProject {
    fn drop(&mut self) {
        // Cleanup temp file (best-effort, don't panic)
        if self.temp_path.exists() {
            if let Err(e) = std::fs::remove_file(&self.temp_path) {
                tracing::warn!(
                    "Failed to cleanup temp .uproject {}: {}",
                    self.temp_path.display(),
                    e
                );
            } else {
                tracing::debug!(
                    "Cleaned up temp .uproject: {}",
                    self.temp_path.display()
                );
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn test_sanitize_removes_feature_plugins() {
        let temp = TempDir::new().unwrap();
        let project_root = temp.path();
        let input = temp.path().join("test.uproject");
        let output = temp.path().join("test_sanitized.uproject");

        // Create Plugins/ directory with test plugins
        let plugins_dir = project_root.join("Plugins");
        fs::create_dir_all(&plugins_dir).unwrap();

        // Create .uplugin files for project plugins
        fs::write(plugins_dir.join("ProjectCore.uplugin"), "{}").unwrap();
        fs::write(plugins_dir.join("ProjectInventory.uplugin"), "{}").unwrap();
        fs::write(plugins_dir.join("ProjectCombat.uplugin"), "{}").unwrap();

        // Create test .uproject with mix of allowed and disallowed plugins
        let test_content = r#"{
            "FileVersion": 3,
            "Plugins": [
                {"Name": "Orchestrator", "Enabled": true},
                {"Name": "ProjectCore", "Enabled": true},
                {"Name": "ProjectInventory", "Enabled": true},
                {"Name": "ProjectCombat", "Enabled": true},
                {"Name": "LowEntryExtStdLib", "Enabled": true}
            ]
        }"#;

        fs::write(&input, test_content).unwrap();

        // Sanitize
        let stripped = sanitize_uproject(project_root, &input, &output).unwrap();

        // Verify stripped plugins - all 3 project plugins should be stripped
        assert_eq!(stripped.len(), 3);
        assert!(stripped.contains(&"ProjectCore".to_string()));
        assert!(stripped.contains(&"ProjectInventory".to_string()));
        assert!(stripped.contains(&"ProjectCombat".to_string()));

        // Verify output file
        let result_content = fs::read_to_string(&output).unwrap();
        let result_json: Value = serde_json::from_str(&result_content).unwrap();

        let result_plugins = result_json["Plugins"].as_array().unwrap();
        assert_eq!(result_plugins.len(), 2); // Only Orchestrator, LowEntryExtStdLib (allowed + marketplace)

        // Verify allowed plugins remain
        let plugin_names: Vec<&str> = result_plugins
            .iter()
            .filter_map(|p| p["Name"].as_str())
            .collect();

        assert!(plugin_names.contains(&"Orchestrator"));
        assert!(plugin_names.contains(&"LowEntryExtStdLib"));
        assert!(!plugin_names.contains(&"ProjectCore"));
        assert!(!plugin_names.contains(&"ProjectInventory"));
        assert!(!plugin_names.contains(&"ProjectCombat"));
    }

    #[test]
    fn test_sanitized_project_raii_cleanup() {
        let temp = TempDir::new().unwrap();
        let project_root = temp.path();
        let test_file = temp.path().join("test.uproject");

        // Create Plugins/ directory with test plugin
        let plugins_dir = project_root.join("Plugins");
        fs::create_dir_all(&plugins_dir).unwrap();
        fs::write(plugins_dir.join("ProjectInventory.uplugin"), "{}").unwrap();

        let test_content = r#"{
            "FileVersion": 3,
            "Plugins": [
                {"Name": "Orchestrator", "Enabled": true},
                {"Name": "ProjectInventory", "Enabled": true}
            ]
        }"#;

        fs::write(&test_file, test_content).unwrap();

        let sanitized_path;

        {
            // Create sanitized project (RAII)
            let sanitized = SanitizedProject::create(project_root, &test_file).unwrap();
            sanitized_path = sanitized.path().to_path_buf();

            // Verify temp file exists
            assert!(sanitized_path.exists());
            assert_eq!(sanitized.stripped_plugins().len(), 1);
            assert!(sanitized.stripped_plugins().contains(&"ProjectInventory".to_string()));

            // Verify temp file is sanitized
            let content = fs::read_to_string(&sanitized_path).unwrap();
            let json: Value = serde_json::from_str(&content).unwrap();
            let plugins = json["Plugins"].as_array().unwrap();
            assert_eq!(plugins.len(), 1); // Only Orchestrator
        }

        // After drop, temp file should be cleaned up
        assert!(!sanitized_path.exists());

        // Original file should still exist
        assert!(test_file.exists());
    }

    #[test]
    fn test_validate_detects_disallowed_plugins() {
        let temp = TempDir::new().unwrap();
        let project_root = temp.path();
        let test_file = temp.path().join("test.uproject");

        // Create Plugins/ directory with test plugins
        let plugins_dir = project_root.join("Plugins");
        fs::create_dir_all(&plugins_dir).unwrap();
        fs::write(plugins_dir.join("ProjectInventory.uplugin"), "{}").unwrap();
        fs::write(plugins_dir.join("ProjectCombat.uplugin"), "{}").unwrap();

        let test_content = r#"{
            "FileVersion": 3,
            "Plugins": [
                {"Name": "Orchestrator", "Enabled": true},
                {"Name": "ProjectInventory", "Enabled": true},
                {"Name": "ProjectCombat", "Enabled": false}
            ]
        }"#;

        fs::write(&test_file, test_content).unwrap();

        let disallowed = validate_uproject(project_root, &test_file).unwrap();

        // Only enabled disallowed plugins should be reported
        assert_eq!(disallowed.len(), 1);
        assert!(disallowed.contains(&"ProjectInventory".to_string()));
        assert!(!disallowed.contains(&"ProjectCombat".to_string())); // Disabled, so OK
    }
}
