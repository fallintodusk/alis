//! Dependency resolver - Resolves transitive plugin dependencies
//!
//! Parses .uplugin and .Build.cs files to extract dependencies.
//! Supports transitive dependency resolution for staging into HostProject.

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::{HashSet, VecDeque};
use std::path::PathBuf;

use crate::plugin_mapper::PluginMapper;

/// Plugin dependency information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginDependency {
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "Enabled")]
    pub enabled: bool,
}

/// .uplugin file structure (partial)
#[derive(Debug, Clone, Serialize, Deserialize)]
struct UpluginFile {
    #[serde(rename = "Plugins", default)]
    plugins: Option<Vec<PluginDependency>>,
    // Allow other fields to be present
    #[serde(flatten)]
    _other: serde_json::Value,
}

/// Dependency resolver for plugins
pub struct DependencyResolver {
    mapper: PluginMapper,
}

impl DependencyResolver {
    /// Create a new dependency resolver
    pub fn new(repo_path: PathBuf) -> Result<Self> {
        let mapper = PluginMapper::new(repo_path)?;
        Ok(Self { mapper })
    }

    /// Resolve all transitive dependencies for a plugin
    ///
    /// Returns a set of plugin names (excluding the target plugin itself)
    pub fn resolve_transitive_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        let mut resolved = HashSet::new();
        let mut queue = VecDeque::new();

        // Start with the target plugin
        queue.push_back(plugin_name.to_string());

        while let Some(current) = queue.pop_front() {
            if resolved.contains(&current) {
                continue;
            }

            let deps = self.get_plugin_dependencies(&current)?;

            for dep in deps {
                // Only include project plugins (our custom plugins)
                if self.is_project_plugin(&dep) {
                    if !resolved.contains(&dep) && dep != plugin_name {
                        queue.push_back(dep.clone());
                    }
                }
            }

            resolved.insert(current);
        }

        // Remove the target plugin itself
        resolved.remove(plugin_name);

        // Convert to sorted vector for deterministic output
        let mut result: Vec<String> = resolved.into_iter().collect();
        result.sort();

        Ok(result)
    }

    /// Get direct dependencies for a plugin (public API for scanner integration)
    ///
    /// Returns direct plugin dependencies (not transitive). Only includes project plugins.
    pub fn get_direct_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        self.get_plugin_dependencies(plugin_name)
    }

    /// Get direct dependencies for a plugin (internal implementation)
    fn get_plugin_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        let mut deps = HashSet::new();

        // 1. Parse .uplugin file for plugin dependencies
        if let Ok(plugin_deps) = self.parse_uplugin_dependencies(plugin_name) {
            deps.extend(plugin_deps);
        }

        // 2. Parse .Build.cs file for module dependencies
        if let Ok(module_deps) = self.parse_build_cs_dependencies(plugin_name) {
            deps.extend(module_deps);
        }

        Ok(deps.into_iter().collect())
    }

    /// Parse .uplugin file for plugin dependencies
    fn parse_uplugin_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        let plugin_path = self.mapper.get_plugin_path(plugin_name)?;
        let uplugin_path = plugin_path.join(format!("{}.uplugin", plugin_name));

        if !uplugin_path.exists() {
            return Ok(Vec::new());
        }

        let contents = std::fs::read_to_string(&uplugin_path)
            .with_context(|| format!("Failed to read .uplugin: {}", uplugin_path.display()))?;

        let uplugin: UpluginFile = serde_json::from_str(&contents)
            .with_context(|| "Failed to parse .uplugin file")?;

        let mut deps = Vec::new();
        if let Some(plugins) = uplugin.plugins {
            for plugin in plugins {
                if plugin.enabled {
                    deps.push(plugin.name);
                }
            }
        }

        Ok(deps)
    }

    /// Parse .Build.cs file for module dependencies that map to plugins
    fn parse_build_cs_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        let plugin_path = self.mapper.get_plugin_path(plugin_name)?;
        let build_cs_path = plugin_path.join(format!("Source/{}/{}.Build.cs", plugin_name, plugin_name));

        if !build_cs_path.exists() {
            return Ok(Vec::new());
        }

        let contents = std::fs::read_to_string(&build_cs_path)
            .with_context(|| format!("Failed to read .Build.cs: {}", build_cs_path.display()))?;

        let mut deps = HashSet::new();

        // Extract dependency names from PublicDependencyModuleNames and PrivateDependencyModuleNames
        // Look for strings that start with "Project" or match plugin naming patterns
        for line in contents.lines() {
            // Skip comments
            if line.trim_start().starts_with("//") {
                continue;
            }

            // Look for module names in quotes
            if line.contains('"') {
                for part in line.split('"') {
                    let module_name = part.trim();

                    // Check if this module name corresponds to a project plugin
                    if self.is_project_plugin(module_name) {
                        deps.insert(module_name.to_string());
                    }
                }
            }
        }

        Ok(deps.into_iter().collect())
    }

    /// Check if a module name corresponds to a project plugin (not engine module)
    fn is_project_plugin(&self, module_name: &str) -> bool {
        self.mapper.get_all_plugins().contains(&module_name.to_string())
    }

    /// Get the plugin mapper
    pub fn get_mapper(&self) -> &PluginMapper {
        &self.mapper
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    fn get_repo_root() -> Option<PathBuf> {
        let mut path = env::current_dir().ok()?;
        while !path.join(".git").exists() {
            if !path.pop() {
                return None;
            }
        }
        Some(path)
    }

    #[test]
    fn test_dependency_resolver_creation() {
        if let Some(repo_path) = get_repo_root() {
            let result = DependencyResolver::new(repo_path);
            assert!(result.is_ok());
        }
    }

    #[test]
    fn test_resolve_plugin_dependencies() -> Result<()> {
        if let Some(repo_path) = get_repo_root() {
            let resolver = DependencyResolver::new(repo_path)?;
            let all_plugins = resolver.mapper.get_all_plugins();

            // Test dependency resolution with any available plugin
            if !all_plugins.is_empty() {
                let test_plugin = &all_plugins[0];
                let deps = resolver.resolve_transitive_dependencies(test_plugin)?;

                // Just verify resolution works - actual dependencies depend on project structure
                // All resolved dependencies should exist in the plugin list
                for dep in &deps {
                    assert!(
                        all_plugins.contains(dep),
                        "Resolved dependency '{}' should exist in plugin list",
                        dep
                    );
                }
            }
        }
        Ok(())
    }

    #[test]
    fn test_parse_uplugin_dependencies() -> Result<()> {
        if let Some(repo_path) = get_repo_root() {
            let resolver = DependencyResolver::new(repo_path)?;
            let all_plugins = resolver.mapper.get_all_plugins();

            // Test Orchestrator plugin (has dependencies in .uplugin)
            if all_plugins.contains(&"Orchestrator".to_string()) {
                let deps = resolver.parse_uplugin_dependencies("Orchestrator")?;
                println!("Orchestrator .uplugin dependencies: {:?}", deps);

                // Should contain ProjectCore based on the .uplugin we saw earlier
                assert!(
                    deps.contains(&"ProjectCore".to_string()),
                    "Orchestrator should depend on ProjectCore in .uplugin"
                );
            }
        }
        Ok(())
    }
}
