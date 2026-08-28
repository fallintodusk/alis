# Plan Kazan presentation research campaign

**Status:** RESEARCH COMPLETE; COMBINED IMPLEMENTATION STABILIZATION FLAGS OPEN
**Research progress:** 7/7 concerns complete
**Product target:** Intentional generated blockout suitable for public prototype footage.

## Stable routes

- [Territory generation router](../../../Plugins/World/ProjectWorld/docs/territory_generation.md)
- [Territory authority contract](../../../Plugins/World/ProjectWorld/docs/territory_contract.md)
- [World Partition contract](../../../Plugins/World/ProjectWorld/docs/world_partition.md)
- [Visual Verification owner](../../../tools/World/VisualVerification/README.md)
- [Kazan initiative roadmap](world_generate_kazan_territory_roadmap.md)

## Frozen boundary

Canonical geography, generator architecture, authored overlays, and the selected
`512/1536` runtime profile stay frozen. Generated presentation bytes may change
only through their existing owner and regeneration contract. No concern may add
a second authority for water, vegetation, roads, buildings, or terrain.

Universal material behavior, recipes, and generated reusable resources belong to
ProjectMaterial. ProjectWorld owns semantic binding/assignment. ProjectWorldData owns
geography and sourced semantic facts only; it must not accumulate material paths, graph
parameters, recipes, MICs, or material manifests as concerns migrate.

ProjectMaterial is a lower-level Resources content leaf. World consumers use stable soft
asset identities and, when packaged activation/cook requires it, a normal plugin content
dependency. They add no `Build.cs` dependency on ProjectMaterial or ProjectMaterialEditor
and no material runtime service. The material-core implementation must establish this tier
in stable plugin rules and C4 before the first consumer migration.

A one-time consumer reference cutover may change only the packages that serialize the old
material path, but changed bytes still require new immutable manifests for their exact owner
scopes. Promote those manifests atomically through `active_set.json` and pass the authority
audit. Later same-path material tuning advances ProjectMaterial authority only and causes
zero World package writes or World manifest advancement.

Every concern must select one explicit owner. An explicit owner does not imply
a new plugin or module; use one only when dependency or runtime isolation proves
it is required.

## Research workflow

1. The [research orchestrator](../../01_done/world/world_research_kazan_ue58_presentation.md)
   closed after all seven comprehensive concern packets and the priority comparison.
2. All seven concern packets are research-complete. Compare them as one bounded selection
   task; do not reopen a passed concern without a concrete cross-packet contradiction.
3. Reviewer proposes the concern decision and evidence.
4. Agent checks critical engine, repository, authority, and target-performance claims.
5. Apply corrections and mark that concern `RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED`.
6. Repeat until every concern is research-complete. Implement nothing during this campaign.
7. Priority one was the packaged playable tour (Track P). Its technical gates are accepted;
   [the operator walkthrough remains](world_enable_kazan_playable_tour.md).
8. If the selected concern is Landscape, Water, Roads, Vegetation, or Buildings, first
   accept the [shared material generation core](../content/material_generate_assets_from_json.md),
   then promote one consumer to `todo/00_current/`.
   Implement the core once; material-dependent concerns consume universal ProjectMaterial
   resources through their semantic owner and must not create another graph generator.
   ProjectWorldData owns geography/semantic facts, not material implementation. A concrete
   ObjectId-specific ProjectObject resource is the separately reviewed exception.
9. Promote only the selected concern to the flat `todo/00_current/` directory for its
   separately approved implementation pass. Track P has no material-core prerequisite.
10. Research completion changes a concern's status in backlog; it does not move the todo
   to done. Only completed implementation plus verification authorizes that move.

## Concern register

| Concern | Implementation todo | Research state | Owner |
|---|---|---|---|
| Environment and day/night | [Research](world_present_kazan_environment.md) | RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED | ProjectWorldEnvironment lifecycle adapter + ProjectWorldData configuration |
| Landscape presentation | [Implementation](world_present_kazan_landscape.md) | INTEGRATED K1 CANDIDATE / WATER LOCALITY RE-ACCREDITATION PENDING | ProjectMaterial universal terrain resources + ProjectWorld binding/assignment + ProjectWorldData geography only |
| Water presentation | [Research](world_present_kazan_water.md) | RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED | ProjectMaterial universal Water resources + ProjectWorld binding/geometry + ProjectWorldData geography only |
| Road presentation | [Research](world_present_kazan_roads.md) | RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED | ProjectMaterial universal Road resources + ProjectWorld geometry and `presentation:v1` binding/assignment + ProjectWorldData road facts only |
| Vegetation presentation | [Research](world_present_kazan_vegetation.md) | RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED | ProjectWorld vegetation realization + ProjectWorldData profile |
| Building presentation | [Research](world_present_kazan_buildings.md) | RESEARCH COMPLETE / IMPLEMENTATION NOT SELECTED | ProjectMaterial universal Building resources + ProjectWorld `presentation:v1` binding/assignment + ProjectWorldData geography only |
| Playable tour + release capture | [Research](world_enable_kazan_demo_capture.md) | TRACK P TECHNICALLY ACCEPTED / TRACK V FINAL RELEASE TRUTH REBIND PENDING | Tour: Kazan descriptor selection -> ProjectSinglePlay policy -> ProjectCharacter behavior. Capture: ProjectCinematic authored capture/render. VisualVerification is evidence infrastructure only. |

Final selection compares visible impact, integration risk, implementation effort,
dependencies, packaged-build risk, and measured RTX 4070 High 1440p/60 cost.

Material-dependent concern research may finish before the shared compiler exists. No
material-dependent implementation may start before that compiler is accepted.

## Selected implementation routing

- Awaiting operator only: packaged playable tour (Track P).
- Integrated candidates requiring isolated re-accreditation: Landscape K1 and the
  automated ProjectCinematic release master (Track V).
- Selected first material consumer after independent core acceptance: Landscape K1, because
  accepted packaged visuals confirm the green striped debug terrain dominates the frame.
- Not current: Water, Roads, Vegetation, Buildings, and Environment.
