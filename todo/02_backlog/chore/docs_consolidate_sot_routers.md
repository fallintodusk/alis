# Consolidate documentation to one SOT per fact

Parked 2026-08-07. Findings from a full audit of the world-generation doc
set plus the root routers. The worst offenders were fixed at audit time
(see "Already fixed"); everything below is the remainder.

Why it matters: routers are what agents read first every session. Every
duplicated fact is a future contradiction, and every stale command is a
wrong action taken confidently. One fact, one home; routers only point.

The priority `territory_generation.md` entry-router split landed before
Slice 2. The remaining items are ordinary hygiene and must not block world
implementation without a concrete contradiction or file-size pressure.

## Already fixed (2026-08-07, no action needed)

- The accepted no-bulk-migration strategy is isolated in
  `legacy_world_transition.md`; the content-integration slice executes it.
- `Plugins/World/ProjectWorld/docs/manual.md` deleted - it was ~90%
  verbatim `world_partition.md`, and its one unique line pointed at a
  non-existent doc. Inbound link in `docs/gameplay/README.md` repointed.
- Two forbidden `todo/create_world.md` references in `world_partition.md`
  (a file that does not exist), plus one in
  `docs/architecture/implementation_examples.md`.
- 17 non-ASCII characters in `world_partition.md`.
- Audit check list and enrollment procedure moved out of the contract into
  `scripts/ue/world/README.md`; the contract keeps only the rules.
- Lean `territory_generation.md` task router now fronts the deep contract.

## Territory entry router - done 2026-08-12

`territory_generation.md` is now a lean task router. The preserved deep text
lives in `territory_contract.md`, so fresh agents do not import it by default.
Split that deep contract further only when a changed section has a clear SRP
owner; do not churn stable decisions merely to reach an arbitrary line count.

| Current file | Role |
|---|---|
| `territory_generation.md` | Lean route by task and owner. |
| `territory_contract.md` | Deep contract imported by exact section only. |

Note while splitting: the envelope numbers are ALSO in the source/compiler
profiles and the territory budget file, which are executable. Keep the
rationale and the clip-margin table (single-homed) in the doc; point at the
executable files for the values.

## Split `Plugins/World/ProjectWorld/README.md` (926 lines)

It declares itself a router but carries contract:

- `Unreal realization contract` (~123 lines) - receipt fields, actor
  tagging, Landscape edit-layer policy, presentation/runtime profile
  contracts, Presentation Gate semantics, transaction restoration. Move to
  a `docs/realization_contract.md` or fold into the authority doc above.
- `Validation` section - live-audit check list, automation-ID rule, the
  "never manually repair generated actors" rule. Move to `docs/`.
- `Legacy Paths` + `Definition Host Metadata Policy` - contract in a README.
- Sections 3-12 are design proposals for unimplemented APIs, hedged by a
  disclaimer. Consider moving to a design doc so the README is usable.

## Remaining duplication

| Fact | Copies | Proposed single home |
|---|---|---|
| Presentation Gate p95 / role-scan semantics | ProjectWorld README, EndToEndValidation README, scripts/ue/world README | `tools/World/EndToEndValidation/README.md` |
| Rejected-Apply restores exact prior state incl. absence | territory doc, ProjectWorld README, scripts/ue/world README, EndToEnd README | `scripts/ue/world/README.md` (it is the wrapper's behavior); contract keeps only the invariant |
| Generator-fingerprint scoping | territory doc, scripts README, world todo | territory doc (contract), README points |
| City17 actor census | `legacy_city17_inventory.md`, `legacy_world_transition.md` | the inventory; the transition record points |
| HLOD policy | ProjectWorld README (twice), territory doc, world_partition.md | one home, others point |
| File-size guardrail, Dev Loop Contract, public-repo migration policy, pitfalls rule | `AGENTS.md` AND `docs/agents/canonical.md` - currently CIRCULAR (each says the other is authoritative while both print the rule) | pick one per rule; the router should point |

## Remaining cleanup in `world_partition.md`

The known dead links, stale commands, and nonexistent KazanMain asset path
were corrected on 2026-08-10. Structural cleanup remains:

- The file presents as a contract doc but is mostly editor usage guidance.
- Blueprint helper snippets are presented as current while the ProjectWorld
  README disclaims all such snippets as design proposals.

Also dead: `Plugins/World/ProjectWorld/README.md` links to
`../PCG/ProjectPCG/README.md` (no such category) and to a
`Plugins/World/City17/Config/World/tile_00_00.json` that does not exist.

## Forbidden todo references outside the world area

Three stable docs reference `todo/00_current/` files, which the project
rules forbid because todos are transient:

- `scripts/ue/editor/render/README.md`
- `docs/cinematics/render_setup.md` (twice)

Both point at `todo/00_current/cinematic_capture_pipeline.md`. Resolve by
lifting the standing content into the stable doc before that todo is
archived, otherwise the links rot. Owned by the cinematics work, not by
the world milestone.
