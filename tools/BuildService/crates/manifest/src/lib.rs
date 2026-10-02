//! ManifestStore crate - Reads/writes manifest.json (Git source of truth)
//!
//! Consumed by: BuildCli
//! Consumes: manifest.json in repo root, CDN schema
//!
//! Responsibilities:
//! - Read/parse manifest.json
//! - Update plugin entries (version, hashes, assets, dependencies, channel)
//! - Validate against CDN schema
//! - Sign manifest (placeholder for now)
//! - Write atomically (Git commit handled by CLI)

#![recursion_limit = "256"]

mod atomic_writer;
mod schema_validator;

pub use atomic_writer::AtomicWriter;
pub use schema_validator::SchemaValidator;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

/// Validate that assets[] contains exactly one utoc and one ucas, with optional pak
/// All files must share the same basename (e.g., "PluginName-Windows")
fn validate_assets_pair(plugin_name: &str, assets: &[Artifact]) -> Result<()> {
    let mut utoc_count = 0usize;
    let mut ucas_count = 0usize;
    let mut pak_count = 0usize;

    for artifact in assets {
        match artifact.role.as_deref() {
            Some("utoc") => utoc_count += 1,
            Some("ucas") => ucas_count += 1,
            Some("pak") => pak_count += 1,
            _ => {}
        }
    }

    // Check asset counts: must have exactly 1 utoc + 1 ucas, optionally 1 pak
    if utoc_count != 1 || ucas_count != 1 || pak_count > 1 {
        return Err(anyhow::anyhow!(
            "Plugin '{}' assets[] must contain exactly one 'utoc', one 'ucas', and optionally one 'pak' \
            (found {} utoc, {} ucas, {} pak, {} total)",
            plugin_name,
            utoc_count,
            ucas_count,
            pak_count,
            assets.len()
        ));
    }

    // Validate total count (2-3 assets)
    if assets.len() < 2 || assets.len() > 3 {
        return Err(anyhow::anyhow!(
            "Plugin '{}' assets[] must contain 2-3 artifacts (found {})",
            plugin_name,
            assets.len()
        ));
    }

    let utoc = assets.iter().find(|a| a.role.as_deref() == Some("utoc")).unwrap();
    let ucas = assets.iter().find(|a| a.role.as_deref() == Some("ucas")).unwrap();
    let pak_opt = assets.iter().find(|a| a.role.as_deref() == Some("pak"));

    // Extract filename from URL
    let utoc_name = std::path::Path::new(&utoc.url)
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("");
    let ucas_name = std::path::Path::new(&ucas.url)
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("");

    // Validate URL extension matches role
    if !utoc_name.ends_with(".utoc") {
        return Err(anyhow::anyhow!(
            "Plugin '{}': utoc artifact URL must end with .utoc (got: {})",
            plugin_name,
            utoc_name
        ));
    }
    if !ucas_name.ends_with(".ucas") {
        return Err(anyhow::anyhow!(
            "Plugin '{}': ucas artifact URL must end with .ucas (got: {})",
            plugin_name,
            ucas_name
        ));
    }

    // Validate shared basename (e.g., "PluginName-Windows" for .utoc and .ucas)
    let utoc_base = utoc_name.trim_end_matches(".utoc");
    let ucas_base = ucas_name.trim_end_matches(".ucas");

    if utoc_base != ucas_base {
        return Err(anyhow::anyhow!(
            "Plugin '{}': utoc and ucas files must share the same basename\n\
            utoc: {}\n\
            ucas: {}\n\
            (expected both to have basename: '{}')",
            plugin_name,
            utoc_name,
            ucas_name,
            utoc_base
        ));
    }

    // If pak exists, validate it matches the same basename
    if let Some(pak) = pak_opt {
        let pak_name = std::path::Path::new(&pak.url)
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("");

        if !pak_name.ends_with(".pak") {
            return Err(anyhow::anyhow!(
                "Plugin '{}': pak artifact URL must end with .pak (got: {})",
                plugin_name,
                pak_name
            ));
        }

        let pak_base = pak_name.trim_end_matches(".pak");
        if pak_base != utoc_base {
            return Err(anyhow::anyhow!(
                "Plugin '{}': pak file must share the same basename as utoc/ucas\n\
                pak:  {}\n\
                utoc: {}\n\
                ucas: {}\n\
                (expected all to have basename: '{}')",
                plugin_name,
                pak_name,
                utoc_name,
                ucas_name,
                utoc_base
            ));
        }
    }

    Ok(())
}

/// Dependency constraint
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Dependency {
    pub name: String,
    pub version: String,  // Format: >=X.Y.Z
}

/// Artifact (code or content file)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Artifact {
    pub url: String,
    pub hash: String,  // SHA-256, 64 hex chars
    pub size: u64,     // Bytes
    #[serde(skip_serializing_if = "Option::is_none")]
    pub role: Option<String>,  // "utoc", "ucas", or "pak" for assets
}

