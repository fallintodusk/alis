# Manual TODO: Plugin Architecture (Simplified)

Purpose
- Keep this manual TODO minimal and aligned with the Authoritative Plugin Layout.

What to do (when using Unreal Editor)
- Create widgets and assets for features inside their plugin folders (e.g., Plugins/Features/ProjectMenuMain/Content/).
- No GameFeatures-specific assets are required; activation is handled by the project Feature Activation API.

References
- Architecture: docs/architecture/plugin_architecture.md
- Bootloader: docs/architecture/immutable_bootloader.md
