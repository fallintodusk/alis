# Asset Identity

ProjectCore owners use stable semantic identifiers for assets and capability
classes whose physical package or class path may change.

## Data Assets

Definitions expose a stable primary asset identity. Registries resolve that
identity to the current asset path; callers do not persist physical package
paths as domain identity.

## Capability Classes

Capability registration uses a stable capability identifier. The registry owns
the current class mapping; data and callers use the identifier.

## Invariants

1. Moving an asset or class does not change its semantic identity.
2. Renaming an identity is a data migration and must update every serialized
   consumer through the owning migration route.
3. A fallback alias exists only for a verified current serialized consumer and
   is removed with that consumer.
