# Script Architecture

Scripts are command-boundary adapters. They compose existing owners; they do
not become parallel authorities for build state, test results, release
identity, World data, or Git history.

## Placement Decision

```text
repository or host setup -> scripts/config or scripts/setup
Unreal build             -> scripts/ue/build
static validation        -> scripts/ue/check
Unreal test dispatch     -> scripts/ue/test
runtime launch           -> scripts/ue/run
release transaction      -> scripts/ue/package
World realization        -> scripts/ue/world
public Git projection    -> scripts/git
standalone application   -> tools
small generic helper     -> scripts/utils
```

## Boundaries

1. One public entry point owns an operation; helpers remain private to it.
2. Wrappers preserve the underlying command's success and failure status.
3. PowerShell owns the accepted native Windows release and mirror paths.
4. Machine configuration comes from `scripts/config/`, never tracked personal
   paths.
5. Scripts write temporary artifacts under `tmp/<domain>/<component>/` and
   durable outputs only through the destination owner.
6. A script that changes external state validates identity and refusal paths
   before mutation.

Existing commands are routed from [scripts](../README.md). Authoring rules are
in [Script Development](../DEVELOPING.md).
