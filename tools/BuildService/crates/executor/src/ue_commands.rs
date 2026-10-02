//! Unreal Engine command generation (DLC workflow commands)

use anyhow::Result;
use std::path::{Path, PathBuf};

/// UE paths configuration
#[derive(Debug, Clone)]
pub struct UePaths {
    pub run_uat: PathBuf,
    pub unreal_editor_cmd: PathBuf,
    pub unreal_pak: PathBuf,
}

// ============================================================================
// DLC-Based Modular Build Commands
// ============================================================================
//
// The following commands implement the DLC-based plugin architecture where:
// 1. Base release is created once with sanitized .uproject (boot/core plugins only)
// 2. Project builds incrementally (compiles all modules regardless of .uproject)
// 3. Each plugin cooks as isolated DLC container referencing the base
//
// This prevents monolithic pakchunk0 and enables per-plugin CDN updates.
//
// Architecture documented in: tools/BuildService/docs/architecture/README.md
// ============================================================================

/// Generate base release creation command
///
/// Creates numbered base release using sanitized .uproject to prevent monolithic pakchunk0.
/// Only boot/core plugins (Orchestrator + marketplace dependencies) are enabled in sanitized
/// .uproject, ensuring project plugins cook as separate DLCs instead of merging into base.
///
/// # Base Release Concept
/// - Snapshot of game + orchestrator + core dependencies
/// - Identified by timestamp: `ALIS_YYYYMMDD_HHMMSS`
/// - Stored in `Saved/Releases/<Version>/`
/// - Reused across multiple plugin updates
/// - Only refreshed when core content/engine/toolchain changes
///
/// # Sanitization
/// Sanitized .uproject strips project plugins (e.g., ProjectMenuMain, ProjectWorld) while
/// keeping boot plugins (Orchestrator, DialoguePlugin, InstanceArrayTool, LowEntryExtStdLib).
/// This reduces plugins from ~26 to 4, preventing LNK1181 linker errors in Shipping builds.
///
/// Implementation: `tools/BuildService/crates/executor/src/sanitize.rs`
///
/// # Arguments
/// * `sanitized_uproject` - Path to temporary sanitized .uproject (auto-cleaned via RAII)
/// * `base_version` - Version ID (format: ALIS_YYYYMMDD_HHMMSS)
/// * `ue_paths` - UE toolchain paths (source-built engine from conf UE_SOURCE_PATH)
/// * `platform` - Target platform (Win64)
///
/// # Returns
/// BuildCookRun command string with `-createreleaseversion` parameter
///
/// # Workflow Position
/// Called once per CI run when base refresh needed (core changes, engine upgrade, or forced).
/// All subsequent DLC cooks reference this base via `-basedonreleaseversion`.
///
/// # Example Output
/// ```text
/// Saved/Releases/ALIS_20251120_190500/Windows/
///   Alis/Content/Paks/
///     pakchunk0-Windows.ucas (2.3 GB - base game content)
///     pakchunk0-Windows.utoc (1.5 MB - base game index)
///     global.ucas (2.4 MB - shared data)
/// ```
pub fn create_base_release_command(
    sanitized_uproject: &Path,
    base_version: &str,
    ue_paths: &UePaths,
    platform: &str,
) -> Result<String> {
    if !ue_paths.run_uat.exists() {
        return Err(anyhow::anyhow!(
            "RunUAT.bat not found at: {}\n\
            \n\
            DLC WORKFLOW REQUIREMENT:\n\
            - Must use SOURCE engine build (conf key UE_SOURCE_PATH)\n\
            - Installed (binary) engine lacks Shipping modular libraries\n\
            - Check scripts/config/ue_path.conf: UE_SOURCE_PATH",
            ue_paths.run_uat.display()
        ));
    }

    if !sanitized_uproject.exists() {
        return Err(anyhow::anyhow!(
            "Sanitized .uproject not found at: {}\n\
            \n\
            Base release requires sanitized project (with plugins stripped).\n\
            This is typically created automatically by the executor.\n\
            \n\
            If this error persists, verify:\n\
            - Project root path is correct in config\n\
            - Original .uproject exists and is readable\n\
            - Sanitization module is working correctly",
            sanitized_uproject.display()
        ));
    }

    // CRITICAL: Use Shipping config for all CDN artifacts
    // Binary (installed) engine lacks Shipping runtime libs, so source engine required
    let args = vec![
        format!("\"{}\"", ue_paths.run_uat.display()),
        "BuildCookRun".to_string(),
        format!("-project=\"{}\"", sanitized_uproject.display()),
        format!("-platform={}", platform),
        "-clientconfig=Shipping".to_string(),
        "-build".to_string(),
        "-cook".to_string(),
        "-stage".to_string(),
        "-pak".to_string(),
        "-iostore".to_string(),
        "-package".to_string(),  // Create distributable layout
        format!("-createreleaseversion={}", base_version),
        "-unversionedcookedcontent".to_string(),
        "-utf8output".to_string(),
        "-NoP4".to_string(),
    ];

    Ok(args.join(" "))
}

