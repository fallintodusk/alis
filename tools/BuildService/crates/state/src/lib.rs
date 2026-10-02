//! Incremental build state for DLC-based modular pipeline
//!
//! Tracks per-unit state (code hash, content hash, deps, dirty flag) to enable:
//! - Deterministic incremental builds (rebuild only changed units + dependents)
//! - Engine and base release pin validation (fail fast on mismatch)
//! - Atomic state updates with file locking (safe concurrent access)
//!
//! State persists to YAML at `.alis/state.yaml` with deterministic ordering.

use anyhow::{Context, Result};
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::io::Write;
use std::path::Path;

// ============================================================================
// Core State Model
// ============================================================================

/// Build state with per-unit incremental tracking
///
/// Persisted to YAML with BTreeMap/BTreeSet for deterministic diffs.
/// Updated atomically via temp file + rename with file lock.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BuildState {
    /// Schema version (bump on breaking changes)
    pub format: u16,

    /// Last update timestamp (RFC3339 UTC)
    pub updated_at: DateTime<Utc>,

    /// Engine/toolchain pin (UE version + toolchain hash)
    pub engine: EnginePin,

    /// Base release pin (DLC base version)
    pub base_release: BaseReleasePin,

    /// Target platform (Windows, Linux, Mac)
    pub platform: String,

    /// Per-plugin unit state (sorted by plugin name)
    pub units: BTreeMap<String, Unit>,
}

impl BuildState {
    /// Create new state with pins (preferred constructor)
    pub fn with_pins(engine: EnginePin, base_release: BaseReleasePin, platform: String) -> Self {
        Self {
            format: 1,
            updated_at: Utc::now(),
            engine,
            base_release,
            platform,
            units: BTreeMap::new(),
        }
    }

    /// Create new empty state (deprecated compatibility shim)
    #[deprecated(note = "Use with_pins() or Default::default() instead")]
    pub fn new() -> Self {
        Self::default()
    }

    /// Get base release version (convenience helper)
    pub fn base_version(&self) -> &str {
        &self.base_release.version
    }

    /// Get base release hash (convenience helper)
    pub fn base_hash(&self) -> &str {
        &self.base_release.hash_hex
    }

    /// Get base release sanitized plugins (convenience helper)
    pub fn base_plugins(&self) -> &[String] {
        &self.base_release.sanitized_plugins
    }

    /// Load state from YAML file
    ///
    /// Returns error if file doesn't exist or is malformed.
    /// Use `load_or_init()` for first-run convenience.
    pub fn load(path: &Path) -> Result<Self> {
        if !path.exists() {
            return Err(anyhow::anyhow!("State file does not exist: {}", path.display()));
        }

        let contents = fs::read_to_string(path)
            .with_context(|| format!("Failed to read state file: {}", path.display()))?;

        let state: BuildState = serde_yaml::from_str(&contents)
            .with_context(|| format!("Failed to parse state YAML: {}", path.display()))?;

        tracing::info!(
            "Loaded state: format={}, platform={}, {} units",
            state.format,
            state.platform,
            state.units.len()
        );

        Ok(state)
    }

    /// Load state or initialize new state if file doesn't exist (first run)
    ///
    /// Creates and saves a new state file if none exists. Idempotent and
    /// safe for first-run scenarios.
    ///
    /// # Arguments
    /// * `path` - Path to state file
    /// * `engine` - Engine pin for new state
    /// * `base` - Base release pin for new state
    /// * `platform` - Target platform (Windows, Linux, Mac)
    pub fn load_or_init(
        path: &Path,
        engine: EnginePin,
        base: BaseReleasePin,
        platform: &str,
    ) -> Result<Self> {
        match Self::load(path) {
            Ok(state) => Ok(state),
            Err(_) => {
                tracing::info!("State file not found, initializing new state");
                let mut state = Self::with_pins(engine, base, platform.to_string());
                save_atomic(path, &mut state)?;
                tracing::info!("Created new state file: {}", path.display());
                Ok(state)
            }
        }
    }
}

/// Engine and toolchain pin
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct EnginePin {
    /// Unreal Engine version (e.g., "5.5.x")
    pub ue_version: String,

    /// Toolchain hash (SHA-256 hex of Installed Build id or source commit)
    pub toolchain_hash: String,
}

