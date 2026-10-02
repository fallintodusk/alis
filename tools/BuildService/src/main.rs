//! Build Service CLI - Orchestrates the build-package-publish pipeline
//!
//! Pipeline flow:
//! 1. ChangeScanner - Detect changed plugins
//! 2. OrderPlanner - Sort by dependencies
//! 3. UeBuildExecutor - Build via RunUAT
//! 4. ArtifactPackager - Package and hash artifacts
//! 5. ManifestStore - Update manifest.json
//! 6. CdnPublisher - Upload to CDN and promote

use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use serde::Deserialize;
use std::path::PathBuf;
use walkdir::WalkDir;
use std::time::Instant;

// Crate imports
use build_service_scanner::{BuildUnitBuilder, BuildUnitDescriptor, ChangeScanner, ScannerConfig};
use build_service_order_planner::{OrderPlanner, PlannerConfig};
use build_service_executor::{UeBuildExecutor, BuildConfig as ExecutorBuildConfig, UePaths};
use build_service_packager::ArtifactPackager;
use build_service_manifest::{ManifestStore, Manifest, PluginManifestEntry, Artifact};
use build_service_publisher::{CdnPublisher, PublisherConfig};

/// Build Service CLI - Automates plugin builds and CDN publishing
#[derive(Parser)]
#[command(name = "build_service")]
#[command(about = "Build Service CLI for Unreal Engine plugin builds", long_about = None)]
#[command(version)]
struct Cli {
    /// Configuration file path
    #[arg(short, long, default_value = "config/build_service.toml")]
    config: PathBuf,

    /// Verbose logging
    #[arg(short, long)]
    verbose: bool,

    #[command(subcommand)]
    command: Commands,
}

/// Build Service configuration (from TOML file)
///
/// NO engine paths here - ONE SOT: the source engine root resolves from
/// <project_root>/scripts/config/ue_path.conf (UE_SOURCE_PATH) via
/// engine_config; process env UE_SOURCE_PATH is the only override.
#[derive(Debug, Clone, Deserialize)]
struct BuildServiceConfig {
    paths: PathsConfig,
    cdn: CdnConfig,
    build: BuildConfig,
}

#[derive(Debug, Clone, Deserialize)]
struct PathsConfig {
    project_root: PathBuf,
    staging_dir: PathBuf,
    manifest_file: PathBuf,
    plugin_build_root: PathBuf,
    cook_root: PathBuf,
    iostore_root: PathBuf,
    build_state_file: PathBuf,
}

/// Resolve the source engine root: process env override > repo conf.
fn resolve_ue_root(project_root: &std::path::Path) -> Result<PathBuf> {
    if let Some(v) = std::env::var_os("UE_SOURCE_PATH") {
        let p = PathBuf::from(v);
        tracing::info!("UE_SOURCE_PATH env override in effect: {}", p.display());
        return Ok(p);
    }
    engine_config::resolve_source_engine_root(project_root)
}

#[derive(Debug, Clone, Deserialize)]
struct CdnConfig {
    control_api_url: String,
    control_api_token_env: Option<String>,
}

#[derive(Debug, Clone, Deserialize)]
struct BuildConfig {
    platform: String,
    configuration: String,
    channel: String,
    parallel_builds: usize,
}

#[derive(Subcommand)]
enum Commands {
    /// Build plugins (defaults to changed only, use --force to rebuild all)
    Build {
        /// Force rebuild all plugins even when YAML hashes unchanged (use when build environment changed)
        #[arg(long)]
        force: bool,

        /// Build specific plugin by name
        #[arg(long)]
        plugin: Option<String>,

        /// Dry run (validate without executing builds)
        #[arg(long)]
        dry_run: bool,

        /// Build ID (defaults to timestamp)
        #[arg(long)]
        build_id: Option<String>,

        /// Channel (dev, test, prod)
        #[arg(long, default_value = "dev")]
        channel: String,

        /// Stream UE output to console in real time
        #[arg(long)]
        stream: bool,
    },

