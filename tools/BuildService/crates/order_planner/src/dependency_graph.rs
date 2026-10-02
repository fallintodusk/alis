//! Dependency graph construction from .uplugin files

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet, VecDeque};
use std::path::PathBuf;
use walkdir::WalkDir;

/// Plugin dependency information from .uplugin
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginDependency {
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "Enabled")]
    pub enabled: bool,
}

/// Plugin metadata from .uplugin file
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginMetadata {
    #[serde(rename = "FriendlyName")]
    pub friendly_name: String,
    #[serde(rename = "Version")]
    pub version: Option<i32>,
    #[serde(rename = "VersionName")]
    pub version_name: Option<String>,
    #[serde(rename = "Plugins")]
    pub plugins: Option<Vec<PluginDependency>>,
}

/// Dependency graph for plugins
pub struct DependencyGraph {
    /// Map of plugin name -> list of dependencies (plugin names)
    pub graph: HashMap<String, Vec<String>>,
}

impl DependencyGraph {
    /// Create a new dependency graph by scanning all .uplugin files
    pub fn new(repo_path: PathBuf) -> Result<Self> {
        let mut graph = HashMap::new();
        let plugins_dir = repo_path.join("Plugins");

        if !plugins_dir.exists() {
            tracing::warn!("Plugins directory not found: {}", plugins_dir.display());
            return Ok(Self { graph });
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
                    // Load plugin metadata
                    match Self::load_plugin_metadata(path) {
                        Ok(metadata) => {
                            // Extract enabled dependencies
                            let dependencies = metadata
                                .plugins
                                .unwrap_or_default()
                                .into_iter()
                                .filter(|dep| dep.enabled)
                                .map(|dep| dep.name)
                                .collect();

                            graph.insert(plugin_name.to_string(), dependencies);
                            tracing::debug!("Loaded dependencies for plugin: {}", plugin_name);
                        }
                        Err(e) => {
                            tracing::warn!("Failed to load plugin {}: {}", plugin_name, e);
                        }
                    }
                }
            }
        }

        tracing::info!("Built dependency graph with {} plugins", graph.len());

        Ok(Self { graph })
    }

    /// Load plugin metadata from .uplugin file
    fn load_plugin_metadata(uplugin_path: &std::path::Path) -> Result<PluginMetadata> {
        let contents = std::fs::read_to_string(uplugin_path)
            .context("Failed to read .uplugin file")?;

        let metadata: PluginMetadata = serde_json::from_str(&contents)
            .context("Failed to parse .uplugin file")?;

        Ok(metadata)
    }

    /// Build a subgraph containing only specified plugins and their dependencies
    pub fn build_subgraph(&self, plugins: &[String]) -> Result<HashMap<String, Vec<String>>> {
        let mut subgraph = HashMap::new();
        let mut to_visit: VecDeque<String> = plugins.iter().cloned().collect();
        let mut visited = HashSet::new();

        while let Some(plugin) = to_visit.pop_front() {
            if visited.contains(&plugin) {
                continue;
            }

            visited.insert(plugin.clone());

            if let Some(deps) = self.graph.get(&plugin) {
                // Add this plugin's dependencies to subgraph
                subgraph.insert(plugin.clone(), deps.clone());

                // Queue dependencies for visiting
                for dep in deps {
                    if !visited.contains(dep) {
                        to_visit.push_back(dep.clone());
                    }
                }
            } else {
                // Plugin has no dependencies (or doesn't exist in graph)
                subgraph.insert(plugin, Vec::new());
            }
        }

        Ok(subgraph)
    }

    /// Get all transitive dependencies for a plugin
    pub fn get_transitive_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        let mut all_deps = HashSet::new();
        let mut to_visit = VecDeque::new();
        to_visit.push_back(plugin_name.to_string());

        while let Some(current) = to_visit.pop_front() {
            if let Some(deps) = self.graph.get(&current) {
                for dep in deps {
                    if all_deps.insert(dep.clone()) {
                        to_visit.push_back(dep.clone());
                    }
                }
            }
        }

        Ok(all_deps.into_iter().collect())
    }

    /// Get all plugins in the graph
    pub fn get_all_plugins(&self) -> Vec<String> {
        self.graph.keys().cloned().collect()
    }

    /// Build reverse dependency index (dependents map)
    /// Maps each plugin to the list of plugins that depend on it
    pub fn build_reverse_index(&self) -> HashMap<String, Vec<String>> {
        let mut reverse: HashMap<String, Vec<String>> = HashMap::new();

        for (plugin, dependencies) in &self.graph {
            for dep in dependencies {
                reverse
                    .entry(dep.clone())
                    .or_insert_with(Vec::new)
                    .push(plugin.clone());
            }
        }

        reverse
    }

    /// Get transitive dependents for a plugin (BFS to include all reverse deps)
    /// Returns all plugins that transitively depend on the given plugin
    pub fn get_transitive_dependents(&self, plugin_name: &str) -> Result<Vec<String>> {
        let reverse_index = self.build_reverse_index();
        let mut all_dependents = HashSet::new();
        let mut to_visit = VecDeque::new();
        to_visit.push_back(plugin_name.to_string());

        while let Some(current) = to_visit.pop_front() {
            if let Some(dependents) = reverse_index.get(&current) {
                for dependent in dependents {
                    if all_dependents.insert(dependent.clone()) {
                        to_visit.push_back(dependent.clone());
                    }
                }
            }
        }

        // Remove the starting plugin if it was added (in case of circular refs)
        all_dependents.remove(plugin_name);

        Ok(all_dependents.into_iter().collect())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_dependency_graph_creation() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        let result = DependencyGraph::new(repo_path);
        assert!(result.is_ok());

        if let Ok(graph) = result {
            let plugins = graph.get_all_plugins();
            tracing::info!("Test found {} plugins", plugins.len());
        }
    }

    #[test]
    fn test_build_subgraph() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        if let Ok(graph) = DependencyGraph::new(repo_path) {
            let plugins = vec!["TestPlugin".to_string()];
            let result = graph.build_subgraph(&plugins);
            assert!(result.is_ok());
        }
    }
}
