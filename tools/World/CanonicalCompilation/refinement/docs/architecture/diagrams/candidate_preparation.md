# Candidate preparation and refusal

This view shows the implemented lifecycle of admitted operations. The
[combined-preparation contract](../../../../terrain_refinement.md#combined-feature-preparation)
owns supported operation combinations, ordering and reuse semantics.

```mermaid
sequenceDiagram
    participant Caller as Qualified caller
    participant Publisher as Surface publisher
    participant Base as Authenticated immutable base
    participant Operations as Admitted constructors
    participant Staging as Isolated staging
    Caller->>Publisher: Explicit operations and pinned evidence
    Publisher->>Base: Authenticate complete base and ownership
    Publisher->>Publisher: Admit operations and reject conflicting ownership
    Publisher->>Publisher: Ground before bank/imports; otherwise kind, feature identity and pinned hash
    alt Complete candidate already matches this request
        Publisher->>Publisher: Authenticate all retained outputs
        Publisher-->>Caller: Existing candidate
    else Fresh candidate
        loop Each admitted operation
            Publisher->>Operations: Original support and qualified operation inputs
            Note over Publisher,Operations: Joint basin construction also inspects earlier effective feature changes
            Note over Publisher,Operations: The bank consumes newly constructed Ground and refitted Water
            Operations-->>Publisher: Derived records and optional Terrain replacement, or refusal
        end
        Publisher->>Publisher: Recheck authored Terrain against all effective bound supports
        alt Every operation succeeds
            Publisher->>Staging: Copy complete base and merge changed artifacts
            Publisher->>Staging: Stage regular Ground before bank wrappers and bind final structural support
            Publisher->>Staging: Bind evidence, coverage, reports and result
            Publisher->>Publisher: Recheck base and consumed implementation currency
            Publisher-->>Caller: One complete prepared candidate
        else Any construction refuses
            Publisher-->>Caller: Failure without partial candidate publication
        end
    end
```

Candidate preparation does not activate canonical authority or mutate a World.
Selection and World coordination follow the separate
[effective-authority view](effective_authority.md).