    /// Publish artifacts to CDN
    Publish {
        /// Build ID to publish
        #[arg(long)]
        build_id: String,

        /// Upload to staging (do not promote)
        #[arg(long)]
        stage: bool,

        /// Promote to production (after staging)
        #[arg(long)]
        promote: bool,

        /// Rollback to previous release
        #[arg(long)]
        rollback: bool,
    },

    /// Watch build branch for changes (auto-build)
    Watch {
        /// Branch to monitor (default: build)
        #[arg(long, default_value = "build")]
        branch: String,

        /// Poll interval in seconds
        #[arg(long, default_value = "60")]
        interval: u64,
    },
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<()> {
    let cli = Cli::parse();

    // Initialize logging
    init_logging(cli.verbose)?;

    // Load configuration
    let config_content = std::fs::read_to_string(&cli.config)
        .with_context(|| format!("Failed to read config file: {}", cli.config.display()))?;
    let config: BuildServiceConfig = toml::from_str(&config_content)
        .with_context(|| "Failed to parse config file")?;

    let ue_root = resolve_ue_root(&config.paths.project_root)?;

    tracing::info!("Build Service CLI started");
    tracing::info!("Config: {}", cli.config.display());
    tracing::info!("UE Root (from conf SOT): {}", ue_root.display());
    tracing::info!("Project Root: {}", config.paths.project_root.display());
    tracing::info!("Parallel build slots: {}", config.build.parallel_builds);

    match cli.command {
        Commands::Build {
            force,
            plugin,
            dry_run,
            build_id,
            channel,
            stream,
        } => {
            handle_build(&config, &ue_root, force, plugin, dry_run, build_id, channel, stream).await?;
        }
        Commands::Publish {
            build_id,
            stage,
            promote,
            rollback,
        } => {
            handle_publish(&config, build_id, stage, promote, rollback).await?;
        }
        Commands::Watch { branch, interval } => {
            handle_watch(&config, &ue_root, branch, interval).await?;
        }
    }

    tracing::info!("Build Service CLI completed");
    Ok(())
}

/// Initialize logging based on verbosity
fn init_logging(verbose: bool) -> Result<()> {
    let level = if verbose { "debug" } else { "info" };

    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new(level)),
        )
        .init();

    Ok(())
}

