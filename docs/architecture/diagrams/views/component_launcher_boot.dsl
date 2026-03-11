// C3a - Component view: Launcher Boot Chain
// Shows Launcher download/install/verify phase before starting Game Client

component launcher.launcherApp {
    title "C3 - Launcher Boot Chain"

    // External systems
    include user
    include authService
    include cdnService.cdnInfrastructure.cdnEdge
    include monitoringService

    // Launcher components
    include launcher.launcherApp.launcherCore
    include launcher.launcherApp.launcherConfigManager
    include launcher.launcherApp.launcherAuth
    include launcher.launcherApp.launcherManifestUpdater
    include launcher.launcherApp.launcherSignatureValidator
    include launcher.launcherApp.launcherDownloadManager
    include launcher.launcherApp.launcherHashVerifier
    include launcher.launcherApp.launcherInstaller
    include launcher.launcherApp.launcherProjectPackageStore
    include launcher.launcherApp.launcherPluginBundleCache
    include launcher.launcherApp.launcherUI
    include launcher.launcherApp.launcherProcessStarter
    include launcher.launcherApp.launcherMonitoringSender

    // Client Executable (handoff target - collapsed)
    include project.projectClient

    // Relationships auto-rendered from model_launcher_relationships.dsl
    // Shows flow: User -> Launcher (make disk match manifest) -> starts Game Client
    // Compatibility guaranteed by manifest (all artifacts built from same engine by CI/CD)

    // autolayout lr
}
