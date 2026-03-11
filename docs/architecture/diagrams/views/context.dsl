// C1 - System Context view
// Purpose: Who uses the system and what external systems exist.
// Shows: User -> Launcher (optional auth) -> CDN -> Game Client

systemContext project {
    title "C1 - System Context: Launcher -> CDN -> Game Client Flow"

    // Core flow
    include user
    include launcher
    include project

    // External systems
    include authService
    include cdnService
    include monitoringService

    // Build infrastructure (for context)
    include buildService
    include claimsService

    // autolayout lr
}
