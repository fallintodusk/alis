# Production World Profiles

This directory owns accepted, Kazan-specific source, compiler, runtime,
presentation, and validation profiles plus territory budgets and control-point
catalogs.

Generic tools consume explicit repository-relative paths to these files. They
must not infer production profiles from a tool-owned profile directory. The
validation entry, source profile, and compiler profile must all resolve under
this plugin's descriptor-derived `Data/` root; their IDs, owner, and declared
source path must agree before execution.

`p0` and `representative_v1` remain compiler-focused regression profiles. They
do not own a realization profile and are not evidence for generated terrain.
Full Unreal realization requires a typed realization profile, explicit topology,
and expected layer inventory. The active production acceptance owner is
`kazan_territory_v1.validation.json`.

`kazan_territory_v1` now owns the admitted source profile, compact 210-cell
compiler selection, control network, pre-realization budget, Unreal realization
profile, and executable EndToEndValidation profile. The Matrix profile binds a
Mesh Terrain synthetic twin to the production 210-cell topology and layer
inventory. Durable Unreal enrollment remains a separate intentional L3
operation after that Matrix accepts. The obsolete 36-cell profiles must not be
restored or used as production authority.