/// Handle build command
async fn handle_build(
    config: &BuildServiceConfig,
    ue_root: &std::path::Path,
    force: bool,
    plugin: Option<String>,
    dry_run: bool,
    build_id: Option<String>,
    channel: String,
    stream_output: bool,
) -> Result<()> {
    tracing::info!("=== BUILD COMMAND ===");

    // Engine-identity gate BEFORE any expensive work: the source engine's
    // Major.Minor must match the project association (code-enforced
    // source-release freeze; see docs/ue_engine/version_update.md).
    let uproject = config.paths.project_root.join("Alis.uproject");
    let source_version = engine_config::check_source_engine_identity(ue_root, &uproject)?;
    tracing::info!(
        "Source engine identity OK: {}.{}.{}",
        source_version.major, source_version.minor, source_version.patch
    );
    tracing::info!("Force: {}", force);
    tracing::info!("Plugin: {:?}", plugin);
    tracing::info!("Dry run: {}", dry_run);
    tracing::info!("Build ID: {:?}", build_id);
    tracing::info!("Channel: {}", channel);
    tracing::info!("Stream output: {}", stream_output);

    // Generate build ID if not provided
    let build_id = build_id.unwrap_or_else(|| {
        chrono::Utc::now().format("%Y%m%d_%H%M%S").to_string()
    });
    tracing::info!("Using build ID: {}", build_id);

    // Step 0: Ensure BuildUnit descriptors exist for project + pending plugins
    let descriptor_builder = BuildUnitBuilder::new(config.paths.project_root.clone())
        .context("Failed to initialize BuildUnit builder")?;
    let project_name = descriptor_builder.project_name().to_string();
    descriptor_builder.ensure_project()?;

    // Ensure project descriptor exists even for single-plugin builds
    descriptor_builder.ensure_project()?;

    // Step 1: ChangeScanner - Detect changed plugins (descriptor hashes)
    let mut plugins_to_build = if let Some(plugin_name) = plugin {
        tracing::info!("Building specific plugin: {}", plugin_name);
        descriptor_builder.ensure_for_plugins(&[plugin_name.clone()])?;
        vec![plugin_name]
    } else if force {
        // Force rebuild ALL plugins (ignores YAML change detection)
        tracing::info!("Force rebuild mode - building all plugins");
        descriptor_builder.ensure_all()?;
        let mut all_plugins = descriptor_builder.plugin_mapper().get_all_plugins();
        tracing::info!("Found {} plugins: {:?}", all_plugins.len(), all_plugins);

        // Force mode must also rebuild the root project
        all_plugins.retain(|p| p != &project_name);
        let mut units = Vec::with_capacity(all_plugins.len() + 1);
        units.push(project_name.clone());
        units.extend(all_plugins);
        units
    } else {
        // Default: build only changed plugins
        tracing::info!("Ensuring BuildUnit descriptors exist before scan...");
        descriptor_builder.ensure_all()?;
        tracing::info!("Scanning for changed plugins...");
        let scanner_config = ScannerConfig {
            repo_path: config.paths.project_root.clone(),
        };
        let scanner = ChangeScanner::new(scanner_config)?;
        let changed_set = scanner.scan_changed()?;
        tracing::info!(
            "Found {} changed plugins: {:?}",
            changed_set.plugins.len(),
            changed_set.plugins
        );
        changed_set.plugins
    };

    // Ensure project is included when explicitly requested (force) or already present
    if !plugins_to_build.contains(&project_name) && force {
        plugins_to_build.insert(0, project_name.clone());
    }

    if plugins_to_build.is_empty() {
        tracing::info!("No plugins to build");
        return Ok(());
    }

    // Step 2: OrderPlanner - Sort by dependencies (project first if included)
    tracing::info!("Determining build order...");
    let planner_config = PlannerConfig {
        repo_path: config.paths.project_root.clone(),
    };
    let planner = OrderPlanner::new(planner_config)?;
    let mut build_order = planner.order(&plugins_to_build)?;
    if let Some(pos) = build_order.iter().position(|p| p == &project_name) {
        if pos != 0 {
            let project_entry = build_order.remove(pos);
            build_order.insert(0, project_entry);
        }
    }
    tracing::info!("Build order (topologically sorted): {:?}", build_order);

    // Step 3: UeBuildExecutor - Pin-based DLC workflow with integrated state tracking
    // Architecture: Scanner → State → Executor
    // Executor handles pin computation, state loading, dirty tracking, and atomic saves
    tracing::info!("Starting UE builds with pin-based state tracking...");
    let executor_build_config = ExecutorBuildConfig {
        build: false,
        platform: config.build.platform.clone(),
        configuration: config.build.configuration.clone(),
        cook: true,
        stage: false,   // No longer used
        package: false, // No longer used
    };

    // Tool paths derive from the resolved source engine root - the
    // committed absolute paths were redundant copies of the root.
    let ue_paths = UePaths {
        run_uat: ue_root.join("Engine/Build/BatchFiles/RunUAT.bat"),
        unreal_editor_cmd: ue_root.join("Engine/Binaries/Win64/UnrealEditor-Cmd.exe"),
        unreal_pak: ue_root.join("Engine/Binaries/Win64/UnrealPak.exe"),
    };

    let mut executor = UeBuildExecutor::with_config(
        ue_paths,
        ue_root.to_path_buf(),
        config.paths.project_root.clone(),
        config.paths.project_root.clone(),  // repo_path (same as project_root)
        config.paths.plugin_build_root.clone(),
        config.paths.cook_root.clone(),
        config.paths.iostore_root.clone(),
        executor_build_config,
        stream_output,
        config.paths.build_state_file.clone(),
    );

    // Define core plugins (when these change, we need a new base release)
    // TODO: Make this configurable via config file
    let core_plugins: std::collections::HashSet<String> = [
        "ProjectCore",
        "ProjectData",
        "ProjectBoot",
    ].iter().map(|s| s.to_string()).collect();

    tracing::info!("Starting DLC workflow...");
    tracing::info!("Core plugins: {:?}", core_plugins);

    let build_phase_start = Instant::now();

    // Filter out project from plugins_to_build (it's handled as part of base release)
    let plugins_only: Vec<String> = plugins_to_build
        .iter()
        .filter(|p| *p != &project_name)
        .cloned()
        .collect();

    let build_results = executor.build_plugins_with_pin_tracking(
        &plugins_only,
        &core_plugins,
        dry_run,
    )?;

    let total_units = build_results.len();

    if dry_run {
        tracing::info!("Dry run complete - skipping packaging and manifest update");
        return Ok(());
    }

    // Step 4: ArtifactPackager - Package and hash artifacts
    tracing::info!("Packaging artifacts...");
    let packager = ArtifactPackager::new(config.paths.staging_dir.join(&build_id));

    let mut package_results = Vec::new();
    for build_result in &build_results {
        tracing::info!("Packaging unit: {}", build_result.plugin_name);

        let unit_path = resolve_unit_path(&descriptor_builder, &build_result.plugin_name)?;
        let mut descriptor = BuildUnitDescriptor::load(&unit_path)?
            .with_context(|| format!("Missing BuildUnit.yaml for {}", build_result.plugin_name))?;

        let unit_uuid = if let Some(uuid) = descriptor.uuid.clone() {
            uuid
        } else {
            let new_uuid = uuid::Uuid::new_v4().to_string();
            descriptor.uuid = Some(new_uuid.clone());
            descriptor.save(&unit_path)?;
            new_uuid
        };

        let package_result = packager.package_plugin(
            &build_result.plugin_name,
            &unit_uuid,
            &build_id,  // Use build_id as version
            &build_result.code_out_path,  // Use explicit code output path
            &build_result.iostore_out_path,
        )?;

        tracing::info!(
            "Packaged {} (UUID: {}) with code hash: {}",
            package_result.plugin_name,
            package_result.uuid,
            package_result.code_artifact.sha256
        );

        package_results.push(package_result);
    }

    if total_units > 0 {
        let build_duration = build_phase_start.elapsed();
        let completed = build_results.len();
        let failed = total_units.saturating_sub(completed);
        tracing::info!("+-----------------------------------------");
        tracing::info!("| Build Summary");
        tracing::info!("+-----------------------------------------");
        tracing::info!("| Total plugins: {}", total_units);
        tracing::info!("| Completed: {}", completed);
        tracing::info!("| Failed: {}", failed);
        tracing::info!("| Total time: {}s", build_duration.as_secs());
        tracing::info!("+-----------------------------------------");
    }

    // Step 5: ManifestStore - Update manifest.json
    tracing::info!("Updating manifest...");
    let manifest_store = ManifestStore::new(config.paths.manifest_file.clone())?;

    let engine_build_id = source_version.build_id();
    let mut manifest = if config.paths.manifest_file.exists() {
        manifest_store.load()?
    } else {
        Manifest {
            manifest_version: 1,
            engine_build_id: engine_build_id.clone(),
            signed_at: None,
            signing_key_id: None,
            signature: None,
            plugins: Vec::new(),
        }
    };
    if manifest.engine_build_id != engine_build_id && !manifest.plugins.is_empty() && !force {
        anyhow::bail!(
            "manifest engine {} != source engine {}; rerun build --force so every plugin is rebuilt",
            manifest.engine_build_id,
            engine_build_id
        );
    }
    manifest.engine_build_id = engine_build_id;

    for package_result in package_results {
        // Use UUID and version from package result (ensures consistency with staging directory)
        let plugin_uuid = &package_result.uuid;
        let plugin_version = &package_result.version;

        // Generate CDN URLs with UUID-based pattern (/units/{uuid}/{version}/)
        let base_url = format!("https://cdn.alis.game/units/{}/{}", plugin_uuid, plugin_version);

        // Create code artifact
        let code_artifact = Artifact {
            url: format!("{}/code.zip", base_url),
            hash: package_result.code_artifact.sha256.clone(),
            size: package_result.code_artifact.size_bytes,
            role: None,
        };

        // Create content artifacts from assets (IoStore files: .pak, .utoc, .ucas)
        let content_artifacts: Vec<Artifact> = package_result.assets.iter()
            .filter(|a| a.file_name.ends_with(".pak") || a.file_name.ends_with(".utoc") || a.file_name.ends_with(".ucas"))
            .map(|a| {
                let role = if a.file_name.ends_with(".pak") {
                    Some("pak".to_string())
                } else if a.file_name.ends_with(".utoc") {
                    Some("utoc".to_string())
                } else {
                    Some("ucas".to_string())
                };
                Artifact {
                    url: format!("{}/{}", base_url, a.file_name),
                    hash: a.sha256.clone(),
                    size: a.size_bytes,
                    role,
                }
            })
            .collect();

        // Find corresponding build_result to get base_release_version
        let base_release_version = build_results.iter()
            .find(|br| br.plugin_name == package_result.plugin_name)
            .and_then(|br| br.base_release_version.clone());

        let entry = PluginManifestEntry {
            uuid: plugin_uuid.clone(),
            name: package_result.plugin_name.clone(),
            version: plugin_version.clone(),
            module: Some(format!("Project{}", package_result.plugin_name)),  // TODO: Parse from .uplugin
            platform: "Windows".to_string(),  // TODO: Get from build config
            code: code_artifact,
            assets: content_artifacts,
            depends_on: Vec::new(),  // TODO: Parse from .uplugin dependencies
            channel: channel.clone(),
            signature_thumbprint: None,  // TODO: Implement DLL signing
            release_notes: None,
            mirrors: Vec::new(),
            base_release_version,
        };

        manifest_store.update_plugin(&mut manifest, entry)?;
    }

    manifest_store.save(&manifest)?;
    tracing::info!("Manifest updated successfully");

    // Refresh BuildUnit descriptor hashes after successful builds
    if !dry_run {
        let plugin_only: Vec<String> = build_order
            .iter()
            .filter(|name| *name != &project_name)
            .cloned()
            .collect();
        descriptor_builder.refresh_hashes(&plugin_only)?;
        descriptor_builder.refresh_project()?;
        tracing::info!(
            "Updated BuildUnit.yaml hashes for project + {} plugins",
            plugin_only.len()
        );
    }

    // BuildState save is handled by the executor that owns its lifecycle.

    // Issue the source-release receipt for this gated run: binds the
    // staged artifacts to the exact inputs. Publishing later verifies it
    // (non-bypassable gate); the receipt never covers itself.
    let build_staging_dir = config.paths.staging_dir.join(&build_id);
    if build_staging_dir.exists() {
        let mut receipt_artifacts = Vec::new();
        for entry in WalkDir::new(&build_staging_dir).into_iter().filter_map(|e| e.ok()) {
            let p = entry.path();
            if p.is_file()
                && p.file_name().map(|n| n != engine_config::RECEIPT_FILE_NAME).unwrap_or(false)
            {
                receipt_artifacts.push(p.to_path_buf());
            }
        }
        let receipt = engine_config::Receipt::issue(
            engine_config::current_commit(&config.paths.project_root)?,
            engine_config::workspace_fingerprint(&config.paths.project_root)?,
            &uproject,
            ue_root,
            format!("{}/{}", config.build.configuration, config.build.platform),
            &receipt_artifacts,
        )?;
        let receipt_path = build_staging_dir.join(engine_config::RECEIPT_FILE_NAME);
        std::fs::write(&receipt_path, serde_json::to_string_pretty(&receipt)?)?;
        tracing::info!("Source-release receipt issued: {}", receipt_path.display());
    }

    tracing::info!("=== BUILD COMPLETE ===");
    tracing::info!("Build ID: {}", build_id);
    tracing::info!("Plugins built: {}", build_order.len());
    tracing::info!("Manifest: {}", config.paths.manifest_file.display());

    Ok(())
}

