# Refactor Inventory Presentation Contracts

Status: uninvestigated

## Current gap

ProjectInventoryUI still infers player-storage grouping and ordering from
container/tag conventions. The ViewModel and panel therefore share domain
decisions that should come from one read-only presentation contract.

## Investigation boundary

- Model hands, compact storage, and large storage as explicit ordered
  descriptors owned by the inventory boundary.
- Keep nearby world storage outside the player-storage descriptor list.
- Let ProjectInventoryUI render descriptors without owning equip semantics.
- Extend ProjectUI layout primitives only if a reusable responsive-width need
  is proven.
- Remove compatibility fields in the same accepted migration; do not retain
  parallel presentation authorities.

Start from the [inventory behavior owner](../../../Plugins/Features/ProjectInventory/docs/design_vision.md)
and [inventory UI architecture](../../../Plugins/UI/ProjectInventoryUI/docs/architecture.md).
