// Styles shared across views
// Why separate from model? To keep semantics (model) independent from
// presentation (colors/shapes). This avoids noise in diffs and enables
// per-environment or per-audience style overrides without editing the model.

styles {
    element "Person" {
        shape person
    }
    element "Software System" {
        background "#1168bd"
        color "#ffffff"
    }
    element "Container" {
        background "#438dd5"
        color "#ffffff"
    }

    // Tags for quick visual grouping
    element "Actor" {
        background "#7f8c8d"
        color "#ffffff"
    }
    element "External" {
        background "#5d6d7e"
        color "#ffffff"
        stroke "#34495e"
        strokeWidth 3
    }
    element "App" {
        background "#2e86c1"
        color "#ffffff"
    }
    element "Boot" {
        background "#d35400"
        color "#ffffff"
    }
    element "Plugin" {
        background "#7f8c8d"
        color "#ffffff"
    }

    // Distinct, representative tier colors (BOXES)
    // Orange = Boot Infrastructure (Orchestrator) - manages plugin lifecycle
    element "Tier:Boot" {
        background "#d35400"
        color "#ffffff"
    }
    // Dark blue = Foundation (ProjectCore) - zero dependencies, defines interfaces
    element "Tier:Foundation" {
        background "#0b3d91"
        color "#ffffff"
    }
    // Light blue = Systems (ProjectLoading, ProjectSession, etc.) - implement Foundation interfaces
    element "Tier:Systems" {
        background "#1f77b4"
        color "#ffffff"
    }
    // Crimson = Gameplay Consolidators (ProjectMenuPlay, ProjectSinglePlay, ProjectOnlinePlay) - orchestrate UE layer + game layer
    element "Tier:Gameplay" {
        background "#d4405e"
        color "#ffffff"
    }
    // Purple = UI Framework + Features (ProjectUI, ProjectMenuMain, ProjectMenuGame, ProjectSettingsUI) - screens, widgets
    element "Tier:UI" {
        background "#8e44ad"
        color "#ffffff"
    }
    // Bright green = Features (ProjectCombat, ProjectDialogue, ProjectInventory) - pure mechanics
    element "Tier:Features" {
        background "#2ecc71"
        color "#ffffff"
    }
    // Brown = World (MainMenuWorld, City17) - content composition (maps, data layers, declare GameModes)
    element "Tier:World" {
        background "#8d6e63"
        color "#ffffff"
    }

    // Optional domain hues (light tint overlays when used by renderers)
    element "Domain:Menu" {
        stroke "#1abc9c"
    }
    element "Domain:Gameplay" {
        stroke "#f1c40f"
    }
    element "Domain:World" {
        stroke "#e67e22"
    }
    element "Domain:UI" {
        stroke "#9b59b6"
    }
    element "Domain:Platform" {
        stroke "#3498db"
    }

    // Access plane highlighting
    element "Access:Private" {
        stroke "#e74c3c"
        strokeWidth 4
        shape roundedbox
    }
    element "Access:Public" {
        stroke "#2ecc71"
        strokeWidth 4
        shape roundedbox
    }

    // Relationship styles for DIP pattern visualization (ARROWS)
    // Green dashed = System REGISTERS interface in ServiceLocator (e.g., ProjectLoading registers ILoadingService)
    // Arrow shows: System -> Core (System calls Core's ServiceLocator.Register())
    relationship "Implements" {
        color "#27ae60"
        style dashed
        thickness 2
    }
    // Blue solid = Feature RESOLVES interface from ServiceLocator (e.g., ProjectMenuMain resolves ILoadingService)
    // Arrow shows: Feature -> Core (Feature calls Core's ServiceLocator.Resolve())
    relationship "Uses" {
        color "#2980b9"
        style solid
        thickness 1
    }
    // Default (no tag) = Gray solid = Regular module dependencies (not interface-related)
}
