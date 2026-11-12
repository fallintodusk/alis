# Refinement implementation

| Path | Responsibility |
|---|---|
| [geometry.py](geometry.py) | Policy and constrained final surface |
| [authored_terrain.py](authored_terrain.py) | Provenance-bound dry height deltas and complete incident-quad closure over immutable final grids |
| [civil_grade.py](civil_grade.py) | Whole-quad civil influence census and explicit base-grade references |
| [road_elevation.py](road_elevation.py) | Source-station/lateral bridge elevation candidates |
| [road_surface_projection.py](road_surface_projection.py) | Shared finite-surface projection onto straight native Road ribbons |
| [gangway_elevation.py](gangway_elevation.py) | Static fixed-pier/floating-landing candidates and explicit quality limits |
| [road_publication.py](road_publication.py) | Bridge-specific entrypoint into shared surface publication |
| [surface_publication.py](surface_publication.py) | Qualified source admission and one composed immutable-base feature publication |
| [authority.py](authority.py) | Isolated effective selection preparation and frozen child-bundle authentication |
| [activation.py](activation.py) | Immutable child installation, exact selection CAS and qualified recovery |
| [schema_reference.py](schema_reference.py) | Closed schema annotation normalization and historical selection handoff |
| [water_confluence.py](water_confluence.py) | Disposable source-node terminal constraints for derived channel fits |
| [contained_water.py](contained_water.py) | Pure Ground-quantile construction and connected standing footprint refusal |
| [publication.py](publication.py) | Authenticated base reuse and candidate publication |
| [identity.py](identity.py) | Current child and consumed-helper fingerprints |
| [contracts.py](contracts.py) | Refinement result admission and writes |
| [retained_input.py](retained_input.py) | Bounded authenticated content-addressed model input reads |
