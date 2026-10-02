//! BuildUnit.yaml descriptor support (baseline metadata + hashes)

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use uuid::Uuid;

/// BuildUnit descriptor (one per plugin, stored next to .uplugin)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildUnitDescriptor {
    /// Plugin name
    pub name: String,

    /// Stable identifier (generated when descriptor is created)
    #[serde(default)]
    pub uuid: Option<String>,

    /// Optional module override
    #[serde(default)]
    pub module: Option<String>,

    /// Rebuild on dependency change (default: false)
    #[serde(default)]
    pub rebuild_on_dependency_change: bool,

    /// Custom file globs to include in fingerprint
    #[serde(default)]
    pub custom_globs: Vec<String>,

    /// Rollout rules (channel-specific settings)
    #[serde(default)]
    pub rollout: RolloutRules,

    /// Stored hashes from the last successful build
    #[serde(default)]
    pub hashes: DescriptorHashes,
}

/// Hash bucket definitions
#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct DescriptorHashes {
    #[serde(default)]
    pub code: String,
    #[serde(default)]
    pub content: String,
    #[serde(default)]
    pub data: HashMap<String, String>,
}

/// Rollout rules for different channels
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct RolloutRules {
    /// Channels this plugin should be deployed to
    #[serde(default)]
    pub channels: Vec<String>,

    /// Minimum version requirements
    #[serde(default)]
    pub min_version: Option<String>,
}

impl BuildUnitDescriptor {
    /// Create a new descriptor populated with hashes
    pub fn new(name: &str, code_hash: String, content_hash: String) -> Self {
        Self {
            name: name.to_string(),
            uuid: Some(Uuid::new_v4().to_string()),
            module: None,
            rebuild_on_dependency_change: false,
            custom_globs: Vec::new(),
            rollout: RolloutRules::default(),
            hashes: DescriptorHashes {
                code: code_hash,
                content: content_hash,
                data: HashMap::new(),
            },
        }
    }

    /// Load BuildUnit.yaml from plugin directory
    pub fn load(plugin_path: &Path) -> Result<Option<Self>> {
        let build_unit_path = plugin_path.join("BuildUnit.yaml");

        if !build_unit_path.exists() {
            return Ok(None);
        }

        let contents = fs::read_to_string(&build_unit_path)
            .with_context(|| format!("Failed to read {}", build_unit_path.display()))?;

        let descriptor: BuildUnitDescriptor = serde_yaml::from_str(&contents)
            .context("Failed to parse BuildUnit.yaml")?;

        Ok(Some(descriptor))
    }

    /// Persist descriptor to disk
    pub fn save(&self, plugin_path: &Path) -> Result<()> {
        let build_unit_path = plugin_path.join("BuildUnit.yaml");
        let contents = serde_yaml::to_string(self).context("Failed to serialize BuildUnit.yaml")?;
        fs::write(&build_unit_path, contents)
            .with_context(|| format!("Failed to write {}", build_unit_path.display()))?;
        Ok(())
    }

    /// Update hashes while preserving metadata
    pub fn update_hashes(&mut self, code_hash: String, content_hash: String) {
        self.hashes.code = code_hash;
        self.hashes.content = content_hash;
        if self.uuid.is_none() {
            self.uuid = Some(Uuid::new_v4().to_string());
        }
    }

    /// Check if this plugin should be deployed to a specific channel
    pub fn is_channel_enabled(&self, channel: &str) -> bool {
        if self.rollout.channels.is_empty() {
            // No channel restrictions - deploy to all
            return true;
        }

        self.rollout.channels.iter().any(|c| c == channel)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_build_unit_descriptor_defaults() {
        let mut descriptor = BuildUnitDescriptor::new("TestPlugin", "code".into(), "content".into());
        assert_eq!(descriptor.name, "TestPlugin");
        assert!(descriptor.is_channel_enabled("dev"));
        assert!(descriptor.hashes.code == "code");
        assert!(descriptor.hashes.content == "content");

        descriptor.update_hashes("new_code".into(), "new_content".into());
        assert_eq!(descriptor.hashes.code, "new_code");
        assert!(descriptor.uuid.is_some());
    }

    #[test]
    fn test_channel_restrictions() {
        let descriptor = BuildUnitDescriptor {
            name: "TestPlugin".to_string(),
            uuid: None,
            module: None,
            rebuild_on_dependency_change: false,
            custom_globs: Vec::new(),
            rollout: RolloutRules {
                channels: vec!["dev".to_string(), "test".to_string()],
                min_version: None,
            },
            hashes: DescriptorHashes::default(),
        };

        assert!(descriptor.is_channel_enabled("dev"));
        assert!(descriptor.is_channel_enabled("test"));
        assert!(!descriptor.is_channel_enabled("prod"));
    }

    #[test]
    fn test_load_non_existent() {
        let temp_dir = env::temp_dir();
        let result = BuildUnitDescriptor::load(&temp_dir);
        assert!(result.is_ok());
        assert!(result.unwrap().is_none());
    }
}
