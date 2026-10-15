# Main component view

```mermaid
flowchart LR
    Recipe[Structural recipe] --> Compiler[Editor compiler]
    Native[Epic material expressions and functions] --> Compiler
    Compiler -->|authenticated descriptors and packages| Catalog[Runtime catalog]
    Catalog --> Owner[Runtime resource owner]
    Owner -->|one admitted identity| Default[Native material DAG]
    Owner -->|explicit opt-in| Adapter[Bundle producer adapter]
    Adapter --> TG[Native Texture Graph]
    Default --> Slots[Stable GPU output slots]
    Adapter -->|complete outputs and fence| Slots
    Owner -->|budget, pins and lifetime| Slots
    Slots -->|shared output ABI| Consumer[Material consumers]
```

The runtime owner is the only public cache and lifecycle authority. The optional
adapter is an execution implementation, not another output registry or cache.

The filtered profile keeps its private intermediate inside the same owner:

```mermaid
flowchart LR
    Raw[Graded particles] --> Filter[Native filter]
    Filter --> Normal[Shared normal derivation]
    Filter --> Masks[Shared mask derivation]
    Filter --> StructureSlot[Structure export]
    Normal --> NormalSlot[Normal export]
    Masks --> MasksSlot[Masks export]
    StructureSlot --> Material[Shared material consumers]
    NormalSlot --> Material
    MasksSlot --> Material
```
