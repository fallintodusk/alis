use crate::build_unit::BuildUnitDescriptor;
use crate::hashing::{compute_code_hash, compute_content_hash};
use crate::plugin_mapper::PluginMapper;
use crate::project::detect_project;
use anyhow::{Context, Result};
use std::path::{Path, PathBuf};

/// BuildUnitBuilder - Step 0 helper that creates descriptors before scans/builds
pub struct BuildUnitBuilder {
    plugin_mapper: PluginMapper,
    project_path: PathBuf,
    project_name: String,
}

impl BuildUnitBuilder {
    pub fn new(repo_path: PathBuf) -> Result<Self> {
        let plugin_mapper = PluginMapper::new(repo_path.clone())?;
        let (project_name, project_path) = detect_project(&repo_path)?;
        Ok(Self {
            plugin_mapper,
            project_path,
            project_name,
        })
    }

    /// Ensure project + all plugins have descriptors
    pub fn ensure_all(&self) -> Result<()> {
        self.ensure_project()?;
        let plugins = self.plugin_mapper.get_all_plugins();
        self.ensure_for_plugins(&plugins)
    }

    /// Ensure descriptors exist for the provided plugin names
    pub fn ensure_for_plugins(&self, plugins: &[String]) -> Result<()> {
        for plugin_name in plugins {
            self.ensure_descriptor(plugin_name)?;
        }
        Ok(())
    }

    /// Refresh hashes for built plugins (creates descriptor if missing)
    pub fn refresh_hashes(&self, plugins: &[String]) -> Result<()> {
        for plugin_name in plugins {
            self.write_descriptor(plugin_name, true)?;
        }
        Ok(())
    }

    /// Ensure project descriptor exists (root game code/config/content)
    pub fn ensure_project(&self) -> Result<()> {
        self.write_project_descriptor(false)
    }

    /// Refresh project descriptor hashes (keeps launcher baseline current)
    pub fn refresh_project(&self) -> Result<()> {
        self.write_project_descriptor(true)
    }

    pub fn plugin_mapper(&self) -> &PluginMapper {
        &self.plugin_mapper
    }

    pub fn project_name(&self) -> &str {
        &self.project_name
    }

    pub fn project_path(&self) -> &Path {
        &self.project_path
    }

    fn ensure_descriptor(&self, plugin_name: &str) -> Result<()> {
        self.write_descriptor(plugin_name, false)
    }

    fn write_descriptor(&self, plugin_name: &str, force: bool) -> Result<()> {
        let plugin_path = self.plugin_mapper.get_plugin_path(plugin_name)?;
        self.write_descriptor_for_path(plugin_name, &plugin_path, force)
    }

    fn write_project_descriptor(&self, force: bool) -> Result<()> {
        self.write_descriptor_for_path(&self.project_name, &self.project_path, force)
    }

    fn write_descriptor_for_path(
        &self,
        unit_name: &str,
        path: &Path,
        force: bool,
    ) -> Result<()> {
        let descriptor_path = path.join("BuildUnit.yaml");

        if !force && descriptor_path.exists() {
            return Ok(());
        }

        let code_hash = compute_code_hash(path)?;
        let content_hash = compute_content_hash(path)?;

        let mut descriptor = if descriptor_path.exists() {
            BuildUnitDescriptor::load(path)?
                .with_context(|| format!("Failed to load {}", descriptor_path.display()))?
        } else {
            BuildUnitDescriptor::new(unit_name, code_hash.clone(), content_hash.clone())
        };

        descriptor.update_hashes(code_hash, content_hash);
        descriptor.save(path)?;
        Ok(())
    }
}
