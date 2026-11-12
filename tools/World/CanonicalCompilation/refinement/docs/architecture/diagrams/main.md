# Refinement boundary relationships

```mermaid
flowchart LR
    Base[Authenticated immutable compile result] -->|base inputs| API[Refinement API]
    Evidence[SourceIngestion qualified evidence] -->|pinned receipts| API
    Caller[Qualified fixture caller] -->|optional module entrypoints| Pure[Ground and constructed-surface constructors]
    Base -->|base geometry and selected support| Pure
    Pure -->|unpublished modeled output| Fixture[Disposable typed consumers]
    Intent[Provenance-bound authored height intent] -->|pinned existing overlay contract| API
    API -->|closed bridge, confluence, basin, authored-dry or structural-import operation| Impl[Construction and publication implementation]
    Impl -->|pure unpublished basin or confluence candidate| Fixture
    Contracts[Owned schemas and identities] -->|validation| Impl
    Native[ExecutionEnvironment] -->|pinned geometry API| Pure
    Native -->|pinned geometry API| Impl
    Impl -->|candidate result and immutable child| Selection[Qualified selection primitives]
    Coordinator[World transaction coordinator] -->|prepare validate publish restore| Selection
    Selection -->|selected authority and recovery metadata| Coordinator
```

This view represents current capabilities. Pure Ground or constructed-platform
output has no production-publication edge. Model-specific semantics belong to
the [surface contract](../../../../terrain_refinement.md).
The [candidate-preparation view](candidate_preparation.md) owns the current
multi-operation lifecycle.