/// Handle publish command
async fn handle_publish(
    config: &BuildServiceConfig,
    build_id: String,
    stage: bool,
    promote: bool,
    rollback: bool,
) -> Result<()> {
    tracing::info!("=== PUBLISH COMMAND ===");
    tracing::info!("Build ID: {}", build_id);
    tracing::info!("Stage: {}", stage);
    tracing::info!("Promote: {}", promote);
    tracing::info!("Rollback: {}", rollback);

    // Validate flags
    let operations = [stage, promote, rollback].iter().filter(|&&x| x).count();
    if operations != 1 {
        return Err(anyhow::anyhow!("Must specify exactly one of: --stage, --promote, or --rollback"));
    }

    // Initialize CDN publisher (communicates only with Control API via nginx)
    let control_api_token = resolve_control_api_token(&config.cdn)?;

    let publisher_config = PublisherConfig {
        control_api_url: config.cdn.control_api_url.clone(),
        control_api_token,
    };

    let publisher = CdnPublisher::new(publisher_config);

    if rollback {
        tracing::info!("Rolling back channel...");
        let channel = config.build.channel.clone();
        let response = publisher.rollback(&channel, None).await?;
        tracing::info!("Rollback successful: {}", response.message);
        tracing::info!("Rolled back to version: {}", response.rolled_back_to);
        return Ok(());
    }

    // Stage: Upload artifacts via Control API
    if stage || promote {
        tracing::info!("Uploading artifacts via Control API...");

        // Collect all artifacts from staging directory
        let build_staging_dir = config.paths.staging_dir.join(&build_id);
        if !build_staging_dir.exists() {
            return Err(anyhow::anyhow!(
                "Build staging directory not found: {}",
                build_staging_dir.display()
            ));
        }

        let mut artifact_paths = Vec::new();
        // Recursively walk the staging directory to find all artifact files
        // (the receipt itself is not an artifact - manifest-style no-self-hash)
        for entry in WalkDir::new(&build_staging_dir)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.is_file()
                && path.file_name().map(|n| n != engine_config::RECEIPT_FILE_NAME).unwrap_or(false)
            {
                artifact_paths.push(path.to_path_buf());
            }
        }

        tracing::info!("Found {} artifact files to upload", artifact_paths.len());
        publisher.upload_artifacts(&build_id, &build_staging_dir, &artifact_paths).await?;
        tracing::info!("Artifacts uploaded successfully");

        // Upload manifest
        if config.paths.manifest_file.exists() {
            tracing::info!("Uploading manifest...");
            publisher.upload_manifest(&build_id, &config.paths.manifest_file).await?;
            tracing::info!("Manifest uploaded successfully");
        } else {
            tracing::warn!("Manifest file not found: {}", config.paths.manifest_file.display());
        }
    }

    // Promote: NON-BYPASSABLE source gate. The receipt issued by the
    // gated build run must verify against CURRENT repo state and the
    // exact artifacts being promoted; only then can promotion happen
    // (VerifiedReceipt is constructible solely via Receipt::verify).
    if promote {
        tracing::info!("Promoting build to channel...");
        let build_staging_dir = config.paths.staging_dir.join(&build_id);
        let receipt_path = build_staging_dir.join(engine_config::RECEIPT_FILE_NAME);
        let receipt: engine_config::Receipt = serde_json::from_str(
            &std::fs::read_to_string(&receipt_path).with_context(|| format!(
                "source-release receipt not found: {} - promotion requires a \
                 gated build run (there is no publish-existing-artifact path \
                 that skips the source gate)",
                receipt_path.display()
            ))?,
        )?;
        let mut receipt_artifacts = Vec::new();
        for entry in WalkDir::new(&build_staging_dir).into_iter().filter_map(|e| e.ok()) {
            let p = entry.path();
            if p.is_file()
                && p.file_name().map(|n| n != engine_config::RECEIPT_FILE_NAME).unwrap_or(false)
            {
                receipt_artifacts.push(p.to_path_buf());
            }
        }
        let verified = receipt.verify(
            &engine_config::current_commit(&config.paths.project_root)?,
            &engine_config::workspace_fingerprint(&config.paths.project_root)?,
            &config.paths.project_root.join("Alis.uproject"),
            &receipt_artifacts,
        )?;
        let channel = config.build.channel.clone();
        let response = publisher.verify_and_promote(&verified, &build_id, &channel).await?;
        tracing::info!("Promotion successful: {} bundles verified, {} copied",
            response.bundles_verified, response.bundles_copied);
    }

    tracing::info!("=== PUBLISH COMPLETE ===");
    Ok(())
}

