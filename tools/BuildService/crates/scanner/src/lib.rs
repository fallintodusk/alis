//! ChangeScanner crate - Detects changed plugins via BuildUnit descriptors
//!
//! Consumed by: BuildCli
//! Consumes: Plugin repository + BuildUnit.yaml descriptors

mod build_unit;
mod build_unit_builder;
mod dependency_resolver;
mod hashing;
mod plugin_mapper;
mod project;

pub use build_unit::{BuildUnitDescriptor, DescriptorHashes};
pub use build_unit_builder::BuildUnitBuilder;
pub use dependency_resolver::DependencyResolver;
pub use plugin_mapper::PluginMapper;

use crate::hashing::{compute_code_hash, compute_content_hash};
use crate::project::detect_project;
use anyhow::Result;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

/// Set of changed plugins
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChangedSet {
    pub plugins: Vec<String>,
}

/// Snapshot of all scanned units for state comparison
///
/// Consumed by: build_service_state::mark_dirty_from_scan()
#[derive(Debug, Clone)]
pub struct ScanSnapshot {
    pub units: BTreeMap<String, ScannedUnit>,
}

/// Single scanned unit (plugin) with hashes and dependencies
#[derive(Debug, Clone)]
pub struct ScannedUnit {
    pub code_hash: String,
    pub content_hash: String,
    pub deps: BTreeSet<String>,
}

/// ChangeScanner configuration
#[derive(Debug, Clone)]
pub struct ScannerConfig {
    pub repo_path: PathBuf,
}

/// ChangeScanner - Compares BuildUnit.yaml hashes to current filesystem state
pub struct ChangeScanner {
    plugin_mapper: PluginMapper,
    project_name: String,
    project_path: PathBuf,
}

impl ChangeScanner {
    /// Create a new ChangeScanner
    pub fn new(config: ScannerConfig) -> Result<Self> {
        let plugin_mapper = PluginMapper::new(config.repo_path.clone())?;
        let (project_name, project_path) = detect_project(&config.repo_path)?;
        Ok(Self {
            plugin_mapper,
            project_name,
            project_path,
        })
    }

    /// Scan for changed plugins by comparing stored vs current hashes
    pub fn scan_changed(&self) -> Result<ChangedSet> {
        tracing::info!("Scanning for changed plugins via BuildUnit descriptors...");

        let mut changed = Vec::new();

        if self.unit_changed(&self.project_name, &self.project_path)? {
            changed.push(self.project_name.clone());
        }

        for plugin_name in self.plugin_mapper.get_all_plugins() {
            let plugin_path = self.plugin_mapper.get_plugin_path(&plugin_name)?;
            if self.unit_changed(&plugin_name, &plugin_path)? {
                changed.push(plugin_name);
            }
        }

        changed.sort();
        changed.dedup();
        Ok(ChangedSet { plugins: changed })
    }

    /// Scan all units and return snapshot for state comparison
    ///
    /// Returns a snapshot containing code/content hashes and dependencies for ALL plugins.
    /// This snapshot is consumed by build_service_state::mark_dirty_from_scan() to determine
    /// which units need rebuilding.
    pub fn scan_all(&self) -> Result<ScanSnapshot> {
        tracing::info!("Scanning all units for state comparison...");

        let mut units = BTreeMap::new();

        // Create dependency resolver for getting plugin dependencies
        let dep_resolver = DependencyResolver::new(self.plugin_mapper.repo_root().to_path_buf())?;

        // Scan project first
        let project_code_hash = compute_code_hash(&self.project_path)?;
        let project_content_hash = compute_content_hash(&self.project_path)?;
        // Project has no dependencies on other units
        let project_deps = BTreeSet::new();

        units.insert(
            self.project_name.clone(),
            ScannedUnit {
                code_hash: project_code_hash,
                content_hash: project_content_hash,
                deps: project_deps,
            },
        );

        // Scan all plugins
        for plugin_name in self.plugin_mapper.get_all_plugins() {
            let plugin_path = self.plugin_mapper.get_plugin_path(&plugin_name)?;

            let code_hash = compute_code_hash(&plugin_path)?;
            let content_hash = compute_content_hash(&plugin_path)?;

            // Get direct dependencies (not transitive) - filter to only project plugins
            let raw_deps = dep_resolver.get_direct_dependencies(&plugin_name)?;
            let deps: BTreeSet<String> = raw_deps.into_iter().collect();

            units.insert(
                plugin_name,
                ScannedUnit {
                    code_hash,
                    content_hash,
                    deps,
                },
            );
        }

        tracing::info!("Scanned {} units", units.len());

        Ok(ScanSnapshot { units })
    }

    fn unit_changed(&self, unit_name: &str, unit_path: &Path) -> Result<bool> {
        let descriptor_opt = BuildUnitDescriptor::load(unit_path)?;

        if descriptor_opt.is_none() {
            tracing::warn!(
                "Unit '{}' is missing BuildUnit.yaml; forcing rebuild",
                unit_name
            );
            return Ok(true);
        }

        let descriptor = descriptor_opt.unwrap();
        let code_hash = compute_code_hash(unit_path)?;
        let content_hash = compute_content_hash(unit_path)?;

        Ok(descriptor.hashes.code != code_hash || descriptor.hashes.content != content_hash)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_scanner_creation() {
        let current_dir = env::current_dir().unwrap();
        let mut repo_path = current_dir.clone();

        while !repo_path.join(".git").exists() {
            if !repo_path.pop() {
                return;
            }
        }

        let config = ScannerConfig {
            repo_path: repo_path.clone(),
        };

        let result = ChangeScanner::new(config);
        assert!(result.is_ok());
    }
}
