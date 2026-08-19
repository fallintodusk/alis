# Production World Profiles

This directory owns accepted, Kazan-specific source, compiler, runtime,
presentation, and validation profiles plus territory budgets and control-point
catalogs.

Generic tools consume explicit repository-relative paths to these files. They
must not infer production profiles from a tool-owned profile directory. The
validation entry, source profile, and compiler profile must all resolve under
this plugin's descriptor-derived `Data/` root; their IDs, owner, and declared
source path must agree before execution.

`require_landscape` means Landscape presence and structure, not terrain correctness.
It asserts that a content-identical incremental leg rewrites no Landscape components,
that the Authored Corrections layer survives, and that GeoReferencing placement error is
zero - all of which a completely flat Landscape satisfies. Elevation correctness is a
separate contract carried by the `terrain_height_*` receipt fields and authenticated by
layered validation; see
[ProjectWorld architecture overview](../../../ProjectWorld/docs/architecture_overview.md)
section 3.1. `p0` and `representative_v1` set `require_landscape` with an empty
`expected_topology` and own no realization profile, so they never execute the terrain
generator and must never be read as evidence that terrain elevation is correct. A profile
that wants terrain correctness must declare explicit topology and run the layered
realization path.

`kazan_territory_v1` now owns the admitted source profile, compact 210-cell
compiler selection, control network, pre-realization budget, Unreal realization
profile, and executable EndToEndValidation profile. The Matrix profile binds a
Landscape-compatible synthetic twin to the production 210-cell topology and
layer inventory. Durable Unreal enrollment remains a separate intentional L3
operation after that Matrix accepts. The obsolete 36-cell profiles must not be
restored or used as production authority.
