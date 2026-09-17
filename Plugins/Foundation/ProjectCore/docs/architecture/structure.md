# Structure

## Purpose

ProjectCore provides the small abstractions and registries that let ALIS
components collaborate without importing provider internals.

## Owns

- Public capability interfaces and their contract types.
- Service registration and lookup mechanics.
- Experience descriptor registration.
- Project paths, configuration helpers, and foundation diagnostics.

## Does not own

- Concrete loading, save, settings, gameplay, UI, or World behavior.
- General shared data unrelated to a ProjectCore contract; that belongs to
  [ProjectSharedTypes](../../../ProjectSharedTypes/README.md).
- Inventories of providers or consumers; executable dependencies remain in
  descriptors and `Build.cs` files.

## Composition

| Part | Responsibility |
|---|---|
| Interfaces and service contracts | Cross-component capabilities |
| Contract types | Data crossing those interfaces |
| Service locator | Runtime registration and lookup |
| Experience registry | Descriptor registration and discovery |
| Foundation helpers | Paths, configuration, validation, and diagnostics |

## Relationships

Relationships are drawn once in [the main view](diagrams/main.md).

Through typed service keys, providers publish a narrow interface and consumers
resolve it without knowledge of the concrete provider. Absence remains an
explicit outcome at the consumer lifecycle boundary.

## Invariants

1. ProjectCore contains no feature, presentation, World, or concrete service
   policy.
2. A public interface exposes only the capability required by its consumers.
3. Service registrations are removed when their provider shuts down.
4. Experience descriptors describe requested content and travel; they do not
   execute loading.
