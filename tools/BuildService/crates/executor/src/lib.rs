//! UeBuildExecutor crate - DLC-based modular plugin builds
//!
//! Primary workflow: `build_plugins_as_dlc()`
//! - Create base release (sanitized project)
//! - Incremental DLL compilation
//! - Per-plugin DLC cooking with -CookDir
//! - IoStore container generation

mod process_executor;
mod ue_commands;
pub mod sanitize;
mod validation;
mod file_ops;
mod dll_staging;

pub use process_executor::{ProcessExecutor, ProcessExecutorConfig};
pub use ue_commands::UePaths;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};
use std::collections::HashSet;
use build_service_state::{BuildState, EnginePin, BaseReleasePin, BuiltUnit, ScanSnapshot};
use build_service_scanner::{ChangeScanner, ScannerConfig};

/// Build configuration
#[derive(Debug, Clone)]
pub struct BuildConfig {
    pub build: bool,
    pub platform: String,
    pub configuration: String,
    // Deprecated fields (kept for backward compat)
    pub cook: bool,
    pub stage: bool,
    pub package: bool,
}

impl Default for BuildConfig {
    fn default() -> Self {
        Self {
            build: false,
            platform: "Win64".to_string(),
            configuration: "Development".to_string(),
            cook: true,
            stage: false,  // No longer used
            package: false,  // No longer used
        }
    }
}

/// Build result from 3-phase pipeline / DLC workflow
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildResult {
    pub plugin_name: String,
    // Explicit phase outputs
    pub code_out_path: PathBuf,
    pub cook_out_path: PathBuf,
    pub iostore_out_path: PathBuf,
    // Legacy compatibility
    pub staged_path: PathBuf,
    pub success: bool,
    pub log_path: PathBuf,
    pub duration_secs: u64,
    // Base release version used for DLC cooking (None if not using DLC workflow)
    pub base_release_version: Option<String>,
}

/// Maps build platform names to UAT staging directory names
///
/// UE convention: Build tools use "Win64", but UAT creates "Windows" staging directories
/// This mapping ensures we look in the correct directory after BuildCookRun
fn platform_to_stage_dir(platform: &str) -> &str {
    match platform {
        "Win64" => "Windows",
        "Linux" => "Linux",
        "Mac" => "Mac",
        _ => platform,
    }
}

// ============================================================================
// Base Release Policy
// ============================================================================

/// Base release policy decision
///
/// Centralizes "new base vs reuse" logic for cleaner testing and observability.
#[derive(Debug, Clone, PartialEq, Eq)]
enum BasePolicy {
    /// Create new base release (forced or first run)
    CreateNew { reason: String },
    /// Reuse existing base release
    Reuse { version: String },
}

impl BasePolicy {
    /// Determine base policy from state and build context
    fn determine(
        existing_state: Option<&BuildState>,
        plugins_to_build: &[String],
        core_plugins: &HashSet<String>,
    ) -> Self {
        // First run: no state file
        if existing_state.is_none() {
            return BasePolicy::CreateNew {
                reason: "first run (no state file)".to_string(),
            };
        }

        let state = existing_state.unwrap();

        // No base release recorded (version empty or missing hash)
        if state.base_release.version.is_empty() || state.base_release.hash_hex.is_empty() {
            return BasePolicy::CreateNew {
                reason: "no base release recorded".to_string(),
            };
        }

        // Core plugin changed (requires base refresh)
        let core_changed = plugins_to_build.iter().any(|p| core_plugins.contains(p));
        if core_changed {
            return BasePolicy::CreateNew {
                reason: format!(
                    "core plugin changed: {}",
                    plugins_to_build
                        .iter()
                        .filter(|p| core_plugins.contains(*p))
                        .cloned()
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
            };
        }

        // Reuse existing base
        BasePolicy::Reuse {
            version: state.base_release.version.clone(),
        }
    }

    fn should_create_new(&self) -> bool {
        matches!(self, BasePolicy::CreateNew { .. })
    }

    fn reason(&self) -> String {
        match self {
            BasePolicy::CreateNew { reason } => reason.clone(),
            BasePolicy::Reuse { version } => format!("reusing base: {}", version),
        }
    }
}

/// UeBuildExecutor - Executes 3-phase plugin builds with DLC workflow support
pub struct UeBuildExecutor {
    ue_root: PathBuf,
    ue_paths: UePaths,
    project_path: PathBuf,
    repo_path: PathBuf,  // Repository root for dependency resolution
    plugin_build_root: PathBuf,
    #[allow(dead_code)]
    cook_root: PathBuf,
    #[allow(dead_code)]
    iostore_root: PathBuf,
    build_config: BuildConfig,
    stream_output: bool,
    build_state: BuildState,  // Tracks base releases and build history
    state_file_path: PathBuf,  // Path to persist BuildState YAML
}

impl UeBuildExecutor {
    /// Create executor with workspace configuration
    pub fn new(
        ue_root: PathBuf,
        project_path: PathBuf,
        workspace_root: PathBuf,
    ) -> Self {
        let ue_paths = UePaths {
            run_uat: ue_root.join("Engine/Build/BatchFiles/RunUAT.bat"),
            unreal_editor_cmd: ue_root.join("Engine/Binaries/Win64/UnrealEditor-Cmd.exe"),
            unreal_pak: ue_root.join("Engine/Binaries/Win64/UnrealPak.exe"),
        };

        let plugin_build_root = workspace_root.join("build");
        let cook_root = workspace_root.join("cook");
        let iostore_root = workspace_root.join("iostore");

        // Derive repo path from project_path (go up to find .git)
        let repo_path = find_repo_root(&project_path).unwrap_or_else(|| project_path.clone());

        // Initialize empty BuildState (caller should load from file)
        let build_state = BuildState::default();
        let state_file_path = workspace_root.join("state").join("build_state.yaml");

        Self {
            ue_root,
            ue_paths,
            project_path,
            repo_path,
            plugin_build_root,
            cook_root,
            iostore_root,
            build_config: BuildConfig::default(),
            stream_output: false,
            build_state,
            state_file_path,
        }
    }

