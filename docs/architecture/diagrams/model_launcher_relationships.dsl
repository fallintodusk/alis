// Launcher Component Relationships
// Shows internal flow within Launcher executable

// Entry point: user launches Launcher app
user -> launcher.launcherApp.launcherCore "Launches game" "Launcher.exe"

// Launcher internal flow
launcher.launcherApp.launcherCore -> launcher.launcherApp.launcherUI "Initialize UI" "Composition"
launcher.launcherApp.launcherUI -> user "Shows UI (install path selection, update options, download progress, sign-in button, start game)" "UI"
user -> launcher.launcherApp.launcherUI "Clicks buttons (set install path, sign in, skip/update, start game)" "User input"

// Authentication flow
launcher.launcherApp.launcherUI -> launcher.launcherApp.launcherAuth "User clicked Sign In" "Control flow"
launcher.launcherApp.launcherAuth -> authService "OAuth sign-in flow" "HTTPS/OAuth"
launcher.launcherApp.launcherAuth -> launcher.launcherApp.launcherUI "Sign-in status (success/failure)" "Callback"

// Config management
launcher.launcherApp.launcherCore -> launcher.launcherApp.launcherConfigManager "Read config.ini for install path" "Composition"
launcher.launcherApp.launcherConfigManager -> launcher.launcherApp.launcherCore "Provides install path (or null on first run)" "Callback"
launcher.launcherApp.launcherUI -> launcher.launcherApp.launcherConfigManager "Saves install path to config.ini" "Control flow"

// Manifest update check
launcher.launcherApp.launcherCore -> launcher.launcherApp.launcherManifestUpdater "Check for updates" "Composition"
launcher.launcherApp.launcherManifestUpdater -> cdnService.cdnInfrastructure "GET /manifest.json" "HTTPS"
launcher.launcherApp.launcherManifestUpdater -> launcher.launcherApp.launcherSignatureValidator "Validate manifest signature" "Dependency"
launcher.launcherApp.launcherManifestUpdater -> launcher.launcherApp.launcherUI "Callbacks if updates available (reports what needs updating)" "Callback"

// Download phase (if needed)
launcher.launcherApp.launcherUI -> launcher.launcherApp.launcherDownloadManager "User clicked Update (download project/orchestrator/plugins)" "Control flow"
launcher.launcherApp.launcherDownloadManager -> cdnService.cdnInfrastructure "Download payloads" "HTTPS"
launcher.launcherApp.launcherDownloadManager -> launcher.launcherApp.launcherUI "Report download progress" "Callback"

// Verification and installation
launcher.launcherApp.launcherDownloadManager -> launcher.launcherApp.launcherHashVerifier "Verify downloaded payloads SHA-256" "Dependency"
launcher.launcherApp.launcherDownloadManager -> launcher.launcherApp.launcherInstaller "Install verified payloads" "Dependency"
launcher.launcherApp.launcherInstaller -> launcher.launcherApp.launcherProjectPackageStore "Store base project package (exe + uproject)" "Dependency"
launcher.launcherApp.launcherInstaller -> launcher.launcherApp.launcherPluginBundleCache "Store plugin bundles (DLL + assets per manifest)" "Dependency"
launcher.launcherApp.launcherInstaller -> launcher.launcherApp.launcherUI "Report installation progress" "Callback"

// Start game (only enabled when reconciliation complete)
launcher.launcherApp.launcherUI -> launcher.launcherApp.launcherProcessStarter "User clicked Start Game" "Control flow"
launcher.launcherApp.launcherProcessStarter -> launcher.launcherApp.launcherProjectPackageStore "Get exe path from project package" "Dependency"
launcher.launcherApp.launcherProcessStarter -> launcher.launcherApp.launcherConfigManager "Get install path + manifest path" "Dependency"
launcher.launcherApp.launcherProcessStarter -> launcher.launcherApp.launcherAuth "Request auth token (if exists)" "Dependency"
launcher.launcherApp.launcherProcessStarter -> project.projectClient "Starts UE exe with config (auth token, manifest path, game install path) via secure IPC" "Process spawn"
launcher.launcherApp.launcherProcessStarter -> project.projectClient.orchestrator "UE exe starts -> Orchestrator Manager entry point receives config" "Triggers"

// Telemetry
launcher.launcherApp.launcherCore -> launcher.launcherApp.launcherMonitoringSender "Send events for telemetry" "Composition"
launcher.launcherApp.launcherMonitoringSender -> monitoringService "Send launcher telemetry (downloads, errors, timing)" "HTTPS/OTel"
