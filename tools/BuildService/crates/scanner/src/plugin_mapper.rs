//! Plugin mapper - Maps file paths to plugin names

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::PathBuf;
use walkdir::WalkDir;

/// Plugin metadata from .uplugin file
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginMetadata {
    #[serde(rename = "FriendlyName")]
    pub friendly_name: String,
    #[serde(rename = "Version")]
    pub version: Option<i32>,
    #[serde(rename = "VersionName")]
    pub version_name: Option<String>,
}

/// PluginMapper - Maps files to plugins
pub struct PluginMapper {
    repo_path: PathBuf,
    plugin_paths: HashMap<String, PathBuf>,
}

impl PluginMapper {
    /// Create a new PluginMapper
    pub fn new(repo_path: PathBuf) -> Result<Self> {
        let mut mapper = Self {
            repo_path,
            plugin_paths: HashMap::new(),
        };

        mapper.discover_plugins()?;

        Ok(mapper)
    }

    /// Discover all plugins in the repository
    fn discover_plugins(&mut self) -> Result<()> {
        let plugins_dir = self.repo_path.join("Plugins");

        if !plugins_dir.exists() {
            tracing::warn!("Plugins directory not found: {}", plugins_dir.display());
            return Ok(());
        }

        // Find all .uplugin files
        for entry in WalkDir::new(&plugins_dir)
            .follow_links(false)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.extension().and_then(|s| s.to_str()) == Some("uplugin") {
                if let Some(plugin_name) = path.file_stem().and_then(|s| s.to_str()) {
                    if let Some(plugin_dir) = path.parent() {
                        self.plugin_paths.insert(
                            plugin_name.to_string(),
                            plugin_dir.to_path_buf(),
                        );
                        tracing::debug!("Discovered plugin: {}", plugin_name);
                    }
                }
            }
        }

        tracing::info!("Discovered {} plugins", self.plugin_paths.len());

        Ok(())
    }

    /// Map a file path to its plugin name
    pub fn file_to_plugin(&self, file_path: &str) -> Result<Option<String>> {
        // Normalize file path
        let file_path_buf = PathBuf::from(file_path);

        // Check if file is under any plugin directory
        for (plugin_name, plugin_path) in &self.plugin_paths {
            // Make plugin_path relative to repo root for comparison
            if let Ok(relative_plugin_path) = plugin_path.strip_prefix(&self.repo_path) {
                if file_path_buf.starts_with(relative_plugin_path) {
                    return Ok(Some(plugin_name.clone()));
                }
            }

            // Also try absolute path comparison
            if file_path_buf.starts_with(plugin_path) {
                return Ok(Some(plugin_name.clone()));
            }
        }

        // File is not part of any plugin
        Ok(None)
    }

    /// Get the path to a specific plugin
    pub fn get_plugin_path(&self, plugin_name: &str) -> Result<PathBuf> {
        self.plugin_paths
            .get(plugin_name)
            .cloned()
            .ok_or_else(|| anyhow::anyhow!("Plugin not found: {}", plugin_name))
    }

    /// Get all discovered plugin names
    pub fn get_all_plugins(&self) -> Vec<String> {
        self.plugin_paths.keys().cloned().collect()
    }

    /// Get repository root path
    pub fn repo_root(&self) -> &PathBuf {
        &self.repo_path
    }

    /// Load plugin metadata from .uplugin file
    pub fn load_plugin_metadata(&self, plugin_name: &str) -> Result<PluginMetadata> {
        let plugin_path = self.get_plugin_path(plugin_name)?;
        let uplugin_path = plugin_path.join(format!("{}.uplugin", plugin_name));

        let contents = std::fs::read_to_string(&uplugin_path)
            .context(format!("Failed to read .uplugin file: {}", uplugin_path.display()))?;

        let metadata: PluginMetadata = serde_json::from_str(&contents)
            .context("Failed to parse .uplugin file")?;

        Ok(metadata)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_plugin_mapper_creation() {
        let current_dir = env::current_dir().unwrap();

        // Try to find repo root
        let mut repo_path = current_dir.clone();
        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return; // Can't find repo, skip test
            }
        }

        let result = PluginMapper::new(repo_path);
        assert!(result.is_ok());

        if let Ok(mapper) = result {
            let plugins = mapper.get_all_plugins();
            tracing::info!("Found {} plugins in test", plugins.len());
        }
    }

    #[test]
    fn test_file_to_plugin_mapping() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        if let Ok(mapper) = PluginMapper::new(repo_path) {
            // Test with a hypothetical plugin file path
            let test_path = "Plugins/Boot/BootROM/Source/BootROM/Private/BootROM.cpp";
            let result = mapper.file_to_plugin(test_path);
            assert!(result.is_ok());
        }
    }
}
