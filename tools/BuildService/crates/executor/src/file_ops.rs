// File system operations for the build executor
//
// Provides utility functions for finding files and directories in the project structure

use anyhow::{Context, Result};
use std::path::{Path, PathBuf};

/// Find the .uproject file in the project directory
///
/// # Arguments
/// * `project_path` - Root path of the UE project
///
/// # Returns
/// * Ok(PathBuf) - Absolute path to the .uproject file
/// * Err(...) - If no .uproject file is found
pub fn find_uproject_file(project_path: &Path) -> Result<PathBuf> {
    for entry in std::fs::read_dir(project_path)
        .with_context(|| format!("Failed to read project directory: {}", project_path.display()))?
    {
        let entry = entry?;
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) == Some("uproject") {
            return Ok(path);
        }
    }

    Err(anyhow::anyhow!(
        "No .uproject file found in: {}",
        project_path.display()
    ))
}

/// Find a plugin's Content directory
///
/// Searches across multiple plugin categories to locate the plugin's Content folder.
/// This is needed for the -CookDir parameter to force cooking of unreferenced content.
///
/// # Arguments
/// * `project_path` - Root path of the UE project
/// * `plugin_name` - Name of the plugin to find
///
/// # Returns
/// * Ok(String) - Relative path from project root (e.g., "Plugins/UI/ProjectMenuMain/Content")
/// * Err(...) - If plugin Content directory not found
pub fn find_plugin_content_dir(project_path: &Path, plugin_name: &str) -> Result<String> {
    // Plugin categories to search (covers ALIS project structure)
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
        let content_path = project_path
            .join("Plugins")
            .join(category)
            .join(plugin_name)
            .join("Content");

        if content_path.exists() {
            let relative = format!("Plugins/{}/{}/Content", category, plugin_name);
            tracing::debug!("Found plugin Content directory: {}", relative);
            return Ok(relative);
        }
    }

    Err(anyhow::anyhow!(
        "Plugin '{}' Content directory not found in any category\n\
        \n\
        Searched categories: {:?}\n\
        Project path: {}\n\
        \n\
        Verify plugin exists and has Content/ folder.",
        plugin_name,
        categories,
        project_path.display()
    ))
}

/// Find repository root by looking for .git directory
///
/// Walks up the directory tree from `start_path` until it finds a .git directory.
///
/// # Arguments
/// * `start_path` - Starting path to search from
///
/// # Returns
/// * Some(PathBuf) - Absolute path to repository root
/// * None - If no .git directory found (not in a git repo)
pub fn find_repo_root(start_path: &Path) -> Option<PathBuf> {
    let mut current = start_path.to_path_buf();
    while !current.join(".git").exists() {
        if !current.pop() {
            return None;
        }
    }
    Some(current)
}
