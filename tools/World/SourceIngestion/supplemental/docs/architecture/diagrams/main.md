# Supplemental source relationships

```mermaid
flowchart LR
    Q[Externally qualified observed Ground or raster model capsule] -->|Basis-specific descriptor| A[Public supplemental API]
    A -->|Admission and pinned read| I[Validation and publication]
    C[Closed product and receipt contracts] -->|Validation| I
    I -->|Verified copies and recovery| R[SourceIngestion retention]
    I -->|Hash, lock and payload verification| S[SourceIngestion helpers]
    I -->|Pinned receipt and basis-specific Ground evidence| K[CanonicalCompilation consumer]
```

This view states relationships at the supplemental owner boundary. The
[structure](../structure.md) routes to each internal part; qualification and
retention semantics remain with their owning contracts.
