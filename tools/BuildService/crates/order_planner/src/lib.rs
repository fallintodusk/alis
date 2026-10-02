//! OrderPlanner crate - Topological sort of changed plugins based on dependencies
//!
//! Consumed by: BuildCli
//! Consumes: .uplugin dependency graphs, manifest.json, optional BuildUnit.yaml
//!
//! Responsibilities:
//! - Parse .uplugin files to build dependency graph
//! - Perform topological sort of changed plugins
//! - Detect circular dependencies
//! - Honor BuildUnit.yaml overrides (rebuild_on_dependency_change, etc.)

mod dependency_graph;
mod topological_sort;

pub use dependency_graph::{DependencyGraph, PluginDependency, PluginMetadata};
pub use topological_sort::TopologicalSorter;

use anyhow::Result;
use std::path::PathBuf;

/// OrderPlanner configuration
#[derive(Debug, Clone)]
pub struct PlannerConfig {
    pub repo_path: PathBuf,
}

/// OrderPlanner - Sorts plugins in dependency order
pub struct OrderPlanner {
    config: PlannerConfig,
    dependency_graph: DependencyGraph,
}

impl OrderPlanner {
    /// Create a new OrderPlanner
    pub fn new(config: PlannerConfig) -> Result<Self> {
        let dependency_graph = DependencyGraph::new(config.repo_path.clone())?;

        Ok(Self {
            config,
            dependency_graph,
        })
    }

    /// Order plugins in dependency-safe build order
    ///
    /// Takes a list of changed plugins and returns them in an order where
    /// dependencies are built before dependents.
    ///
    /// NOTE: This only rebuilds the changed plugins themselves, not their dependents.
    /// Use `order_impacted()` if you need to rebuild dependents when a dependency changes.
    pub fn order(&self, changed_plugins: &[String]) -> Result<Vec<String>> {
        tracing::info!("Ordering {} changed plugins...", changed_plugins.len());

        if changed_plugins.is_empty() {
            return Ok(Vec::new());
        }

        // Build the subgraph containing only changed plugins and their dependencies
        let subgraph = self.dependency_graph.build_subgraph(changed_plugins)?;

        // Perform topological sort
        let sorter = TopologicalSorter::new();
        let ordered = sorter.sort(&subgraph)?;

        // Filter to only include plugins that were in the original changed list
        let filtered: Vec<String> = ordered
            .into_iter()
            .filter(|p| changed_plugins.contains(p))
            .collect();

        tracing::info!("Ordered {} plugins for building", filtered.len());

        Ok(filtered)
    }

    /// Order impacted plugins in dependency-safe build order
    ///
    /// Takes a list of changed plugins and expands the set to include all transitive
    /// dependents (plugins that depend on the changed plugins). Returns all impacted
    /// plugins in an order where dependencies are built before dependents.
    ///
    /// Use this when you want to rebuild affected plugins when a dependency changes.
    pub fn order_impacted(&self, changed_plugins: &[String]) -> Result<Vec<String>> {
        tracing::info!(
            "Computing impacted build order for {} changed plugins...",
            changed_plugins.len()
        );

        if changed_plugins.is_empty() {
            return Ok(Vec::new());
        }

        // Filter to only known plugins (graceful handling of renamed/removed plugins)
        let known_plugins: std::collections::HashSet<String> =
            self.dependency_graph.get_all_plugins().into_iter().collect();

        let valid_changed: Vec<String> = changed_plugins
            .iter()
            .filter(|p| {
                let exists = known_plugins.contains(*p);
                if !exists {
                    tracing::warn!(
                        "Plugin '{}' not found in dependency graph (renamed/removed?), skipping",
                        p
                    );
                }
                exists
            })
            .cloned()
            .collect();

        if valid_changed.is_empty() {
            tracing::warn!("No valid plugins in changed list after filtering");
            return Ok(Vec::new());
        }

        // Expand changed set to include all transitive dependents
        let mut impacted_set: std::collections::HashSet<String> =
            valid_changed.iter().cloned().collect();

        for plugin in &valid_changed {
            let dependents = self.dependency_graph.get_transitive_dependents(plugin)?;
            impacted_set.extend(dependents);
        }

        let impacted: Vec<String> = impacted_set.into_iter().collect();
        tracing::info!(
            "Expanded {} valid changed plugins to {} impacted plugins (including dependents)",
            valid_changed.len(),
            impacted.len()
        );

        // Build subgraph with all impacted plugins and their dependencies
        let subgraph = self.dependency_graph.build_subgraph(&impacted)?;

        // Perform topological sort
        let sorter = TopologicalSorter::new();
        let ordered = sorter.sort(&subgraph)?;

        // Filter to only include impacted plugins (not transitive dependencies)
        let filtered: Vec<String> = ordered
            .into_iter()
            .filter(|p| impacted.contains(p))
            .collect();

        tracing::info!(
            "Ordered {} impacted plugins for building (changed + dependents)",
            filtered.len()
        );

        Ok(filtered)
    }

    /// Get all dependencies for a plugin (transitive)
    pub fn get_all_dependencies(&self, plugin_name: &str) -> Result<Vec<String>> {
        self.dependency_graph.get_transitive_dependencies(plugin_name)
    }

    /// Get all dependents for a plugin (transitive)
    ///
    /// Returns all plugins that transitively depend on the given plugin.
    /// Useful for determining what needs to be rebuilt when a plugin changes.
    pub fn get_all_dependents(&self, plugin_name: &str) -> Result<Vec<String>> {
        self.dependency_graph.get_transitive_dependents(plugin_name)
    }

    /// Check if there are circular dependencies
    pub fn has_cycles(&self) -> Result<bool> {
        let sorter = TopologicalSorter::new();

        match sorter.sort(&self.dependency_graph.graph) {
            Ok(_) => Ok(false),
            Err(e) => {
                if e.to_string().contains("Circular dependency") {
                    Ok(true)
                } else {
                    Err(e)
                }
            }
        }
    }

    /// Reload dependency graph (useful after plugin changes)
    pub fn reload(&mut self) -> Result<()> {
        self.dependency_graph = DependencyGraph::new(self.config.repo_path.clone())?;
        tracing::info!("Dependency graph reloaded");
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_planner_creation() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        // Find repo root
        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return; // Skip test if not in repo
            }
        }

        let config = PlannerConfig {
            repo_path: repo_path.clone(),
        };

        let result = OrderPlanner::new(config);
        assert!(result.is_ok());
    }

    #[test]
    fn test_order_empty_list() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        let config = PlannerConfig { repo_path };

        if let Ok(planner) = OrderPlanner::new(config) {
            let result = planner.order(&[]);
            assert!(result.is_ok());
            assert_eq!(result.unwrap().len(), 0);
        }
    }

    #[test]
    fn test_order_single_plugin() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        let config = PlannerConfig { repo_path };

        if let Ok(planner) = OrderPlanner::new(config) {
            let changed = vec!["TestPlugin".to_string()];
            let result = planner.order(&changed);
            // May fail if TestPlugin doesn't exist, which is ok for this test
            if let Ok(ordered) = result {
                assert!(ordered.len() <= 1);
            }
        }
    }
}
