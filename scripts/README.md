# Scripts and Automation

Public router for ALIS automation scripts.

This router focuses on the script categories that are part of the public source tree and meaningful to external contributors.

## Main Script Categories

- Build: [ue/build/README.md](ue/build/README.md)
- Package and verify releases: [ue/package/README.md](ue/package/README.md)
- Validation checks: [ue/check/README.md](ue/check/README.md)
- Automated tests: [ue/test/README.md](ue/test/README.md)
- Standalone runtime checks: [ue/standalone/README.md](ue/standalone/README.md)
- Run helpers: [ue/run/README.md](ue/run/README.md)
- Git and public mirroring: [git/README.md](git/README.md)

## What Lives Here

- Unreal build wrappers
- validation and test entry points
- packaging, signing, and verification scripts
- standalone and runtime launch helpers
- git workflow helpers and public mirror automation

## Public-Scope Note

Some local-only script categories are intentionally not routed from this public README because they are excluded from the mirror or depend on private machine setup.

Examples:
- private engine-path configuration
- local CI wiring
- internal utility helpers that are not needed to understand the public workflow

## Related Docs

- Root docs router: [../docs/README.md](../docs/README.md)
- Build and release docs: [../docs/build/README.md](../docs/build/README.md)
- Public mirror policy: [git/mirror/README.md](git/mirror/README.md)
- Script development guide: [DEVELOPING.md](DEVELOPING.md)