    /// Create with custom config and UE paths
    ///
    /// BuildState is loaded internally by build_plugins_as_dlc().
    pub fn with_config(
        ue_paths: UePaths,
        ue_root: PathBuf,
        project_path: PathBuf,
        repo_path: PathBuf,
        plugin_build_root: PathBuf,
        cook_root: PathBuf,
        iostore_root: PathBuf,
        build_config: BuildConfig,
        stream_output: bool,
        state_file_path: PathBuf,
    ) -> Self {
        Self {
            ue_root,
            ue_paths,
            project_path,
            repo_path,
            plugin_build_root,
            cook_root,
            iostore_root,
            build_config,
            stream_output,
            build_state: BuildState::default(),  // Placeholder - loaded in build_plugins_as_dlc()
            state_file_path,
        }
    }

    // ========================================================================
    // PIN COMPUTATION METHODS
    // ========================================================================

    /// Compute EnginePin from UE installation (Enhanced for timestamp resilience)
    ///
    /// EnginePin = UE version + toolchain hash
    ///
    /// Enhanced strategy to avoid spurious rebuilds:
    /// 1. Parse Build.version JSON for all version fields
    /// 2. Hash Build.version JSON content (immune to file timestamp changes)
    /// 3. Optionally include UBT hash for deeper toolchain tracking
    ///
    /// This approach is more resilient than hashing UBT DLL alone, which can
    /// change on timestamp updates even when toolchain is functionally identical.
    fn compute_engine_pin(&self) -> Result<EnginePin> {
        // Extract UE version from Engine/Build/Build.version
        let version_file = self.ue_root.join("Engine/Build/Build.version");
        let (ue_version, version_json_content) = if version_file.exists() {
            let content = std::fs::read_to_string(&version_file)
                .with_context(|| format!("Failed to read Build.version: {}", version_file.display()))?;

            // Parse JSON to extract version
            let version_json: serde_json::Value = serde_json::from_str(&content)
                .with_context(|| "Failed to parse Build.version JSON")?;

            // Format: "MajorVersion.MinorVersion.PatchVersion"
            let major = version_json["MajorVersion"].as_i64().unwrap_or(5);
            let minor = version_json["MinorVersion"].as_i64().unwrap_or(5);
            let patch = version_json["PatchVersion"].as_i64().unwrap_or(0);

            (format!("{}.{}.{}", major, minor, patch), content)
        } else {
            tracing::warn!("Build.version not found, using default: 5.5.0");
            ("5.5.0".to_string(), String::new())
        };

        // Enhanced toolchain hash: Hash Build.version JSON content (timestamp-immune)
        // This captures UE version, branch, changelist, etc. without file timestamp noise
        use sha2::{Digest, Sha256};
        let mut hasher = Sha256::new();

        if !version_json_content.is_empty() {
            // Primary hash source: Build.version JSON (stable across timestamp changes)
            hasher.update(version_json_content.as_bytes());
            tracing::debug!("Engine pin using Build.version JSON hash (timestamp-resilient)");
        } else {
            // Fallback: hash UBT binary if Build.version unavailable
            let ubt_path = self.ue_root.join("Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll");
            if ubt_path.exists() {
                let ubt_bytes = std::fs::read(&ubt_path)
                    .with_context(|| format!("Failed to read UBT binary: {}", ubt_path.display()))?;
                hasher.update(&ubt_bytes);
                tracing::debug!("Engine pin using UBT binary hash (fallback)");
            } else {
                tracing::warn!("Neither Build.version nor UBT binary found, using placeholder hash");
                return Ok(EnginePin::new(ue_version, "placeholder".to_string()));
            }
        }

        let toolchain_hash = format!("{:x}", hasher.finalize());
        Ok(EnginePin::new(ue_version, toolchain_hash))
    }