/// Generate incremental project build command
///
/// Compiles ALL plugin modules using UBT, regardless of .uproject plugin enablement.
/// This is run once per CI build before cooking DLCs, ensuring DLLs are up-to-date.
///
/// # Build Independence
/// UBT ignores .uproject plugin list and compiles all modules it finds in:
/// - Plugins/UI/ProjectMenuMain/Source/ProjectMenuMain/
/// - Plugins/World/ProjectWorld/Source/ProjectWorld/
/// - etc.
///
/// This allows plugins to be disabled in .uproject (for base release) while still
/// compiling their runtime DLLs for DLC distribution.
///
/// # Workflow Separation
/// - **Code changes:** Run this command (compiles DLLs, ~30-60s)
/// - **Content changes only:** Skip this, run DLC cook directly (reuses existing DLLs)
/// - **Both:** Run this first, then cook
///
/// # Arguments
/// * `project_path` - Path to main .uproject (NOT sanitized - uses original)
/// * `ue_paths` - UE toolchain paths
/// * `platform` - Target platform (Win64)
///
/// # Returns
/// BuildCookRun command with `-build -skipcook -skipstage` parameters
///
/// # Workflow Position
/// Called once per CI build after base decision, before per-plugin DLC cooks.
///
/// # Example Output
/// ```text
/// Plugins/UI/ProjectMenuMain/Binaries/Win64/
///   UnrealGame-ProjectMenuMain-Win64-Shipping.dll (164 KB)
///   UnrealGame-ProjectMenuMain-Win64-Shipping.pdb
/// Plugins/World/ProjectWorld/Binaries/Win64/
///   UnrealGame-ProjectWorld-Win64-Shipping.dll
///   ...
/// (22 plugin DLLs total in validated build)
/// ```
pub fn build_project_incremental_command(
    project_path: &Path,
    ue_paths: &UePaths,
    platform: &str,
) -> Result<String> {
    if !ue_paths.run_uat.exists() {
        return Err(anyhow::anyhow!(
            "RunUAT.bat not found at: {}",
            ue_paths.run_uat.display()
        ));
    }

    if !project_path.exists() {
        return Err(anyhow::anyhow!(
            "Project .uproject not found at: {}",
            project_path.display()
        ));
    }

    // Use Shipping for all CDN builds (consistency with base release)
    let args = vec![
        format!("\"{}\"", ue_paths.run_uat.display()),
        "BuildCookRun".to_string(),
        format!("-project=\"{}\"", project_path.display()),
        format!("-platform={}", platform),
        "-clientconfig=Shipping".to_string(),
        "-build".to_string(),
        "-skipcook".to_string(),
        "-skippak".to_string(),  // Don't create pak files during code-only build
        "-skipstage".to_string(),
        "-NoP4".to_string(),
        "-utf8output".to_string(),
        "-unattended".to_string(),
    ];

    Ok(args.join(" "))
}

