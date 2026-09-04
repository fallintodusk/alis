# ProjectExperienceData

Data/content-only owner for concrete configured application experiences.

## Ownership

- `Data/Experiences/` holds the JSON source of truth for each configured experience:
  identity, map, traversal token, and asset scan directory.
- `Data/Schemas/experience.schema.json` is the contract those records satisfy. Its
  `x-alis-generator` block is what registers the type with ProjectDefinitionGenerator.
- `Content/Experiences/` holds the generated `UProjectExperienceDefinition` assets. They
  are generated, never hand-edited, and they are the cooked runtime projection.

This plugin has no `Source` module, and must not gain one. Contracts and logic stay with
the component that interprets them: ProjectCore owns the `UProjectExperienceDefinition`
data contract, ProjectLoading owns discovery/registration and load-request projection, and
ProjectSinglePlay owns what a traversal token such as `PreviewFlight` means.

## Why this is not ProjectWorldData

ProjectWorldData owns geography and sourced semantic facts only. A traversal or product
selection may not be placed there merely because a territory selected it. An experience
record is product composition, not geography, so it lives here and refers to a world by
package path.

## Why JSON is the source of truth

ALIS is text-first: the JSON record is reviewable, diffable, and present in the public
source tree, while the cooked asset is a derived projection. Runtime never reads these
JSON files, so they need no Shipping staging - only the generated assets are cooked.

## Adding an experience

1. Add one JSON record under `Data/Experiences/`.
2. Run ProjectDefinitionGenerator (Tools -> Project Definition Generator -> Generate All,
   or the `GenerateDefinitions` commandlet).
3. Add a main-menu button with `"action": "loadexperience:<ExperienceName>"`.

Adding another experience that fits this configured contract requires no C++ class,
registration line, or menu handler.

An experience with genuinely custom load behavior may still be a plugin-owned descriptor
class registered by its own module, the way City17 and MainMenuWorld do it. Those keep
working and take precedence over a configured record of the same id. This contract covers
configured experiences, not every possible one.

Ids must be unique across all configured records: the id names the generated asset and
becomes its `FPrimaryAssetId`. ProjectDefinitionGenerator rejects duplicates at generation
time, which is the authority for that rule.