    /// Compute BaseReleasePin from current base release
    ///
    /// BaseReleasePin = Base release version string (e.g., "ALIS_20250120_143022")
    /// Pin change triggers full rebuild (all units marked dirty)
    fn compute_base_release_pin(&self, base_version: &str) -> BaseReleasePin {
        BaseReleasePin::new(base_version.to_string())
    }

    /// Log staged DLL files for observability
    ///
    /// Scans staged Binaries directory and logs count + total size.
    /// Helps verify `-package` successfully staged runtime binaries.
    fn log_staged_dlls(&self, stage_path: &Path, context: &str) {
        let binaries_dir = stage_path.join("Binaries/Win64");

        if !binaries_dir.exists() {
            tracing::warn!("  [{}] No Binaries/Win64 directory found (expected after -package)", context);
            return;
        }

        let mut dll_count = 0;
        let mut total_size: u64 = 0;

        if let Ok(entries) = std::fs::read_dir(&binaries_dir) {
            for entry in entries.flatten() {
                if let Ok(metadata) = entry.metadata() {
                    if metadata.is_file() {
                        if let Some(ext) = entry.path().extension() {
                            if ext == "dll" {
                                dll_count += 1;
                                total_size += metadata.len();
                            }
                        }
                    }
                }
            }
        }

        if dll_count > 0 {
            let size_mb = total_size as f64 / 1024.0 / 1024.0;
            tracing::info!("  [{}] Staged {} DLL files ({:.2} MB total)", context, dll_count, size_mb);
        } else {
            tracing::warn!("  [{}] No DLL files found in Binaries/Win64", context);
        }
    }

    /// Compute base release hash from pakchunk0 IoStore files
    ///
    /// Hashes .pak (loose files), .utoc (metadata), and .ucas (bulk data)
    /// to detect ANY base release changes including config/video/localization files
    fn compute_base_hash(&self, base_version: &str) -> Result<String> {
        let base_paks_dir = self.project_path
            .join("Saved/Releases")
            .join(base_version)
            .join(platform_to_stage_dir(&self.build_config.platform))
            .join("Content/Paks");

        if !base_paks_dir.exists() {
            return Err(anyhow::anyhow!(
                "Base paks directory not found: {}\nBase release may not have been created successfully",
                base_paks_dir.display()
            ));
        }

        // Hash pakchunk0 files (pak + utoc + ucas)
        let platform_suffix = platform_to_stage_dir(&self.build_config.platform);
        let pak_file = base_paks_dir.join(format!("pakchunk0-{}.pak", platform_suffix));
        let utoc_file = base_paks_dir.join(format!("pakchunk0-{}.utoc", platform_suffix));
        let ucas_file = base_paks_dir.join(format!("pakchunk0-{}.ucas", platform_suffix));

        // All three files must exist
        if !pak_file.exists() || !utoc_file.exists() || !ucas_file.exists() {
            return Err(anyhow::anyhow!(
                "Base pakchunk0 files not found in: {}\nExpected: {}, {}, {}",
                base_paks_dir.display(),
                pak_file.display(),
                utoc_file.display(),
                ucas_file.display()
            ));
        }

        use sha2::{Digest, Sha256};
        let mut hasher = Sha256::new();

        // Hash all three files for complete base integrity
        // Order: pak, utoc, ucas (alphabetical for consistency)
        let pak_bytes = std::fs::read(&pak_file)
            .with_context(|| format!("Failed to read base pak: {}", pak_file.display()))?;
        let utoc_bytes = std::fs::read(&utoc_file)
            .with_context(|| format!("Failed to read base utoc: {}", utoc_file.display()))?;
        let ucas_bytes = std::fs::read(&ucas_file)
            .with_context(|| format!("Failed to read base ucas: {}", ucas_file.display()))?;

        hasher.update(&pak_bytes);
        hasher.update(&utoc_bytes);
        hasher.update(&ucas_bytes);

        // Return hex only (64 characters), no prefix
        Ok(format!("{:x}", hasher.finalize()))
    }

