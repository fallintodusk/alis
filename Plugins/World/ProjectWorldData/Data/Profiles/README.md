# Production World Profiles

This directory owns accepted, Kazan-specific source, compiler, runtime,
presentation, and validation profiles plus territory budgets and control-point
catalogs.

Generic tools consume explicit repository-relative paths to these files. They
must not infer production profiles from a tool-owned profile directory. The
validation entry, source profile, and compiler profile must all resolve under
this plugin's descriptor-derived `Data/` root; their IDs, owner, and declared
source path must agree before execution.

`kazan_territory_v1` files are intentionally absent until Slice 1 admits every
required source product, validates the extraction envelope, and freezes the
revised 210-cell dry-run budgets. The obsolete 36-cell profiles must not be
restored or used as production authority.