impl EnginePin {
    pub fn new(ue_version: String, toolchain_hash: String) -> Self {
        Self {
            ue_version,
            toolchain_hash,
        }
    }
}

/// Base release pin (DLC workflow)
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct BaseReleasePin {
    /// Base release version ID (e.g., "ALIS_20251119_100000")
    pub version: String,

    /// SHA-256 hash of base pakchunk0 (.utoc + .ucas)
    #[serde(default)]
    pub hash_hex: String,

    /// Plugins stripped during sanitization (not included in base)
    #[serde(default)]
    pub sanitized_plugins: Vec<String>,
}

impl BaseReleasePin {
    pub fn new(version: String) -> Self {
        Self {
            version,
            hash_hex: String::new(),
            sanitized_plugins: Vec::new(),
        }
    }

    /// Create pin with full metadata (used after base release creation)
    pub fn with_metadata(version: String, hash_hex: String, sanitized_plugins: Vec<String>) -> Self {
        Self {
            version,
            hash_hex,
            sanitized_plugins,
        }
    }
}

/// Per-plugin unit state
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Unit {
    /// Stable UUID per plugin (generated once, never changes)
    pub uuid: String,

    /// Code hash from scanner (hex, excludes Binaries/)
    pub code_hash: String,

    /// Content hash from scanner (hex, Content/ directory)
    pub content_hash: String,

    /// Last successful build timestamp (RFC3339 UTC)
    pub last_built_at: Option<DateTime<Utc>>,

    /// Last published version (semver or timestamp tag)
    pub last_published_version: Option<String>,

    /// Dependencies (plugin names this unit depends on, sorted)
    pub deps: BTreeSet<String>,

    /// Dirty flag (needs rebuild due to code/content/engine/base/deps change)
    pub dirty: bool,
}

impl Unit {
    /// Create new unit with generated UUID
    pub fn new(code_hash: String, content_hash: String, deps: BTreeSet<String>) -> Self {
        Self {
            uuid: uuid::Uuid::new_v4().to_string(),
            code_hash,
            content_hash,
            last_built_at: None,
            last_published_version: None,
            deps,
            dirty: true, // New units are always dirty
        }
    }
}

// ============================================================================
// Atomic Save with File Lock
// ============================================================================

/// Save state atomically with file lock and temp+rename
///
/// 1. Acquire exclusive file lock (.state.lock in same directory)
/// 2. Write to temp file (NamedTempFile)
/// 3. Persist to target path (Windows-safe overwrite)
///
/// BTreeMap/BTreeSet ensure stable YAML diffs across machines.
pub fn save_atomic(path: &Path, state: &mut BuildState) -> Result<()> {
    use fs2::FileExt;

    let dir = path
        .parent()
        .ok_or_else(|| anyhow::anyhow!("State path has no parent directory"))?;

    fs::create_dir_all(dir)
        .with_context(|| format!("Failed to create state directory: {}", dir.display()))?;

    // Acquire exclusive file lock
    let lock_path = dir.join(".state.lock");
    let lock_file = fs::OpenOptions::new()
        .create(true)
        .read(true)
        .write(true)
        .open(&lock_path)
        .with_context(|| format!("Failed to open state lock: {}", lock_path.display()))?;

    lock_file
        .lock_exclusive()
        .with_context(|| "Failed to acquire exclusive state lock")?;

    // Update timestamp
    state.updated_at = Utc::now();

    // Serialize to YAML (BTreeMap ensures sorted keys)
    let yaml = serde_yaml::to_string(state).context("Failed to serialize state to YAML")?;

    // Write to temp file
    let mut tmp = tempfile::NamedTempFile::new_in(dir)
        .with_context(|| format!("Failed to create temp state file in: {}", dir.display()))?;

    tmp.as_file_mut()
        .write_all(yaml.as_bytes())
        .context("Failed to write temp state file")?;

    // Persist to target path (Windows-safe: remove existing file first)
    #[cfg(windows)]
    {
        if path.exists() {
            fs::remove_file(path)
                .with_context(|| format!("Failed to remove old state: {}", path.display()))?;
        }
        tmp.persist(path)
            .with_context(|| format!("Failed to persist state: {}", path.display()))?;
    }

    #[cfg(not(windows))]
    {
        tmp.persist(path)
            .with_context(|| format!("Failed to persist state: {}", path.display()))?;
    }

    tracing::info!("Saved state atomically: {} units", state.units.len());

    // Explicitly release lock (would drop anyway, but makes intent clear)
    lock_file.unlock().context("Failed to release state lock")?;

    Ok(())
}