    /// Build plugins using DLC workflow (OWNS STATE LIFECYCLE)
    ///
    /// The executor owns the DLC workflow and BuildState lifecycle:
    /// a) Load buildState from file
    /// b) Determine if base refresh needed (core plugins changed?)
    /// c) If base refresh: sanitize .uproject, create base release, cook plugins as DLC
    ///    If reuse base: incremental build modules, cook plugins as DLC using existing base
    /// d) Save buildState with updated build history
    ///
    /// Architecture: See tools/BuildService/docs/architecture/README.md.
    pub fn build_plugins_as_dlc(
        &mut self,
        plugins: &[String],
        core_plugins: &HashSet<String>,
        dry_run: bool,
    ) -> Result<Vec<BuildResult>> {
        tracing::info!("=== DLC WORKFLOW START ===");
        tracing::info!("Plugins to build: {:?}", plugins);
        tracing::info!("Core plugins (trigger base refresh): {:?}", core_plugins);

        // Step 3a: Load BuildState from file; the executor owns its lifecycle.
        self.build_state = if self.state_file_path.exists() {
            BuildState::load(&self.state_file_path)
                .with_context(|| format!("Failed to load BuildState from {}", self.state_file_path.display()))?
        } else {
            tracing::info!("No existing BuildState file - creating new state");
            BuildState::default()
        };

        // Log current state (pin-based)
        let base_version_str = self.build_state.base_version();
        if !base_version_str.is_empty() && !self.build_state.base_hash().is_empty() {
            tracing::info!("Loaded BuildState: base_release={}", base_version_str);
        } else {
            tracing::info!("No existing base release - will create new one");
        }

        // Step 3b: Determine if base refresh needed (using BasePolicy)
        let base_policy = BasePolicy::determine(
            Some(&self.build_state),
            plugins,
            core_plugins,
        );
        tracing::info!("Base policy: {}", base_policy.reason());

        let base_version = if base_policy.should_create_new() {
            tracing::info!("Base refresh needed - core plugins changed or no existing base");

            if dry_run {
                tracing::info!("Dry run - skipping base release creation");
                "ALIS_DRY_RUN".to_string()
            } else {
                // Step 3c: Create new base release
                let new_base_version = format!("ALIS_{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"));
                tracing::info!("Creating new base release: {}", new_base_version);

                // CRITICAL: Verify Shipping toolchain before building
                self.assert_shipping_toolchain()?;

                // Sanitize .uproject (removes plugin references)
                let project_file = self.find_uproject_file()?;
                let sanitized_dir = self.plugin_build_root.join("sanitized");
                std::fs::create_dir_all(&sanitized_dir)?;
                let sanitized_path = sanitized_dir.join(project_file.file_name().unwrap());
                let mut stripped_plugins = sanitize::sanitize_uproject(
                    &self.project_path,
                    &project_file,
                    &sanitized_path,
                )?;
                // Sort for deterministic YAML diffs
                stripped_plugins.sort();
                tracing::info!("Created sanitized .uproject: {}", sanitized_path.display());
                tracing::info!("Stripped {} plugins: {:?}", stripped_plugins.len(), stripped_plugins);

                // Run base release build via BuildCookRun
                let command = ue_commands::create_base_release_command(
                    &sanitized_path,
                    &new_base_version,
                    &self.ue_paths,
                    &self.build_config.platform,
                )?;

                let log_path = self.get_phase_log_path("base_release", &new_base_version);
                let executor = self.make_executor();
                let success = executor.execute(&command, &log_path)?;

                if !success {
                    return Err(anyhow::anyhow!(
                        "Base release creation failed. Log: {}",
                        log_path.display()
                    ));
                }

                tracing::info!("[OK] Base release created: {}", new_base_version);

                // Compute base pakchunk0 hash from IoStore files
                let base_hash = self.compute_base_hash(&new_base_version)
                    .with_context(|| "Failed to compute base release hash")?;
                tracing::info!("  Base hash: {} (64-char hex)", &base_hash[..16]);

                // Update BuildState with new base release (pin-based)
                self.build_state.base_release = build_service_state::BaseReleasePin::with_metadata(
                    new_base_version.clone(),
                    base_hash,
                    stripped_plugins,
                );

                new_base_version
            }
        } else {
            tracing::info!("Reusing existing base release");

            if !dry_run {
                // Incremental build all modules (no content cook)
                tracing::info!("Running incremental module build...");
                let project_file = self.find_uproject_file()?;
                let command = ue_commands::build_project_incremental_command(
                    &project_file,
                    &self.ue_paths,
                    &self.build_config.platform,
                )?;

                let log_path = self.get_phase_log_path("incremental_build", "modules");
                let executor = self.make_executor();
                let success = executor.execute(&command, &log_path)?;

                if !success {
                    return Err(anyhow::anyhow!(
                        "Incremental module build failed. Log: {}",
                        log_path.display()
                    ));
                }

                tracing::info!("[OK] Incremental module build completed");
            }

            // Get version from pin (should always exist if we got here)
            let version = self.build_state.base_version();
            if version.is_empty() {
                "ALIS_UNKNOWN".to_string()
            } else {
                version.to_string()
            }
        };

        if dry_run {
            tracing::info!("Dry run complete - skipping DLC cooking");
            return Ok(Vec::new());
        }

