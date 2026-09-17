# Architecture Principles

Repository-wide principles that apply when no narrower component owns the
decision.

## Universal Naming Convention

Reusable first-party modules, types, APIs, and log categories use the
`Project*` prefix. `Alis` is reserved for the game module, content, project
configuration, generated Unreal API macro, and user-facing branding.

The executable validator is
[`validate_no_alis_prefix.py`](../../scripts/ue/check/governance/validate_no_alis_prefix.py).

## Component Ownership

- A plugin, module, tool, or domain is a black box with an explicit
  responsibility and non-responsibility.
- Put a fact at the narrowest owner that can keep it true.
- Depend on a public contract, never a neighbor's private implementation.
- Let code, schemas, configuration, and tests own machine-provable facts.
- Use stable documentation for cross-file semantics, invariants, and rationale.

## Native Engine First

Use current Unreal Engine capabilities before adding a project framework.
Introduce a wrapper only when the native boundary cannot satisfy a demonstrated
ALIS requirement.

## Data and Authority

- Give mutable state one authoritative writer.
- Derived manifests, reports, caches, and views never become parallel writers.
- Validate untrusted data at its owning boundary and fail closed when identity,
  compatibility, or required evidence is unknown.
- Generated output is replaced through its owning transaction, not repaired by
  hand.

## Current-Only Architecture

Architecture documents state supported current behavior. Proposed work belongs
in the task tracker, and general evolution belongs in Git.
ALIS does not retain compatibility layers or migration narration without a
verified current consumer.