fn resolve_control_api_token(cdn_config: &CdnConfig) -> Result<Option<String>> {
    if let Some(env_name) = cdn_config
        .control_api_token_env
        .as_ref()
        .map(|s| s.trim())
        .filter(|s| !s.is_empty())
    {
        let token = std::env::var(env_name)
            .with_context(|| format!("Missing required environment variable {}", env_name))?;
        if token.trim().is_empty() {
            anyhow::bail!("Environment variable {} is empty", env_name);
        }
        Ok(Some(token))
    } else {
        Ok(None)
    }
}

/// Handle watch command
async fn handle_watch(
    config: &BuildServiceConfig,
    ue_root: &std::path::Path,
    branch: String,
    interval: u64,
) -> Result<()> {
    tracing::info!("=== WATCH COMMAND ===");
    tracing::info!("Branch: {}", branch);
    tracing::info!("Interval: {}s", interval);

    let repo = git2::Repository::open(&config.paths.project_root)
        .with_context(|| "Failed to open Git repository")?;

    let mut last_commit_id = String::new();

    loop {
        // Get current commit on branch
        let reference = repo.find_reference(&format!("refs/heads/{}", branch))
            .with_context(|| format!("Branch not found: {}", branch))?;

        let commit = reference.peel_to_commit()
            .with_context(|| "Failed to get commit from branch")?;

        let current_commit_id = commit.id().to_string();

        if last_commit_id.is_empty() {
            // First iteration - just record the commit
            tracing::info!("Watching branch '{}' at commit {}", branch, &current_commit_id[..8]);
            last_commit_id = current_commit_id.clone();
        } else if current_commit_id != last_commit_id {
            // New commit detected - trigger build
            tracing::info!("New commit detected: {} -> {}",
                &last_commit_id[..8], &current_commit_id[..8]);
            tracing::info!("Triggering auto-build...");

            // Trigger build for changed plugins (default behavior)
            let result = handle_build(
                config,
                ue_root,
                false, // force = false (use default changed behavior)
                None,  // plugin = None
                false, // dry_run = false
                None,  // build_id = None (auto-generate)
                config.build.channel.clone(),
                false, // stream_output = false for watch mode
            ).await;

            match result {
                Ok(_) => {
                    tracing::info!("Auto-build completed successfully");
                    last_commit_id = current_commit_id;
                }
                Err(e) => {
                    tracing::error!("Auto-build failed: {}", e);
                    // Continue watching even if build fails
                }
            }
        } else {
            tracing::debug!("No new commits on branch '{}'", branch);
        }

        // Wait for next poll
        tokio::time::sleep(tokio::time::Duration::from_secs(interval)).await;
    }
}

fn resolve_unit_path(builder: &BuildUnitBuilder, unit_name: &str) -> Result<PathBuf> {
    if unit_name == builder.project_name() {
        Ok(builder.project_path().to_path_buf())
    } else {
        builder
            .plugin_mapper()
            .get_plugin_path(unit_name)
            .with_context(|| format!("Plugin '{}' not found in repo", unit_name))
    }
}