        // Cook each plugin as DLC against the base release
        let mut results = Vec::new();
        for plugin_name in plugins {
            tracing::info!("Cooking plugin as DLC: {} (base: {})", plugin_name, base_version);

            let start_time = std::time::Instant::now();
            let project_file = self.find_uproject_file()?;

            // Find plugin directory and construct cook path
            // CRITICAL: Without -CookDir, unreferenced content won't cook
            let plugin_content_dir = self.find_plugin_content_dir(plugin_name)?;
            tracing::info!("Using -CookDir=\"{}\"", plugin_content_dir);

            let command = ue_commands::cook_plugin_as_dlc_command(
                plugin_name,
                &project_file,
                &base_version,
                Some(&plugin_content_dir),  // Force cook all plugin content
                &self.ue_paths,
                &self.build_config.platform,
            )?;

            let log_path = self.get_phase_log_path(plugin_name, &format!("dlc_{}", base_version));
            let executor = self.make_executor();
            let success = executor.execute(&command, &log_path)?;

            if !success {
                return Err(anyhow::anyhow!(
                    "DLC cooking failed for {}. Log: {}",
                    plugin_name,
                    log_path.display()
                ));
            }

            let duration = start_time.elapsed();
            tracing::info!("[OK] DLC cooked: {} in {}s", plugin_name, duration.as_secs());

            // Construct result (paths point to DLC staging directory)
            let dlc_staging = self.project_path.join("Saved/StagedBuilds")
                .join(platform_to_stage_dir(&self.build_config.platform));

            // CRITICAL: Verify DLLs staged (copy from source if missing)
            // BuildCookRun with -skipbuild may not stage binaries reliably
            let dll_count = dll_staging::verify_and_stage_dlls(
                plugin_name,
                &dlc_staging,
                &self.project_path,
            )?;
            tracing::info!("  DLL staging verified: {} DLLs for code.zip", dll_count);

            let dlc_staging = dlc_staging.join(plugin_name);

            // Validate DLC isolation (Phase 5 requirement)
            // Ensures container only contains plugin assets (no cross-contamination)
            let iostore_path = dlc_staging.join("Content/Paks");
            if let Err(e) = self.validate_dlc_isolation(plugin_name, &iostore_path) {
                tracing::error!("DLC isolation validation failed: {}", e);
                return Err(e);
            }

            results.push(BuildResult {
                plugin_name: plugin_name.clone(),
                code_out_path: dlc_staging.clone(),  // Staged plugin root (contains .uplugin, Binaries/, Config/, Resources/)
                cook_out_path: dlc_staging.join("Content"),
                iostore_out_path: dlc_staging.join("Content/Paks"),
                staged_path: dlc_staging,
                success: true,
                log_path,
                duration_secs: duration.as_secs(),
                base_release_version: Some(base_version.clone()),  // DLC workflow - track base version
            });
        }

        // Step 3d: Save buildState with updated build history
        if let Err(e) = self.save_state() {
            tracing::warn!("Failed to save BuildState: {}", e);
        } else {
            tracing::info!("BuildState saved: {}", self.state_file_path.display());
        }

        tracing::info!("=== DLC WORKFLOW COMPLETE ===");
        tracing::info!("Base release: {}", base_version);
        tracing::info!("Plugins built: {}", results.len());

        Ok(results)
    }

