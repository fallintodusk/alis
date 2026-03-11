Foundation

This folder contains Tier 1 (foundation) plugins that provide base types, interfaces, and utilities with zero game logic dependencies.

## Purpose

Foundation plugins are the architectural base layer that all other plugins depend on. They provide:
- Pure interfaces and abstract contracts
- Shared types and data structures
- Service locator and DI infrastructure
- Logging categories and utilities
- Core helpers (no concrete systems)

## Design Principles

**What belongs here:**
- Zero-dependency utilities and types
- Service discovery patterns (e.g., service locator)
- Abstract interfaces that define contracts
- Foundation logging and diagnostics

**What does NOT belong here:**
- Concrete game systems (those go in `Systems/`)
- Gameplay features (those go in `Features/`)
- Asset management or data assets
- Business logic or domain-specific code

## Current Plugins

- [ProjectCore](ProjectCore/README.md) - service locator, interfaces, shared types, configuration helpers

## Related Documentation

- [Architecture Router](../../docs/architecture/README.md) - public architecture overview
- [Core Principles](../../docs/architecture/principles.md) - broad architecture rules