/// Plugin entry in the locally validated bootloader manifest.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PluginManifestEntry {
    pub uuid: String,  // RFC 4122 format, used for storage paths
    pub name: String,
    pub version: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub module: Option<String>,
    pub platform: String,  // "Windows", "Linux", or "Mac"
    pub code: Artifact,    // Code artifact (code.zip)
    pub assets: Vec<Artifact>,  // Content artifacts (.utoc, .ucas)
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub depends_on: Vec<Dependency>,
    #[serde(default = "default_channel")]
    pub channel: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub signature_thumbprint: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub release_notes: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub mirrors: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub base_release_version: Option<String>,  // Base release version this DLC was built against
}

fn default_channel() -> String {
    "stable".to_string()
}

/// Full manifest structure (matches bootloader schema)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Manifest {
    pub manifest_version: i32,
    pub engine_build_id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub signed_at: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub signing_key_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub signature: Option<String>,
    pub plugins: Vec<PluginManifestEntry>,
}

/// ManifestStore - Manages manifest.json
pub struct ManifestStore {
    manifest_path: PathBuf,
    validator: SchemaValidator,
}

impl ManifestStore {
    pub fn new(manifest_path: PathBuf) -> Result<Self> {
        let validator = SchemaValidator::new()?;
        Ok(Self {
            manifest_path,
            validator,
        })
    }

    /// Load manifest from file
    pub fn load(&self) -> Result<Manifest> {
        if !self.manifest_path.exists() {
            tracing::warn!("Manifest not found, creating new: {}", self.manifest_path.display());
            return Ok(Manifest {
                manifest_version: 1,
                engine_build_id: "UE5.5-CL-0".to_string(),  // Placeholder
                signed_at: None,
                signing_key_id: None,
                signature: None,
                plugins: Vec::new(),
            });
        }

        let contents = std::fs::read_to_string(&self.manifest_path)
            .with_context(|| format!("Failed to read manifest: {}", self.manifest_path.display()))?;

        let manifest: Manifest = serde_json::from_str(&contents)
            .with_context(|| "Failed to parse manifest JSON")?;

        tracing::info!("Loaded manifest with {} plugins", manifest.plugins.len());

        Ok(manifest)
    }

    /// Save manifest to file (atomic write)
    pub fn save(&self, manifest: &Manifest) -> Result<()> {
        // Validate before saving
        let json_value = serde_json::to_value(manifest)?;
        self.validator.validate(&json_value)?;

        // Serialize to pretty JSON
        let json = serde_json::to_string_pretty(manifest)
            .context("Failed to serialize manifest")?;

        // Atomic write
        AtomicWriter::write_atomic(&self.manifest_path, json.as_bytes())?;

        tracing::info!("Saved manifest with {} plugins to: {}", manifest.plugins.len(), self.manifest_path.display());

        Ok(())
    }

    /// Update plugin entry in manifest
    pub fn update_plugin(&self, manifest: &mut Manifest, entry: PluginManifestEntry) -> Result<()> {
        // Find existing plugin entry
        if let Some(existing) = manifest.plugins.iter_mut().find(|p| p.name == entry.name) {
            tracing::info!("Updating existing plugin: {}", entry.name);
            *existing = entry;
        } else {
            tracing::info!("Adding new plugin: {}", entry.name);
            manifest.plugins.push(entry);
        }

        // Sort plugins by name for consistency
        manifest.plugins.sort_by(|a, b| a.name.cmp(&b.name));

        Ok(())
    }

    /// Validate manifest against CDN schema
    pub fn validate(&self, manifest: &Manifest) -> Result<()> {
        // Schema validation (structure, types, patterns)
        let json_value = serde_json::to_value(manifest)?;
        self.validator.validate(&json_value)?;

        // Runtime validation (exact utoc+ucas pair)
        for plugin in &manifest.plugins {
            validate_assets_pair(&plugin.name, &plugin.assets)?;
        }

        Ok(())
    }

    /// Sign manifest (placeholder)
    pub fn sign(&self, _manifest: &Manifest) -> Result<String> {
        tracing::warn!("Manifest signing not implemented, skipping");

        // Placeholder - would use private key to sign manifest
        // For now, return empty signature
        Ok(String::new())
    }

