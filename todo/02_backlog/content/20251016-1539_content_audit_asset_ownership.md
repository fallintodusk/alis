# Audit Content Asset Ownership

Status: uninvestigated

## Current gap

Top-level game content may still contain duplicated marketplace samples,
inconsistent ownership, and obsolete demo assets. File names alone cannot prove
that an Unreal asset is unused.

## Investigation boundary

- Inventory content by plugin/game owner and primary asset/cook reachability.
- Detect duplicate payloads by hash, then trace hard, soft, Blueprint, map, and
  material references before proposing a move or deletion.
- Keep reusable assets with their component owner and ALIS-only composition in
  game content.
- Migrate references and validate Editor load, cook, and packaged routes in one
  bounded slice.

Do not reorganize binary assets from directory appearance alone.