    /// Build plugins with pin-based state tracking (NEW ARCHITECTURE)
    ///
    /// Implements Scanner → State → Executor workflow:
    /// 1. Scan all plugins (code/content hashes + dependencies)
    /// 2. Compute current pins (engine + base release)
    /// 3. Load or initialize state with pins
    /// 4. Mark dirty units based on pin/hash changes
    /// 5. Build only dirty units
    /// 6. Apply build results to clear dirty flags
    /// 7. Save state atomically
    ///
    /// # Arguments
    /// * `plugins` - List of plugin names to consider for building
    /// * `core_plugins` - Set of core plugins that trigger base refresh
    /// * `dry_run` - If true, skip actual builds (just compute what would be built)
    ///
    /// # Returns
    /// Vec of BuildResult for each plugin that was built
    pub fn build_plugins_with_pin_tracking(
        &mut self,
        plugins: &[String],
        core_plugins: &HashSet<String>,
        dry_run: bool,
    ) -> Result<Vec<BuildResult>> {
        tracing::info!("=== PIN-BASED DLC WORKFLOW START ===");
        tracing::info!("Plugins to consider: {:?}", plugins);
        tracing::info!("Core plugins (trigger base refresh): {:?}", core_plugins);

        // Step 1: Scan all plugins for hashes and dependencies
        tracing::info!("Step 1: Scanning filesystem...");
        let scanner = ChangeScanner::new(ScannerConfig {
            repo_path: self.repo_path.clone(),
        })?;
        let scanner_snapshot = scanner.scan_all()?;
        tracing::info!("  Scanned {} units", scanner_snapshot.units.len());

        // Keep scanner snapshot for hash persistence later
        let scanner_snapshot_for_hashes = scanner_snapshot.clone();

        // Convert scanner snapshot to state snapshot
        let mut state_units = std::collections::BTreeMap::new();
        for (name, unit) in scanner_snapshot.units {
            state_units.insert(
                name,
                build_service_state::ScannedUnit {
                    code_hash: unit.code_hash,
                    content_hash: unit.content_hash,
                    deps: unit.deps,
                },
            );
        }
        let state_snapshot = ScanSnapshot {
            units: state_units,
        };

        // Step 2: Compute current pins
        tracing::info!("Step 2: Computing pins...");
        let engine_pin = self.compute_engine_pin()?;
        tracing::info!("  Engine: {} (toolchain: {}...)",
            engine_pin.ue_version,
            &engine_pin.toolchain_hash[..8]);

        // Determine base policy (new vs reuse)
        let existing_state_opt = BuildState::load(&self.state_file_path).ok();
        let base_policy = BasePolicy::determine(
            existing_state_opt.as_ref(),
            plugins,
            core_plugins,
        );

        tracing::info!("  Base policy: {}", base_policy.reason());

        let base_version = match &base_policy {
            BasePolicy::CreateNew { .. } => {
                format!("ALIS_{}", chrono::Utc::now().format("%Y%m%d_%H%M%S"))
            }
            BasePolicy::Reuse { version } => version.clone(),
        };

        let base_pin = self.compute_base_release_pin(&base_version);
        tracing::info!("  Base: {}", base_pin.version);

        // Step 3: Load or initialize state with pins
        tracing::info!("Step 3: Loading state...");
        self.build_state = BuildState::load_or_init(
            &self.state_file_path,
            engine_pin.clone(),
            base_pin.clone(),
            &self.build_config.platform,
        )?;

        // Step 4: Mark dirty units based on pin/hash changes
        tracing::info!("Step 4: Marking dirty units...");
        let dirty_count = build_service_state::mark_dirty_from_scan(
            &mut self.build_state,
            state_snapshot,
            &engine_pin,
            &base_pin,
        )?;

        // Get list of dirty unit names
        let dirty_units: Vec<String> = self.build_state.units
            .iter()
            .filter(|(_, unit)| unit.dirty)
            .map(|(name, _)| name.clone())
            .collect();

        tracing::info!("  {} dirty units: {:?}", dirty_count, dirty_units);

        if dirty_units.is_empty() {
            tracing::info!("No dirty units - all plugins up to date");
            return Ok(Vec::new());
        }

        // Filter plugins list to only dirty ones
        let plugins_to_build: Vec<String> = plugins
            .iter()
            .filter(|p| dirty_units.contains(p))
            .cloned()
            .collect();

        if plugins_to_build.is_empty() {
            tracing::info!("No requested plugins are dirty");
            return Ok(Vec::new());
        }

        tracing::info!("Building {} dirty plugins: {:?}", plugins_to_build.len(), plugins_to_build);

        if dry_run {
            tracing::info!("Dry run - skipping builds");
            return Ok(Vec::new());
        }

        // Step 5: Build dirty plugins (reuse existing DLC build logic)
        let mut results = Vec::new();

        // Execute base policy decision
        if base_policy.should_create_new() {
            tracing::info!("Creating new base release: {}", base_version);

            // CRITICAL: Verify Shipping toolchain before building
            self.assert_shipping_toolchain()?;

            // Sanitize .uproject (removes plugin references)
            let project_file = self.find_uproject_file()?;
            let sanitized_dir = self.plugin_build_root.join("sanitized");
            std::fs::create_dir_all(&sanitized_dir)?;
            let sanitized_path = sanitized_dir.join(project_file.file_name().unwrap());
            let mut stripped_plugins = sanitize::sanitize_uproject(
                &self.project_path,
                &project_file,
                &sanitized_path,
            )?;
            // Sort for deterministic YAML diffs
            stripped_plugins.sort();
            tracing::info!("Created sanitized .uproject: {}", sanitized_path.display());
            tracing::info!("Stripped {} plugins: {:?}", stripped_plugins.len(), stripped_plugins);

            // Run base release build via BuildCookRun
            let command = ue_commands::create_base_release_command(
                &sanitized_path,
                &base_version,
                &self.ue_paths,
                &self.build_config.platform,
            )?;

            let log_path = self.get_phase_log_path("base_release", &base_version);
            let executor = self.make_executor();
            let success = executor.execute(&command, &log_path)?;

            if !success {
                return Err(anyhow::anyhow!(
                    "Base release creation failed. Log: {}",
                    log_path.display()
                ));
            }

            tracing::info!("[OK] Base release created: {}", base_version);

            // Log staged DLLs for observability
            let base_stage_path = self.project_path
                .join("Saved/Releases")
                .join(&base_version)
                .join(platform_to_stage_dir(&self.build_config.platform));
            self.log_staged_dlls(&base_stage_path, "base release");

            // Compute base pakchunk0 hash from IoStore files
            let base_hash = self.compute_base_hash(&base_version)
                .with_context(|| "Failed to compute base release hash")?;
            tracing::info!("  Base hash: {} (64-char hex)", &base_hash[..16]);

            // Update BuildState with new base release (pin-based)
            self.build_state.base_release = build_service_state::BaseReleasePin::with_metadata(
                base_version.clone(),
                base_hash,
                stripped_plugins,
            );
        } else {
            tracing::info!("Reusing existing base release");

            // Incremental build all modules (no content cook)
            tracing::info!("Running incremental module build...");
            let project_file = self.find_uproject_file()?;
            let command = ue_commands::build_project_incremental_command(
                &project_file,
                &self.ue_paths,
                &self.build_config.platform,
            )?;

            let log_path = self.get_phase_log_path("incremental_build", "modules");
            let executor = self.make_executor();
            let success = executor.execute(&command, &log_path)?;

            if !success {
                return Err(anyhow::anyhow!(
                    "Incremental module build failed. Log: {}",
                    log_path.display()
                ));
            }

            tracing::info!("[OK] Incremental module build completed");
        }

        // Cook each dirty plugin as DLC
        let mut built_units = Vec::new();

        for plugin_name in &plugins_to_build {
            tracing::info!("Cooking plugin as DLC: {} (base: {})", plugin_name, base_version);

            let start_time = std::time::Instant::now();
            let project_file = self.find_uproject_file()?;

            // Find plugin directory and construct cook path
            let plugin_content_dir = self.find_plugin_content_dir(plugin_name)?;
            tracing::info!("Using -CookDir=\"{}\"", plugin_content_dir);

            let command = ue_commands::cook_plugin_as_dlc_command(
                plugin_name,
                &project_file,
                &base_version,
                Some(&plugin_content_dir),
                &self.ue_paths,
                &self.build_config.platform,
            )?;

            let log_path = self.get_phase_log_path(plugin_name, &format!("dlc_{}", base_version));
            let executor = self.make_executor();
            let success = executor.execute(&command, &log_path)?;

            if !success {
                tracing::error!("DLC cooking failed for {}", plugin_name);
                // Keep dirty flag for failed builds
                continue;
            }

            let duration = start_time.elapsed();
            tracing::info!("[OK] DLC cooked: {} in {}s", plugin_name, duration.as_secs());

            // Construct result (paths point to DLC staging directory)
            let dlc_staging = self.project_path.join("Saved/StagedBuilds")
                .join(platform_to_stage_dir(&self.build_config.platform));

            // Verify DLLs staged
            let dll_count = dll_staging::verify_and_stage_dlls(
                plugin_name,
                &dlc_staging,
                &self.project_path,
            )?;
            tracing::info!("  DLL staging verified: {} DLLs for code.zip", dll_count);

            // Log detailed DLL staging info (count + size)
            let dlc_staging_plugin = dlc_staging.join(plugin_name);
            self.log_staged_dlls(&dlc_staging_plugin, &format!("DLC: {}", plugin_name));

            let dlc_staging = dlc_staging_plugin;

            // Validate DLC isolation
            let iostore_path = dlc_staging.join("Content/Paks");
            if let Err(e) = self.validate_dlc_isolation(plugin_name, &iostore_path) {
                tracing::error!("DLC isolation validation failed: {}", e);
                continue;
            }

            // Step 6: Track successful build (will clear dirty flag)
            // Use pre-build hashes from scanner snapshot
            let (code_hash, content_hash) = if let Some(scanned) = scanner_snapshot_for_hashes.units.get(plugin_name) {
                (Some(scanned.code_hash.clone()), Some(scanned.content_hash.clone()))
            } else {
                (None, None)
            };

            built_units.push(BuiltUnit {
                plugin_name: plugin_name.clone(),
                published_version: base_version.clone(),
                code_hash,
                content_hash,
            });

            results.push(BuildResult {
                plugin_name: plugin_name.clone(),
                code_out_path: dlc_staging.clone(),
                cook_out_path: dlc_staging.join("Content"),
                iostore_out_path: dlc_staging.join("Content/Paks"),
                staged_path: dlc_staging,
                success: true,
                log_path,
                duration_secs: duration.as_secs(),
                base_release_version: Some(base_version.clone()),
            });
        }

        // Apply all successful build results (clears dirty flags)
        if !built_units.is_empty() {
            let cleared_count = build_service_state::apply_build_result(&mut self.build_state, built_units)?;
            tracing::info!("  Cleared dirty flags for {} units", cleared_count);
        }

        // Step 7: Save state atomically
        tracing::info!("Step 7: Saving state...");
        build_service_state::save_atomic(&self.state_file_path, &mut self.build_state)?;
        tracing::info!("  State saved: {}", self.state_file_path.display());

        tracing::info!("=== PIN-BASED DLC WORKFLOW COMPLETE ===");
        tracing::info!("Base release: {}", base_version);
        tracing::info!("Plugins built: {} / {} dirty", results.len(), plugins_to_build.len());

        Ok(results)
    }

