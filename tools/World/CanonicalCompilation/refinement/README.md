# Canonical surface refinement

Owns post-canonical final-surface construction and feature-child selection. It consumes an
authenticated accepted base; it never reacquires sources or promotes a compile.
The [refinement contract](../terrain_refinement.md) owns its scope and guarantees.

| Path | Responsibility |
|---|---|
| [api.py](api.py) | Public construction, authority candidates and owned selection primitives |
| [ground.py](ground.py) | Independent pure raster-Ground construction without publication |
| [ground_resolution.py](ground_resolution.py) | Finite sampling map and shared source-station closure |
| [constructed_surface.py](constructed_surface.py) | Pure source-associated platform Road surface without admission |
| [app/](app/) | Geometry, publication and operation identity |
| [contracts/](contracts/) | Refinement surface, report and result schemas |

Refinement implementation and schemas belong to this child identity. Base
compile-result schemas and compile-only production promotion remain separate.
The existing authority reader/materializer can authenticate a selected feature
child; [candidate selection and activation limits](../terrain_refinement.md#effective-authority-candidates)
define that boundary.
Road construction and authenticated disposable publication are separate public
operations. Neither promotes a production Road profile. Their acceptance limits
are owned by the [refinement contract](../terrain_refinement.md#road-elevation-candidates).
Water confluence construction is a separate bounded operation within this same
owner, with [explicit source and native-consumer limits](../terrain_refinement.md#water-confluence-candidates).
It does not publish or adopt a production refinement.
Pure constructed-basin modeling has [its own containment and publication limits](../terrain_refinement.md#contained-standing-water-construction).