// ============================================================================
// Dirty Marking API
// ============================================================================

/// Scanner snapshot for dirty marking
///
/// Contains per-plugin hashes and dependencies from scanner crate.
#[derive(Debug, Clone)]
pub struct ScanSnapshot {
    pub units: BTreeMap<String, ScannedUnit>,
}

/// Single scanned unit (plugin)
#[derive(Debug, Clone)]
pub struct ScannedUnit {
    pub code_hash: String,
    pub content_hash: String,
    pub deps: BTreeSet<String>,
}

/// Mark units dirty by comparing scanner output with state
///
/// Rules:
/// 1. If engine or base_release pin changed → mark ALL units dirty
/// 2. Else per unit:
///    - code_hash changed → dirty
///    - content_hash changed → dirty
///    - deps set changed → dirty
///    - new plugin (not in state) → create unit, mark dirty
///
/// Returns number of units marked dirty.
pub fn mark_dirty_from_scan(
    state: &mut BuildState,
    scan: ScanSnapshot,
    current_engine: &EnginePin,
    current_base: &BaseReleasePin,
) -> Result<usize> {
    let mut dirty_count = 0;

    // Check if pins changed
    let engine_changed = state.engine != *current_engine;
    let base_changed = state.base_release != *current_base;

    if engine_changed || base_changed {
        // Update pins
        state.engine = current_engine.clone();
        state.base_release = current_base.clone();

        // Mark all units dirty
        for unit in state.units.values_mut() {
            if !unit.dirty {
                unit.dirty = true;
                dirty_count += 1;
            }
        }

        tracing::warn!(
            "Pin change detected (engine={}, base={}): marked {} units dirty",
            engine_changed,
            base_changed,
            dirty_count
        );

        return Ok(dirty_count);
    }

    // Per-unit dirty marking
    for (plugin_name, scanned) in scan.units {
        match state.units.get_mut(&plugin_name) {
            Some(unit) => {
                // Existing unit - check for changes
                let mut changed = false;

                if unit.code_hash != scanned.code_hash {
                    tracing::debug!("Plugin {} code hash changed", plugin_name);
                    unit.code_hash = scanned.code_hash;
                    changed = true;
                }

                if unit.content_hash != scanned.content_hash {
                    tracing::debug!("Plugin {} content hash changed", plugin_name);
                    unit.content_hash = scanned.content_hash;
                    changed = true;
                }

                if unit.deps != scanned.deps {
                    tracing::debug!("Plugin {} deps changed", plugin_name);
                    unit.deps = scanned.deps;
                    changed = true;
                }

                if changed && !unit.dirty {
                    unit.dirty = true;
                    dirty_count += 1;
                    tracing::info!("Marked plugin {} dirty (changed)", plugin_name);
                }
            }
            None => {
                // New plugin - create unit and mark dirty
                let unit = Unit::new(scanned.code_hash, scanned.content_hash, scanned.deps);
                tracing::info!("Created new unit for plugin {} (dirty)", plugin_name);
                state.units.insert(plugin_name.clone(), unit);
                dirty_count += 1;
            }
        }
    }

    tracing::info!(
        "Dirty marking complete: {} units dirty out of {}",
        dirty_count,
        state.units.len()
    );

    Ok(dirty_count)
}

// ============================================================================
// Apply Build Result
// ============================================================================

/// Build result for a single plugin
#[derive(Debug, Clone)]
pub struct BuiltUnit {
    pub plugin_name: String,
    pub published_version: String,
    pub code_hash: Option<String>,    // Optional: if builder re-scanned post-build
    pub content_hash: Option<String>, // Optional: if builder re-scanned post-build
}