    /// Save BuildState to YAML file
    pub fn save_state(&mut self) -> Result<()> {
        // Ensure state directory exists
        if let Some(parent) = self.state_file_path.parent() {
            std::fs::create_dir_all(parent)?;
        }

        build_service_state::save_atomic(&self.state_file_path, &mut self.build_state)?;
        tracing::debug!("BuildState saved to: {}", self.state_file_path.display());
        Ok(())
    }

    /// Get current base release version (if any)
    pub fn get_base_release_version(&self) -> Option<String> {
        let version = self.build_state.base_version();
        if version.is_empty() {
            None
        } else {
            Some(version.to_string())
        }
    }

    /// Find .uproject file (delegates to file_ops module)
    fn find_uproject_file(&self) -> Result<PathBuf> {
        file_ops::find_uproject_file(&self.project_path)
    }

    /// Find plugin Content directory for -CookDir parameter (delegates to file_ops module)
    fn find_plugin_content_dir(&self, plugin_name: &str) -> Result<String> {
        file_ops::find_plugin_content_dir(&self.project_path, plugin_name)
    }

    /// Validate DLC container isolation (delegates to validation module)
    fn validate_dlc_isolation(&self, plugin_name: &str, iostore_path: &Path) -> Result<()> {
        validation::validate_dlc_isolation(plugin_name, iostore_path, &self.ue_paths.unreal_pak)
    }

