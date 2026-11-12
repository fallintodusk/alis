# Road elevation consumption

```mermaid
flowchart LR
    B[Admitted bridge field] -->|Terrain abutments| R[Road realization]
    F[Explicit typed diagnostic fixture] -->|Constructed field and supporting model identity| R
    T[Unchanged final Terrain] -->|Coverage and clearance| R
    S[Selected canonical Road axes and nominal widths] --> P[Shared nominal formation]
    P -->|XY segments and faces| R
    P -->|Height-free selected footprint| A[Support-role audit]
    R --> M[Native source mesh]
    R -->|Actual native origin, stored XYZ and explicit lift| E[Road emission read]
    E --> Q[Diagnostic route qualification]
    C[Complete structural description] -->|Canonical height, zero physical lift| M
    F -->|Qualified finite XYZ field| C
```

The [nominal footprint reader](../../../Source/ProjectWorldRoadEditor/Public/ProjectWorldRoadNominalFootprint.h)
uses the same formation as realization, before height projection and native
storage. It can inspect explicit selected axes without Terrain. Aggregate cell
selection work is bounded independently of each feature's subdivision budget.
Its output is
not a saved-width or collision certificate; final clipping and float conversion
remain consumer acceptance boundaries. Oversized formation refuses before
allocation or iteration. The existing Road tuple and profile selection remain
the authority for whether a Road is realized.
