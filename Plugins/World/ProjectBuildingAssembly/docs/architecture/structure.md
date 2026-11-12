# Modular building prototype

## Boundary

This editor-only component composes a bounded rectangular building layout and
realizes it with native instanced Static Mesh components. It owns original
procedural wall, open window-frame, floor and roof pieces. It does not own
canonical geography, Terrain modification, massing admission or city publication.
The [standing assembly contract](../decision_record.md) owns the accepted product
direction and production admission requirements.

## Parts

| Part | Owner |
|---|---|
| Engine-independent C++20 layout and native realization | [Module](../../Source/ProjectBuildingAssemblyEditor/README.md) |
| Original procedural test kit and schema | [Data](../../Data/README.md) |
| Portable and source-bound native verification | [Tests](../../Tests/README.md) |

The [consumption view](diagrams/main.md) owns the relationship graph. The World
adapter uses public canonical types and the physical triangle surface. Assembly
does not depend on ProjectWorld. Concrete editor composition follows the project
[dependency rules](../../../../../docs/architecture/plugin_rules.md).

## Invariants

The layout core has one implementation shared by the host tests and native
editor module. Its closed dimensions and capacities refuse without a partial
plan. Floors meet adjacent walls; a bounded foundation spans the admitted Terrain
extrema, and the remaining layout stays within the supplied volume. Window
modules are open frames, not inferred glass or detailed architectural survey.

World qualifies a complete inscribed rectangle against the source volume and
physical Terrain triangles, including total coverage and height extrema. It
refuses unsupported shapes, missing support, excessive support span, explicit
civil grades, authored selection and unresolved Water overlap. It performs no
Terrain edits. Building overlap classification precedes selected massing exclusion;
the selection and kit identities stay separate.

Native reuse compares actual meshes, instance/component transforms, material,
collision configuration, normals and generated ownership. A stale tag cannot establish a
no-op. A replacement is staged and checked before the previous owned actor is
removed. An unowned feature-associated actor refuses replacement.

The World buildings producer can consume this component through its opt-in
selection contract. World owns shared persistent kit assets, selected external
actors and remaining blockouts in one layer transaction. Assembly provides module
creation, actual-reference validation and stable actor updates; its caller owns
rollback for updates to external actors. The isolated scene retains its staged
replacement path. Concrete city/package admission, general footprint decomposition,
multivolume buildings and Terrain grading require their own bounded work.

The current kit reader deliberately qualifies the original test kit against a
small 10 x 10 x 6 m probe. It is not general asset intake: a kit that only fits a
different building can refuse there. General kit admission remains a separate
extension. The World selection document is bounded to five supported buildings;
the registered lifecycle fixture exercises one, then three.
Kit-only preparation reclassifies remaining massing to verify its unchanged
identity. Actual assembly writes follow the existing per-cell dirty plan and
unchanged shared module packages are reused. This does not claim sparse total
validation I/O or a general incremental compiler.

## Terrain influence boundary

Epic's [PCG city example](https://dev.epicgames.com/documentation/unreal-engine/city-sample-pcg-for-unreal-engine)
demonstrates staged mesh terrain and object generation. Its
[Landscape Patch system](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-patch-system)
modifies Landscape heightmaps, which is a different realization backend.
Neither is an alternate Mesh Terrain writer here. A future generator needing
Terrain adaptation must submit qualified intent to the existing canonical owner,
then consume the resulting final support. This prototype instead reports a bounded
support refusal; it provides no influence API or hidden correction loop.
Epic's current documentation may target a newer engine than the project; examples
and experimental PCG capabilities must be qualified against the actual installed
engine before adoption. [PCG shape grammar](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine)
is an available future composition frontend; this bounded layout proof does not
require a graph asset or a general grammar engine.
