# Architecture Principles

Repository-wide principles that apply when no narrower component owns the
decision.

## Universal Naming Convention

Reusable first-party modules, types, APIs, and log categories use the
`Project*` prefix. `Alis` is reserved for the game module, content, project
configuration, generated Unreal API macro, and user-facing branding.

The executable validator is
[`validate_no_alis_prefix.py`](../../scripts/ue/check/governance/validate_no_alis_prefix.py).

## Machine-Local Values

A value that differs per machine - the repository checkout, engine and tool
installs, user home and temporary directories - is resolved at runtime and never
written into a tracked file, with one exception: `scripts/config/ue_path.conf`
holds the engine and toolchain roots, and a machine whose layout differs
overrides any of its keys in the gitignored `ue_path.local.conf` (see the
[build workflow](../build/workflow.md)).
Scripts derive the repository root from their own location, resolve the engine
through the `scripts/config/` resolvers, find external tools through `PATH` or
the tool's own install registration, and use environment variables such as
`%TEMP%`, `%LOCALAPPDATA%`, and `$HOME` for operating-system locations.
Documentation, examples, and task files show these values as placeholders such
as `<repo>`.

The only other exceptions are text the project does not author - third-party
trees keep their upstream text - and the `CLAUDE.md` and `CODEX.md` agent
adapter symlinks, whose link targets stay machine-bound by operator decision.
The validator lists each exception with its reason.

[`machine_local_paths.py`](../../scripts/ue/check/governance/machine_local_paths.py)
is the executable definition of a machine-local path. The hardcoded-path job of
[`validate_engine_env.py`](../../scripts/ue/check/governance/validate_engine_env.py)
applies it to every tracked file, and the public mirror applies it to every
published file.

## Component Ownership

- A plugin, module, tool, or domain is a black box with an explicit
  responsibility and non-responsibility.
- Put a fact at the narrowest owner that can keep it true.
- Depend on a public contract, never a neighbor's private implementation.
- Keep an owner's action identity, its public contract identity, and provenance apart. An
  owner may recompute whenever its own inputs or implementation change. A consumer invalidates
  only when an input or public contract it actually consumes changes. Information used only as
  provenance (exact bytes, source digests, compiler fingerprints, release manifests)
  authenticates artifacts for acceptance and release and must not become another owner's
  rebuild key.
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

## Canonical World Authority

Engine-independent canonical World data owns durable World semantics. Unreal
assets and runtime representations are replaceable generated projections: they
contain no unique authored authority and must be reproducible from authenticated
canonical inputs and declared generation contracts. Authored changes enter the
owning source or canonical overlay, never generated asset state.

## Current-Only Architecture

Architecture documents state supported current behavior. Proposed work belongs
in the task tracker, and general evolution belongs in Git.
ALIS does not retain compatibility layers or migration narration without a
verified current consumer.