/// Apply build results to state
///
/// Updates:
/// - last_built_at (current timestamp)
/// - last_published_version
/// - code_hash / content_hash (if provided)
/// - dirty flag (cleared)
///
/// Returns number of units updated.
pub fn apply_build_result(state: &mut BuildState, results: Vec<BuiltUnit>) -> Result<usize> {
    let mut updated_count = 0;
    let now = Utc::now();
    let results_len = results.len();

    for result in results {
        if let Some(unit) = state.units.get_mut(&result.plugin_name) {
            unit.last_built_at = Some(now);
            unit.last_published_version = Some(result.published_version.clone());

            if let Some(hash) = result.code_hash {
                unit.code_hash = hash;
            }

            if let Some(hash) = result.content_hash {
                unit.content_hash = hash;
            }

            if unit.dirty {
                unit.dirty = false;
                tracing::info!(
                    "Applied build result for plugin {} (version {})",
                    result.plugin_name,
                    result.published_version
                );
                updated_count += 1;
            }
        } else {
            tracing::warn!(
                "Build result for unknown plugin {}, skipping",
                result.plugin_name
            );
        }
    }

    tracing::info!(
        "Applied {} build results, {} units updated",
        results_len,
        updated_count
    );

    Ok(updated_count)
}

// ============================================================================
// Pin Validation Guard
// ============================================================================

/// Current pins (from config or detection)
#[derive(Debug, Clone)]
pub struct Pins {
    pub engine: EnginePin,
    pub base_release: BaseReleasePin,
}

/// Assert that state pins match current pins (fail fast)
///
/// Use at start of build to catch pin drift before expensive operations.
pub fn assert_pins(state: &BuildState, current: &Pins) -> Result<()> {
    if state.engine != current.engine {
        return Err(anyhow::anyhow!(
            "Engine pin mismatch!\n\
            State: ue_version={}, toolchain_hash={}\n\
            Current: ue_version={}, toolchain_hash={}\n\
            \n\
            This indicates the engine or toolchain changed since last build.\n\
            All units must be rebuilt. Run with --force-base to refresh.",
            state.engine.ue_version,
            state.engine.toolchain_hash,
            current.engine.ue_version,
            current.engine.toolchain_hash
        ));
    }

    if state.base_release != current.base_release {
        return Err(anyhow::anyhow!(
            "Base release pin mismatch!\n\
            State: version={}\n\
            Current: version={}\n\
            \n\
            This indicates the base release changed since last build.\n\
            All DLC units must be re-cooked against new base.",
            state.base_release.version,
            current.base_release.version
        ));
    }

    tracing::debug!("Pin validation passed");

    Ok(())
}

// ============================================================================
// Compatibility Shims (Deprecated - for executor migration)
// ============================================================================

impl BuildState {
    /// Deprecated save (compatibility shim)
    #[deprecated(note = "Use save_atomic() instead")]
    pub fn save(&mut self, path: &Path) -> Result<()> {
        save_atomic(path, self)
    }
}

