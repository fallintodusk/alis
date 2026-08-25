# World slice 3E - generated gameplay placement

## Outcome

Realize a small designer-authored set of ObjectDefinition-backed gameplay
objects into generated Kazan with stable object identity, World Partition
ownership, object-local regeneration, and runtime state kept outside generation
authority.

## Stable routes

- [Territory contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Object definition layer contract](../../../Plugins/Resources/ProjectObject/docs/layer_contract.md)
- [Object spawn service](../../../Plugins/Foundation/ProjectCore/Source/ProjectCore/Public/Services/IObjectSpawnService.h)
- [World pipeline layer gates](../../../docs/testing/world_pipeline_layers.md)
- [Initiative roadmap](../../02_backlog/world/world_generate_kazan_territory_roadmap.md)

## Frozen boundary

Owner: `project_gameplay_placement:v1` in ProjectWorld.

Input/output contract:

```text
designer placement JSON + existing ObjectDefinition primary asset
  -> typed stable-object input domain
  -> canonical surface-snap transform
  -> ObjectDefinition spawn through IObjectSpawnService
  -> stable DataId / actor GUID + spatial OFPA actor
  -> existing layer inventory, transaction, manifest, and rollback path
```

Expected changed components:

- gameplay-placement schema, loader, tuple registration, and profiles;
- ProjectWorld object-ID dirty planning, realization, inventory, and evidence;
- the existing ProjectCore/ProjectObject spawn abstraction only as needed to
  pass deterministic actor name/GUID through its current provider;
- synthetic and minimal Kazan placement data, generated artifacts/manifests,
  tests, and stable ProjectWorld contract docs.

Expected untouched components:

- canonical source, compiler, and accepted geographic authority;
- terrain, water, roads, vegetation, and building producers;
- ObjectDefinition schema/generator/capability behavior;
- runtime save payloads and replication owners;
- transaction, rollback, enrollment, and World Partition architecture.

Stop on a required ProjectWorld -> ProjectObject dependency, a second object
definition/spawn authority, generated JSON receiving mutable runtime state, or
substantial propagation into an untouched owner.

## Execution

- [x] Pin the typed placement document and registered generator tuple.
- [x] Prove object-ID dirty selection, dependency behavior, no-op, move,
  definition change, removal, and clean reconstruction identity.
- [x] Realize ObjectDefinition actors with stable DataId/GUID, surface snap,
  spatial OFPA ownership, collision/capabilities, and no HLOD.
- [x] Add synthetic and minimal real Kazan placements without changing
  geographic canonical authority.
- [x] Extend result/manifest evidence and Matrix expectations.
- [x] Run focused checks, fresh Common Check, final Matrix, enrollment, and
  strict authority/LFS audit.
- [x] Inspect the real placements through Editor/MCP and route the next slice.

## Acceptance evidence

- Final build and four exact Unreal tests: accepted 1/1 each.
- PowerShell layer/fingerprint tests: 15 passed; Python Matrix contract tests:
  22 passed; inline schema validation: 134 files passed.
- Common Check: `check-20260824T110956Z`.
- Kazan Matrix: `run-20260824T111336Z`; 210 terrain proxies, 145 water
  actors, 161 road actors, 7,501 vegetation instances, 171 building cells,
  and 3 gameplay-placement actors.
- Enrollment: `enroll-20260824T113420Z`; active-set SHA-256
  `08d2b49ab9304e8fad4002bc1a16637b01f3d9d7667cf184afa6012e71312a09`.
- Authority audit:
  `Saved/Validation/WorldAuthority/audit_3e_gameplay_placement_20260824T1137Z.json`;
  10 scopes, 1,371 byte-identical artifacts, zero unowned files, and current
  fingerprints. `git lfs fsck` passed.
- Live Editor/MCP evidence:
  `Saved/Validation/WorldVisual/3e-gameplay-20260824T1141Z/mcp_capture.json`;
  exactly 3 tagged spatial InteractableActors with distinct GUIDs,
  ObjectDefinition meshes, pickup capability, plausible terrain contact, and
  Map Check at 0 errors / 0 warnings.
- Two UE 5.8.1 editor-only crashes are not attributable to 3E with current
  evidence. Their observed stacks stayed outside ProjectWorld and ProjectObject:
  live SM5-to-SM6 map switching and Sequencer/Details shutdown teardown. The
  same Kazan map cold-started successfully; crash diagnosis is stored beside
  the visual receipt.

## Out of scope

New object-definition semantics, scenario objectives, loot balancing, runtime
save-system expansion, AI/navigation, facade/interior work, and art polish.
