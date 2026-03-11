// Project Architecture - Structurizr DSL workspace (modular)
// ASCII only. No generated images committed.

workspace "Project Architecture" "C4 views (UE 5.5)" {
    !identifiers hierarchical

    model {
        !include model.dsl
    }

    views {
        !include views/context.dsl
        !include views/container_plugins.dsl
        !include views/component_launcher_boot.dsl
        !include views/component_game_boot.dsl
        !include views/component_build_service.dsl
        !include views/component_cdn.dsl
        !include styles.dsl
    }

    configuration {
        // scope softwaresystem - Removed: we show containers in multiple systems (Game Client + CDN)
    }
}
