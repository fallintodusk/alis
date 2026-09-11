# World Tools

Router for engine-independent world-data applications. Each child owns one
command lifecycle, its configuration, contracts, fixtures, implementation,
tests, and README.

## Quick Routes

| Goal | Owner |
|---|---|
| See the complete ownership, persistence, realization, and evidence flow | [World architecture overview](../../Plugins/World/ProjectWorld/docs/architecture_overview.md) |
| Reproduce the pinned Python and native tool runtime | [Execution Environment](ExecutionEnvironment/README.md) |
| Acquire, verify, and normalize provider data | [Source Ingestion](SourceIngestion/README.md) |
| Compile accepted source receipts into canonical ALIS cells | [Canonical Compilation](CanonicalCompilation/README.md) |
| Prove the complete source-to-package P0 contract | [End-to-End Validation](EndToEndValidation/README.md) |
| Prove a realized territory is present, placed, and plausible on screen | [Visual Verification](VisualVerification/README.md) |
| Validate cross-component ownership and dependency direction | `python -m unittest discover tools/World/tests` |

## Dependency Direction

```text
EndToEndValidation
    -> public stage commands and receipts
    -> supported Unreal realization and packaging scripts

CanonicalCompilation
    -> SourceIngestion public run identity and accepted receipts
    -> ExecutionEnvironment public Python and native-tool APIs

SourceIngestion
    -> ExecutionEnvironment public Python and native-tool APIs

ExecutionEnvironment
    -> no world stage
```

Provider records never enter canonical internals. Canonical records never
enter source acquisition. End-to-End Validation composes public process and
receipt boundaries; it does not import stage internals or contain another
Unreal realization algorithm.

## Layout Rule

Every profile, contract, fixture, implementation file, and test lives under
its nearest lifecycle owner. Shared runtime pins exist once under
`ExecutionEnvironment/`; stage-specific metadata is never centralized here.
Cross-component fitness tests live in `tests/` because this router owns the
dependency graph rather than any child stage.

Each component uses the same context-first shape:

```text
<Component>/
    app/          implementation modules
    contracts/    owned data and wire contracts
    fixtures/     owned deterministic samples, when needed
    profiles/     owned run configuration, when needed
    tests/        component tests
    api.py        public cross-component surface, when consumed
    bootstrap.py  clean-machine entry point
    run.py        environment-internal entry point, when needed
    README.md      component router
```

The component name appears once. Child names describe their role within that
context; implementation imports use the explicit `World.<Component>.app`
namespace so identical `app/` names cannot collide.
