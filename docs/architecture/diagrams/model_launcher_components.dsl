// Launcher Component Definitions
// This file is included inside the launcherApp container block in model.dsl
// Launcher runs BEFORE UE starts - downloads all payloads and starts UE exe

// Launcher core
launcherCore = component "Launcher.Core" "Main application loop - coordinates UI/download/install phases" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Configuration management
launcherConfigManager = component "Launcher.ConfigManager" "Reads/writes config.ini (%APPDATA%/ALIS/Launcher/config.ini) with install path" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Authentication
launcherAuth = component "Launcher.Auth" "Handles OAuth sign-in flow, stores auth token, provides token to ProcessStarter" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Manifest operations
launcherManifestUpdater = component "Launcher.ManifestUpdater" "Fetches remote manifest, compares with local, callbacks to UI if updates available" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Security
launcherSignatureValidator = component "Launcher.SignatureValidator" "Validates manifest signature + Authenticode thumbprints" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Download operations
launcherDownloadManager = component "Launcher.DownloadManager" "Downloads project/orchestrator/plugins from CDN with resume support" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

launcherHashVerifier = component "Launcher.HashVerifier" "Verifies downloaded payloads against SHA-256 hashes from validated manifest (chain of trust: signed manifest -> trusted hashes -> verified archives)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Installation
launcherInstaller = component "Launcher.Installer" "Extracts downloaded payloads to install path" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Artifact storage (manages two payload buckets)
launcherProjectPackageStore = component "Launcher.ProjectPackageStore" "Manages base project package (.exe + .uproject with orchestrator reference)" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

launcherPluginBundleCache = component "Launcher.PluginBundleCache" "Manages plugin bundles (per-manifest DLL + cooked assets) for dependency tree coherence" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// UI
launcherUI = component "Launcher.UI" "Shows install path selection, update UI (what needs updating, skip option, download progress), start game button" "C++" {
    tags "Tier:UI,Domain:UI"
}

// Process management
launcherProcessStarter = component "Launcher.ProcessStarter" "Starts UE executable with auth token via secure IPC" "C++" {
    tags "Tier:Boot,Domain:Platform"
}

// Telemetry
launcherMonitoringSender = component "Launcher.MonitoringSender" "Sends launcher telemetry (downloads, errors, timing) to monitoring service" "C++" {
    tags "Tier:Boot,Domain:Platform"
}