/// Generate per-plugin DLC cook command
///
/// Cooks plugin as isolated DLC container referencing base release via `-basedonreleaseversion`.
/// Creates separate IoStore containers (`<Plugin>-Windows.utoc/.ucas`) that mount alongside base.
///
/// # DLC Container Isolation
/// Each DLC must contain ONLY assets under `/Plugins/<Plugin>/...` path.
/// Cross-plugin references are forbidden (creates dependency hell).
/// Use `UnrealPak -List` to verify isolation before CDN upload.
///
/// # Asset Cooking Strategy
/// UE5 only cooks assets referenced at runtime (Blueprint usage, C++ TSoftObjectPtr, etc.).
/// For unreferenced assets (test content, optional features), use one of:
///
/// 1. **`-CookDir` parameter (recommended):**
///    ```text
///    -CookDir="Plugins/UI/ProjectMenuMain/Content/test"
///    ```
///    Forces cooking of specific directory regardless of references.
///
/// 2. **`DirectoriesToAlwaysCook` in DefaultGame.ini:**
///    ```ini
///    [/Script/UnrealEd.ProjectPackagingSettings]
///    +DirectoriesToAlwaysCook=(Path="/ProjectMenuMain")
///    ```
///
/// 3. **`FSoftObjectPath` references in C++ (production):**
///    ```cpp
///    TArray<FSoftObjectPath> AssetsToPreload = {
///        FSoftObjectPath(TEXT("/ProjectMenuMain/test/M_TestContentMaterial"))
///    };
///    ```
///
/// ❌ **Anti-pattern:** Primary Asset Labels require Editor + .uproject enablement,
/// defeating the Orchestrator modular system.
///
/// # Arguments
/// * `plugin_name` - Plugin name (matches .uplugin filename, e.g., "ProjectMenuMain")
/// * `project_path` - Path to main .uproject (NOT sanitized)
/// * `base_version` - Base release version to reference (e.g., "ALIS_20251120_190500")
/// * `cook_dir` - Optional directory to force-cook (e.g., "Plugins/UI/ProjectMenuMain/Content")
/// * `ue_paths` - UE toolchain paths
/// * `platform` - Target platform (Win64)
///
/// # Returns
/// BuildCookRun command with `-DLCName` and `-basedonreleaseversion` parameters
///
/// # Workflow Position
/// Called once per changed plugin after incremental project build.
/// Staging must be cleaned between DLC cooks to prevent cross-contamination.
///
/// # Example Output
/// ```text
/// Saved/StagedBuilds/Windows/Alis/Content/Paks/
///   ProjectMenuMain-Windows.ucas (105 KB - 32.99% compressed)
///   ProjectMenuMain-Windows.utoc (464 B)
/// ```
///
/// # Validated Performance
/// - ProjectMenuMain with test asset: 27.78 seconds (1 package cooked)
/// - Container size: 105 KB (vs 2.3 GB base release)
pub fn cook_plugin_as_dlc_command(
    plugin_name: &str,
    project_path: &Path,
    base_version: &str,
    cook_dir: Option<&str>,
    ue_paths: &UePaths,
    platform: &str,
) -> Result<String> {
    if !ue_paths.run_uat.exists() {
        return Err(anyhow::anyhow!(
            "RunUAT.bat not found at: {}",
            ue_paths.run_uat.display()
        ));
    }

    if !project_path.exists() {
        return Err(anyhow::anyhow!(
            "Project .uproject not found at: {}",
            project_path.display()
        ));
    }

    if plugin_name.is_empty() {
        return Err(anyhow::anyhow!(
            "Plugin name cannot be empty\n\
            \n\
            DLC cooking requires a valid plugin name.\n\
            Check build order and ensure plugins are properly discovered."
        ));
    }

    if base_version.is_empty() {
        return Err(anyhow::anyhow!(
            "Base release version cannot be empty\n\
            \n\
            DLC cooking requires an existing base release version.\n\
            \n\
            TROUBLESHOOTING:\n\
            - First build should create base release automatically\n\
            - Check BuildState: build_state.yaml\n\
            - Verify base release was created successfully\n\
            - Force new base: delete build_state.yaml and rebuild"
        ));
    }

    let mut args = vec![
        format!("\"{}\"", ue_paths.run_uat.display()),
        "BuildCookRun".to_string(),
        format!("-project=\"{}\"", project_path.display()),
        format!("-platform={}", platform),
        "-clientconfig=Shipping".to_string(),
        "-skipbuild".to_string(),  // DLLs already compiled by incremental build
        "-cook".to_string(),
        "-stage".to_string(),
        "-pak".to_string(),
        "-iostore".to_string(),
        "-package".to_string(),  // Create distributable DLC layout
        format!("-DLCName={}", plugin_name),
        format!("-basedonreleaseversion={}", base_version),
        "-stagebasereleasepaks".to_string(),
        "-unversionedcookedcontent".to_string(),
        "-utf8output".to_string(),
        "-NoP4".to_string(),
    ];

    // Add optional -CookDir to force unreferenced assets to cook
    // Insert before -NoP4 to maintain command order
    if let Some(dir) = cook_dir {
        args.insert(args.len() - 2, format!("-CookDir=\"{}\"", dir));
    }

    Ok(args.join(" "))
}
