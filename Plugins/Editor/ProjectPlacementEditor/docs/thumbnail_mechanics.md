# Thumbnail Mechanics

## Purpose

This document is the source of truth for ObjectDefinition thumbnail behavior in ProjectPlacementEditor.

## Engine Behavior (UE 5.7)

Critical engine flow (verified in engine source):

- `SAssetTileItem::OnMouseEnter` calls `AssetThumbnail->SetRealTime(true)`.
- `SAssetTileItem::OnMouseLeave` calls `AssetThumbnail->SetRealTime(false)`.
- `FAssetThumbnailPool::LoadThumbnail` chooses render path based on renderer frequency.
- `SAssetThumbnail::OnThumbnailRenderFailed` can flip a tile back to generic class icon.

Relevant engine files:

- `Engine/Source/Editor/ContentBrowser/Private/AssetViewWidgets.cpp`
- `Engine/Source/Editor/UnrealEd/Private/AssetThumbnail.cpp`

## Plugin Implementation Decisions

### 1. Delegate to native mesh renderers

`UDefinitionThumbnailRenderer::Draw` resolves the mesh object and delegates to the mesh renderer returned by `UThumbnailManager`.

Why:

- Keeps static/skeletal rendering logic in native engine renderers.
- Avoids custom scene/render code duplication.

### 2. Robust mesh selection

`UObjectDefinitionThumbnailRenderer::GetMeshForThumbnail` scans mesh entries and picks the first supported mesh type (`UStaticMesh` or `USkeletalMesh`).

Why:

- Some definitions do not have a valid preview mesh at index 0.
- Prevents false blank tiles caused by fixed-index assumptions.

### 3. Collision-safe thumbnail cache key

Cache key uses:

- `ObjectId` + short deterministic suffix (CRC32 of asset path).

Why:

- Prevents collisions when multiple assets share the same `ObjectId`.
- Keeps key short and stable.

### 4. Frequency strategy

Renderer frequency is `EThumbnailRenderFrequency::Realtime`.

Why:

- Hover and scroll in tile view force realtime requests.
- `OnPropertyChange` can produce false-failure fallback in this flow and cause icon loss on hover/scroll.

### 5. Warmup on panel visibility and filter changes

`SProjectObjectBrowser` runs a small number of delayed refresh passes when:

- panel appears
- manual refresh
- path/filter changes
- post AssetRegistry tag-ready refresh

Why:

- Prewarms visible thumbnails without requiring manual hover.
- Balances responsiveness and render load.

### 6. UI label behavior

Tile class label is disabled:

- `ThumbnailLabel = EThumbnailLabel::NoLabel`
- `bCanShowClasses = false`

Why:

- Removes `Data Asset` class text from object tiles.

### 7. Startup stale-thumbnail cleanup

At module startup, stale ObjectDefinition package thumbnail maps are cleared (loading packages when needed before clearing).

Why:

- Prevents old baked thumbnails from overriding current renderer results.

## Known Limits

- During very fast scrolling, temporary placeholder icons can appear due to thumbnail pool frame budget limits.
- This is expected async behavior and should recover after scrolling stops.

## Troubleshooting

If thumbnails disappear or revert to generic icon:

1. Restart editor once after thumbnail code changes.
2. Confirm renderer is registered in `StartupModule`.
3. Check logs for:
   - `LogDefinitionThumbnail`
   - `LogObjDefThumbnail`
4. Verify problematic definition has at least one loadable static/skeletal mesh reference.

Useful warnings:

- `[Draw] ... failed to resolve mesh`
- `[Draw] ... no valid renderer`
- `[GetMesh] ... no supported mesh found`
