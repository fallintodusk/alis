# Production World Profiles

This directory owns accepted, Kazan-specific source, compiler, runtime,
presentation, and validation profiles plus territory budgets and control-point
catalogs.

Generic tools consume explicit repository-relative paths to these files. They
must not infer production profiles from a tool-owned profile directory. The
validation entry, source profile, and compiler profile must all resolve under
this plugin's descriptor-derived `Data/` root; their IDs, owner, and declared
source path must agree before execution.

`kazan_territory_v1` now owns the admitted source profile, compact 210-cell
compiler selection, control network, and pre-realization budget. Its
EndToEndValidation profile remains intentionally absent until the first
accepted compile supplies real per-leg counts and a synthetic twin. The
obsolete 36-cell profiles must not be restored or used as production
authority.
