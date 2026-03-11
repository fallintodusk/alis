Systems Tier

Purpose
- Reusable runtime services (always-on or broadly depended upon).

Examples
- ProjectLoading: loading phases orchestration; no UI widgets.
- ProjectSession: session lifecycle (offline/online-agnostic); not networking.
- ProjectSave: save/load service and slots; UI-agnostic.
- ProjectPCG: PCG engine integration, nodes, registries.
- ProjectWorld: WP/HLOD/streaming policies (always-on).
- ProjectSettings: settings storage, defaults, typed API.

Non-responsibilities
- No domain screens here; UI belongs to Features.
- Session does not wrap networking; Online/* adapters live in an optional Online tier.

Useful entry points
- [ProjectLoading](ProjectLoading/README.md)
- [ProjectSave](ProjectSave/README.md)
- [ProjectSettings](ProjectSettings/README.md)
- [ProjectMotionSystem](ProjectMotionSystem/README.md)