    /// Get plugin entry by name
    pub fn get_plugin<'a>(&self, manifest: &'a Manifest, name: &str) -> Option<&'a PluginManifestEntry> {
        manifest.plugins.iter().find(|p| p.name == name)
    }

    /// Remove plugin entry by name
    pub fn remove_plugin(&self, manifest: &mut Manifest, name: &str) -> Result<bool> {
        let initial_len = manifest.plugins.len();
        manifest.plugins.retain(|p| p.name != name);

        let removed = manifest.plugins.len() < initial_len;
        if removed {
            tracing::info!("Removed plugin: {}", name);
        }

        Ok(removed)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;

    #[test]
    fn test_manifest_store_creation() {
        let store = ManifestStore::new(PathBuf::from("manifest.json")).unwrap();
        assert!(!store.manifest_path.to_string_lossy().is_empty());
    }

    #[test]
    fn test_load_nonexistent_manifest() {
        let temp_dir = env::temp_dir();
        let manifest_path = temp_dir.join("nonexistent_manifest.json");

        let store = ManifestStore::new(manifest_path).unwrap();
        let manifest = store.load().unwrap();

        assert_eq!(manifest.manifest_version, 1);
        assert_eq!(manifest.engine_build_id, "UE5.5-CL-0");
        assert_eq!(manifest.plugins.len(), 0);
    }

    #[test]
    fn test_save_and_load_manifest() {
        let temp_dir = env::temp_dir();
        let manifest_path = temp_dir.join("test_manifest.json");

        let store = ManifestStore::new(manifest_path.clone()).unwrap();

        let manifest = Manifest {
            manifest_version: 1,
            engine_build_id: "UE5.5-CL-123456".to_string(),
            signed_at: None,
            signing_key_id: None,
            signature: None,
            plugins: vec![
                PluginManifestEntry {
                    uuid: "550e8400-e29b-41d4-a716-446655440000".to_string(),
                    name: "TestPlugin".to_string(),
                    version: "1.0.0".to_string(),
                    module: Some("TestPluginCore".to_string()),
                    platform: "Windows".to_string(),
                    code: Artifact {
                        url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/code.zip".to_string(),
                        hash: "a".repeat(64),
                        size: 1024,
                        role: None,
                    },
                    assets: vec![
                        Artifact {
                            url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.utoc".to_string(),
                            hash: "b".repeat(64),
                            size: 2048,
                            role: Some("utoc".to_string()),
                        },
                        Artifact {
                            url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440000/1.0.0/content.ucas".to_string(),
                            hash: "c".repeat(64),
                            size: 4096,
                            role: Some("ucas".to_string()),
                        }
                    ],
                    depends_on: vec![],
                    channel: "stable".to_string(),
                    signature_thumbprint: None,
                    release_notes: None,
                    mirrors: vec![],
                    base_release_version: None,
                }
            ],
        };

        store.save(&manifest).unwrap();
        let loaded = store.load().unwrap();

        assert_eq!(loaded.plugins.len(), 1);
        assert_eq!(loaded.plugins[0].name, "TestPlugin");

        // Cleanup
        let _ = std::fs::remove_file(manifest_path);
    }

    #[test]
    fn test_update_plugin() {
        let temp_dir = env::temp_dir();
        let manifest_path = temp_dir.join("test_update_manifest.json");

        let store = ManifestStore::new(manifest_path.clone()).unwrap();
        let mut manifest = Manifest {
            manifest_version: 1,
            engine_build_id: "UE5.5-CL-123456".to_string(),
            signed_at: None,
            signing_key_id: None,
            signature: None,
            plugins: vec![],
        };

        let entry = PluginManifestEntry {
            uuid: "550e8400-e29b-41d4-a716-446655440001".to_string(),
            name: "TestPlugin".to_string(),
            version: "1.0.0".to_string(),
            module: Some("TestPluginCore".to_string()),
            platform: "Windows".to_string(),
            code: Artifact {
                url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/1.0.0/code.zip".to_string(),
                hash: "a".repeat(64),
                size: 1024,
                role: None,
            },
            assets: vec![
                Artifact {
                    url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/1.0.0/content.utoc".to_string(),
                    hash: "0".repeat(64),
                    size: 2048,
                    role: Some("utoc".to_string()),
                },
                Artifact {
                    url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/1.0.0/content.ucas".to_string(),
                    hash: "1".repeat(64),
                    size: 4096,
                    role: Some("ucas".to_string()),
                }
            ],
            depends_on: vec![],
            channel: "stable".to_string(),
            signature_thumbprint: None,
            release_notes: None,
            mirrors: vec![],
            base_release_version: None,
        };

        store.update_plugin(&mut manifest, entry).unwrap();
        assert_eq!(manifest.plugins.len(), 1);

        // Update existing
        let updated_entry = PluginManifestEntry {
            uuid: "550e8400-e29b-41d4-a716-446655440001".to_string(),
            name: "TestPlugin".to_string(),
            version: "2.0.0".to_string(),
            module: Some("TestPluginCore".to_string()),
            platform: "Windows".to_string(),
            code: Artifact {
                url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/2.0.0/code.zip".to_string(),
                hash: "b".repeat(64),
                size: 2048,
                role: None,
            },
            assets: vec![
                Artifact {
                    url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/2.0.0/content.utoc".to_string(),
                    hash: "2".repeat(64),
                    size: 4096,
                    role: Some("utoc".to_string()),
                },
                Artifact {
                    url: "https://cdn.alis.game/units/550e8400-e29b-41d4-a716-446655440001/2.0.0/content.ucas".to_string(),
                    hash: "3".repeat(64),
                    size: 8192,
                    role: Some("ucas".to_string()),
                }
            ],
            depends_on: vec![],
            channel: "stable".to_string(),
            signature_thumbprint: None,
            release_notes: None,
            mirrors: vec![],
            base_release_version: None,
        };

        store.update_plugin(&mut manifest, updated_entry).unwrap();
        assert_eq!(manifest.plugins.len(), 1);
        assert_eq!(manifest.plugins[0].version, "2.0.0");

        // Cleanup
        let _ = std::fs::remove_file(manifest_path);
    }
}