impl Default for BuildState {
    fn default() -> Self {
        Self::with_pins(
            EnginePin::new("5.5.0".to_string(), "unknown".to_string()),
            BaseReleasePin::new("ALIS_UNKNOWN".to_string()),
            "Windows".to_string(),
        )
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn make_test_state() -> BuildState {
        BuildState::with_pins(
            EnginePin::new("5.5.0".to_string(), "abc123".to_string()),
            BaseReleasePin::new("ALIS_20251120_100000".to_string()),
            "Windows".to_string(),
        )
    }

    #[test]
    fn test_state_new() {
        let state = make_test_state();
        assert_eq!(state.format, 1);
        assert_eq!(state.platform, "Windows");
        assert!(state.units.is_empty());
    }

    #[test]
    fn test_load_save_roundtrip() {
        let temp_dir = tempfile::tempdir().unwrap();
        let state_path = temp_dir.path().join("state.yaml");

        // Create state with unit
        let mut state = make_test_state();
        let mut unit = Unit::new(
            "code_abc".to_string(),
            "content_xyz".to_string(),
            BTreeSet::from(["PluginA".to_string()]),
        );
        unit.last_published_version = Some("1.0.0".to_string());
        state.units.insert("TestPlugin".to_string(), unit);

        // Save atomically
        save_atomic(&state_path, &mut state).unwrap();
        assert!(state_path.exists());

        // Load and verify all fields
        let loaded = BuildState::load(&state_path).unwrap();
        assert_eq!(loaded.format, 1);
        assert_eq!(loaded.platform, "Windows");
        assert_eq!(loaded.engine.ue_version, "5.5.0");
        assert_eq!(loaded.engine.toolchain_hash, "abc123");
        assert_eq!(loaded.base_release.version, "ALIS_20251120_100000");
        assert_eq!(loaded.units.len(), 1);

        let loaded_unit = loaded.units.get("TestPlugin").unwrap();
        assert_eq!(loaded_unit.code_hash, "code_abc");
        assert_eq!(loaded_unit.content_hash, "content_xyz");
        assert_eq!(
            loaded_unit.last_published_version,
            Some("1.0.0".to_string())
        );
        assert_eq!(loaded_unit.deps.len(), 1);
        assert!(loaded_unit.deps.contains("PluginA"));
    }

    #[test]
    fn test_load_or_init_missing_file() {
        let temp_dir = tempfile::tempdir().unwrap();
        let state_path = temp_dir.path().join("state.yaml");

        // File doesn't exist - should create new state
        assert!(!state_path.exists());

        let state = BuildState::load_or_init(
            &state_path,
            EnginePin::new("5.5.0".to_string(), "test_hash".to_string()),
            BaseReleasePin::new("ALIS_TEST".to_string()),
            "Windows",
        )
        .unwrap();

        // State file should now exist
        assert!(state_path.exists());

        // Verify state was initialized correctly
        assert_eq!(state.format, 1);
        assert_eq!(state.platform, "Windows");
        assert_eq!(state.engine.ue_version, "5.5.0");
        assert_eq!(state.engine.toolchain_hash, "test_hash");
        assert_eq!(state.base_release.version, "ALIS_TEST");
        assert!(state.units.is_empty());

        // Load again - should load existing state, not recreate
        let state2 = BuildState::load_or_init(
            &state_path,
            EnginePin::new("different".to_string(), "different".to_string()),
            BaseReleasePin::new("DIFFERENT".to_string()),
            "Linux",
        )
        .unwrap();

        // Should have loaded original state, not new pins
        assert_eq!(state2.engine.ue_version, "5.5.0");
        assert_eq!(state2.engine.toolchain_hash, "test_hash");
        assert_eq!(state2.platform, "Windows");
    }

    #[test]
    fn test_dirty_on_engine_pin_change() {
        let mut state = make_test_state();

        // Add two units, neither dirty
        let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
        unit1.dirty = false;
        let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
        unit2.dirty = false;

        state.units.insert("Plugin1".to_string(), unit1);
        state.units.insert("Plugin2".to_string(), unit2);

        // Change engine pin
        let new_engine = EnginePin::new("5.5.1".to_string(), "def456".to_string());
        let current_base = state.base_release.clone();

        let scan = ScanSnapshot {
            units: BTreeMap::new(),
        };

        let dirty_count =
            mark_dirty_from_scan(&mut state, scan, &new_engine, &current_base).unwrap();

        // All units should be dirty
        assert_eq!(dirty_count, 2);
        assert!(state.units.get("Plugin1").unwrap().dirty);
        assert!(state.units.get("Plugin2").unwrap().dirty);
        assert_eq!(state.engine.ue_version, "5.5.1");
    }

    #[test]
    fn test_dirty_on_base_release_pin_change() {
        let mut state = make_test_state();

        // Add unit, not dirty
        let mut unit = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
        unit.dirty = false;
        state.units.insert("Plugin1".to_string(), unit);

        // Change base release pin
        let current_engine = state.engine.clone();
        let new_base = BaseReleasePin::new("ALIS_20251121_120000".to_string());

        let scan = ScanSnapshot {
            units: BTreeMap::new(),
        };

        let dirty_count = mark_dirty_from_scan(&mut state, scan, &current_engine, &new_base).unwrap();

        // All units should be dirty
        assert_eq!(dirty_count, 1);
        assert!(state.units.get("Plugin1").unwrap().dirty);
        assert_eq!(state.base_release.version, "ALIS_20251121_120000");
    }

    #[test]
    fn test_selective_dirty_code_hash() {
        let mut state = make_test_state();

        // Add two units, neither dirty
        let mut unit1 = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
        unit1.dirty = false;
        let mut unit2 = Unit::new("code2".to_string(), "content2".to_string(), BTreeSet::new());
        unit2.dirty = false;

        state.units.insert("Plugin1".to_string(), unit1);
        state.units.insert("Plugin2".to_string(), unit2);

        // Scan with Plugin1 code hash changed
        let mut scan_units = BTreeMap::new();
        scan_units.insert(
            "Plugin1".to_string(),
            ScannedUnit {
                code_hash: "code1_changed".to_string(),
                content_hash: "content1".to_string(),
                deps: BTreeSet::new(),
            },
        );
        scan_units.insert(
            "Plugin2".to_string(),
            ScannedUnit {
                code_hash: "code2".to_string(),
                content_hash: "content2".to_string(),
                deps: BTreeSet::new(),
            },
        );

        let scan = ScanSnapshot { units: scan_units };
        let current_engine = state.engine.clone();
        let current_base = state.base_release.clone();

        let dirty_count =
            mark_dirty_from_scan(&mut state, scan, &current_engine, &current_base).unwrap();

        // Only Plugin1 should be dirty
        assert_eq!(dirty_count, 1);
        assert!(state.units.get("Plugin1").unwrap().dirty);
        assert!(!state.units.get("Plugin2").unwrap().dirty);
        assert_eq!(
            state.units.get("Plugin1").unwrap().code_hash,
            "code1_changed"
        );
    }

    #[test]
    fn test_selective_dirty_content_hash() {
        let mut state = make_test_state();

        // Add unit, not dirty
        let mut unit = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
        unit.dirty = false;
        state.units.insert("Plugin1".to_string(), unit);

        // Scan with content hash changed
        let mut scan_units = BTreeMap::new();
        scan_units.insert(
            "Plugin1".to_string(),
            ScannedUnit {
                code_hash: "code1".to_string(),
                content_hash: "content1_changed".to_string(),
                deps: BTreeSet::new(),
            },
        );

        let scan = ScanSnapshot { units: scan_units };
        let current_engine = state.engine.clone();
        let current_base = state.base_release.clone();

        let dirty_count =
            mark_dirty_from_scan(&mut state, scan, &current_engine, &current_base).unwrap();

        // Plugin1 should be dirty
        assert_eq!(dirty_count, 1);
        assert!(state.units.get("Plugin1").unwrap().dirty);
        assert_eq!(
            state.units.get("Plugin1").unwrap().content_hash,
            "content1_changed"
        );
    }

    #[test]
    fn test_selective_dirty_deps_change() {
        let mut state = make_test_state();

        // Add unit with deps, not dirty
        let mut unit = Unit::new(
            "code1".to_string(),
            "content1".to_string(),
            BTreeSet::from(["PluginA".to_string()]),
        );
        unit.dirty = false;
        state.units.insert("Plugin1".to_string(), unit);

        // Scan with deps changed
        let mut scan_units = BTreeMap::new();
        scan_units.insert(
            "Plugin1".to_string(),
            ScannedUnit {
                code_hash: "code1".to_string(),
                content_hash: "content1".to_string(),
                deps: BTreeSet::from(["PluginA".to_string(), "PluginB".to_string()]),
            },
        );

        let scan = ScanSnapshot { units: scan_units };
        let current_engine = state.engine.clone();
        let current_base = state.base_release.clone();

        let dirty_count =
            mark_dirty_from_scan(&mut state, scan, &current_engine, &current_base).unwrap();

        // Plugin1 should be dirty due to deps change
        assert_eq!(dirty_count, 1);
        assert!(state.units.get("Plugin1").unwrap().dirty);
        assert_eq!(state.units.get("Plugin1").unwrap().deps.len(), 2);
        assert!(state.units.get("Plugin1").unwrap().deps.contains("PluginB"));
    }

    #[test]
    fn test_new_plugin_creates_unit() {
        let mut state = make_test_state();

        // Scan with new plugin
        let mut scan_units = BTreeMap::new();
        scan_units.insert(
            "NewPlugin".to_string(),
            ScannedUnit {
                code_hash: "code_new".to_string(),
                content_hash: "content_new".to_string(),
                deps: BTreeSet::new(),
            },
        );

        let scan = ScanSnapshot { units: scan_units };
        let current_engine = state.engine.clone();
        let current_base = state.base_release.clone();

        let dirty_count =
            mark_dirty_from_scan(&mut state, scan, &current_engine, &current_base).unwrap();

        // New unit should be created and dirty
        assert_eq!(dirty_count, 1);
        assert_eq!(state.units.len(), 1);

        let unit = state.units.get("NewPlugin").unwrap();
        assert!(unit.dirty);
        assert_eq!(unit.code_hash, "code_new");
        assert_eq!(unit.content_hash, "content_new");
        assert!(!unit.uuid.is_empty());
    }

    #[test]
    fn test_apply_build_result() {
        let mut state = make_test_state();

        // Add dirty unit
        let unit = Unit::new("code1".to_string(), "content1".to_string(), BTreeSet::new());
        state.units.insert("Plugin1".to_string(), unit);

        // Apply build result
        let results = vec![BuiltUnit {
            plugin_name: "Plugin1".to_string(),
            published_version: "1.0.0".to_string(),
            code_hash: Some("code1_built".to_string()),
            content_hash: None,
        }];

        let updated = apply_build_result(&mut state, results).unwrap();

        // Unit should be updated and not dirty
        assert_eq!(updated, 1);

        let unit = state.units.get("Plugin1").unwrap();
        assert!(!unit.dirty);
        assert_eq!(unit.last_published_version, Some("1.0.0".to_string()));
        assert_eq!(unit.code_hash, "code1_built");
        assert!(unit.last_built_at.is_some());
    }

    #[test]
    fn test_assert_pins_success() {
        let state = make_test_state();

        let current = Pins {
            engine: state.engine.clone(),
            base_release: state.base_release.clone(),
        };

        // Should pass without error
        assert!(assert_pins(&state, &current).is_ok());
    }

    #[test]
    fn test_assert_pins_engine_mismatch() {
        let state = make_test_state();

        let current = Pins {
            engine: EnginePin::new("5.5.1".to_string(), "def456".to_string()),
            base_release: state.base_release.clone(),
        };

        // Should fail with engine mismatch
        let result = assert_pins(&state, &current);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Engine pin mismatch"));
    }

    #[test]
    fn test_assert_pins_base_mismatch() {
        let state = make_test_state();

        let current = Pins {
            engine: state.engine.clone(),
            base_release: BaseReleasePin::new("ALIS_20251121_120000".to_string()),
        };

        // Should fail with base mismatch
        let result = assert_pins(&state, &current);
        assert!(result.is_err());
        assert!(result
            .unwrap_err()
            .to_string()
            .contains("Base release pin mismatch"));
    }

    #[test]
    fn test_btree_deterministic_ordering() {
        let temp_dir = tempfile::tempdir().unwrap();
        let state_path = temp_dir.path().join("state.yaml");

        // Create state with multiple units in random order
        let mut state = make_test_state();
        state
            .units
            .insert("ZPlugin".to_string(), Unit::new("z".to_string(), "z".to_string(), BTreeSet::new()));
        state
            .units
            .insert("APlugin".to_string(), Unit::new("a".to_string(), "a".to_string(), BTreeSet::new()));
        state
            .units
            .insert("MPlugin".to_string(), Unit::new("m".to_string(), "m".to_string(), BTreeSet::new()));

        // Save
        save_atomic(&state_path, &mut state).unwrap();

        // Read YAML as text and verify alphabetical ordering
        let yaml = fs::read_to_string(&state_path).unwrap();
        let a_pos = yaml.find("APlugin").unwrap();
        let m_pos = yaml.find("MPlugin").unwrap();
        let z_pos = yaml.find("ZPlugin").unwrap();

        // Keys should appear in alphabetical order
        assert!(a_pos < m_pos);
        assert!(m_pos < z_pos);
    }

    #[test]
    fn test_save_atomic_overwrites_existing() {
        let temp_dir = tempfile::tempdir().unwrap();
        let state_path = temp_dir.path().join("state.yaml");

        // Save initial state
        let mut state1 = make_test_state();
        state1.units.insert(
            "Plugin1".to_string(),
            Unit::new("hash1".to_string(), "content1".to_string(), BTreeSet::new()),
        );
        save_atomic(&state_path, &mut state1).unwrap();
        assert!(state_path.exists());

        // Verify initial save
        let loaded1 = BuildState::load(&state_path).unwrap();
        assert_eq!(loaded1.units.len(), 1);
        assert!(loaded1.units.contains_key("Plugin1"));

        // Save again with different content (Windows-safe overwrite test)
        let mut state2 = make_test_state();
        state2.units.insert(
            "Plugin2".to_string(),
            Unit::new("hash2".to_string(), "content2".to_string(), BTreeSet::new()),
        );
        save_atomic(&state_path, &mut state2).unwrap();

        // Verify overwrite succeeded
        let loaded2 = BuildState::load(&state_path).unwrap();
        assert_eq!(loaded2.units.len(), 1);
        assert!(loaded2.units.contains_key("Plugin2"));
        assert!(!loaded2.units.contains_key("Plugin1"));
    }
}