    fn get_phase_log_path(&self, plugin_name: &str, phase: &str) -> PathBuf {
        self.project_path
            .join("Saved")
            .join("Logs")
            .join(format!("Build_{}_{}_{}.log", plugin_name, phase, chrono::Utc::now().format("%Y%m%d_%H%M%S")))
    }

    fn make_executor(&self) -> ProcessExecutor {
        ProcessExecutor::with_stream_output(self.stream_output)
    }

    /// Verify Shipping engine toolchain exists (preflight check)
    ///
    /// CRITICAL: Modular Shipping builds require Shipping runtime libraries.
    /// Binary (installed) engines don't ship these - must use source engine build
    /// or precompile Shipping target.
    ///
    /// Probes for: Intermediate/Build/Win64/.../Shipping/*.lib
    ///
    /// # Returns
    /// * Ok(()) if Shipping artifacts found
    /// * Err(...) with remediation steps if missing
    fn assert_shipping_toolchain(&self) -> Result<()> {
        // Probe common Shipping library locations
        let probe_paths = [
            self.project_path.join("Intermediate/Build/Win64/x64/UnrealGame/Shipping/Core"),
            self.project_path.join("Intermediate/Build/Win64/UnrealGame/Shipping/Core"),
            self.ue_root.join("Engine/Intermediate/Build/Win64/UnrealEditor/Shipping"),
        ];

        let mut found_libs = false;
        for probe in &probe_paths {
            if probe.exists() {
                // Check if directory contains any .lib files
                if let Ok(entries) = std::fs::read_dir(probe) {
                    for entry in entries.flatten() {
                        if entry.path().extension().and_then(|s| s.to_str()) == Some("lib") {
                            found_libs = true;
                            tracing::debug!("Found Shipping libs in: {}", probe.display());
                            break;
                        }
                    }
                }
            }
            if found_libs {
                break;
            }
        }

        if !found_libs {
            let project_file = self.find_uproject_file()?;
            return Err(anyhow::anyhow!(
                "Shipping engine artifacts not found.\n\
                \n\
                CRITICAL REQUIREMENT FOR DLC WORKFLOW:\n\
                Modular Shipping builds require Shipping runtime libraries.\n\
                Binary (installed) engines don't include these.\n\
                \n\
                SOLUTION (choose one):\n\
                \n\
                1. Use source-built engine (recommended):\n\
                   - Clone UE from GitHub/Epic\n\
                   - Run Setup.bat && GenerateProjectFiles.bat\n\
                   - Build in Shipping configuration\n\
                   - Declare it in scripts/config/ue_path.conf: UE_SOURCE_PATH=<root>\n\
                \n\
                2. Precompile Shipping target:\n\
                   Engine\\Build\\BatchFiles\\RunUAT.bat BuildTarget ^\n\
                     -Target=UnrealGame -Platform=Win64 -Configuration=Shipping ^\n\
                     -Project=\"{}\" -Precompile\n\
                \n\
                3. Build Installed Engine with Shipping:\n\
                   Engine\\Build\\BatchFiles\\RunUAT.bat BuildGraph ^\n\
                     -Script=\"Engine/Build/InstalledEngineBuild.xml\" ^\n\
                     -Target=\"Make Installed Build Win64\" ^\n\
                     -set:GameConfigurations=Shipping ^\n\
                     -set:WithWin64=true -set:HostPlatformOnly=true\n\
                \n\
                Probed paths:\n{}",
                project_file.display(),
                probe_paths.iter()
                    .map(|p| format!("  - {}", p.display()))
                    .collect::<Vec<_>>()
                    .join("\n")
            ));
        }

        tracing::info!("[OK] Shipping toolchain verified");
        Ok(())
    }

}

/// Find repository root by looking for .git directory (delegates to file_ops module)
fn find_repo_root(start_path: &Path) -> Option<PathBuf> {
    file_ops::find_repo_root(start_path)
}
