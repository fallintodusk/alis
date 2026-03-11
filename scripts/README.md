# Scripts & Automation

Quick routing for scripts and automation. Primary reference: [docs/architecture.md](docs/architecture.md).

---

## Quick Navigation

| Category | Description | Documentation |
|----------|-------------|---------------|
| **Build** | C++ compilation, module rebuilding | [ue/build/](ue/build/README.md) |
| **Package** | Public release packaging, hashing, signing, and verification | [ue/package/](ue/package/README.md) |
| **Check** | Validation, static analysis, blueprint compile | [ue/check/](ue/check/README.md) |
| **Test** | Automation tests (Unit, Integration, Smoke) | [ue/test/](ue/test/README.md) |
| **Clean** | Cleaning intermediate files | [ue/clean/](ue/clean/README.md) |
| **Run** | Runtime operations | [ue/run/](ue/run/README.md) |
| **Standalone** | Standalone game testing | [ue/standalone/](ue/standalone/README.md) |
| **Git** | Hooks, mirror export, git workflow automation | [git/](git/README.md) |
| **Utils** | General utilities (Structurizr, etc.) | [utils/](utils/README.md) |
| **CI** | CI/CD pipeline orchestration | `ci/` |

---

## Development

- **[DEVELOPING.md](DEVELOPING.md)**: Guide for creating, moving, and debugging scripts.
  - Where to put a new script?
  - Naming rules
  - Debugging guide

---

## Architecture Summary

SOLID structure:

```text
scripts/
|-- config/          # Single source of truth (ue_path.conf)
|-- ue/
|   |-- build/       # Build operations ONLY
|   |-- package/     # Public release packaging
|   |-- check/       # Validation WITHOUT build
|   |-- clean/       # Clean operations
|   |-- run/         # Runtime operations
|   |-- standalone/  # Standalone game testing
|   `-- test/        # All testing
|-- ci/              # CI/CD orchestration
|-- git/             # Hooks and mirror publication helpers
|-- utils/           # General utilities
`-- win/             # Platform-specific helpers
```

Full guide: [docs/architecture.md](docs/architecture.md)

---

## Quick Links

- Back to [Master Index](../docs/README.md)
- Complete scripts guide: [docs/architecture.md](docs/architecture.md)
- Build: [../docs/build/README.md](../docs/build/README.md)
- Package: [ue/package/README.md](ue/package/README.md)
- Test: [../docs/testing/index.md](../docs/testing/index.md)
