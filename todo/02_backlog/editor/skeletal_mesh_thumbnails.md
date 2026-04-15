# Pre-generate SkeletalMesh thumbnails during asset generation

## Problem

Custom thumbnail renderers cannot render FSkeletalMeshThumbnailScene without GPU hangs.
Skeletal render state initializes asynchronously across multiple frames, but custom renderers
only get single-frame Draw() calls. The engine's USkeletalMeshThumbnailRenderer works because
the ThumbnailPool manages Realtime frequency + multi-frame integration. Delegation from a
custom renderer bypasses this -> black thumbnails or D3D12 TDR crashes.

## Solution

Capture the SkeletalMesh thumbnail during ObjectDefinition .uasset generation and store it
in the package. The Content Browser then uses the cached thumbnail with no runtime rendering.

## Implementation

In the definition generator (after creating the UObjectDefinition asset):

1. Load the first mesh from `Def->Meshes[0].Asset`
2. If it is a USkeletalMesh, render its thumbnail via the engine pipeline:
   ```cpp
   FObjectThumbnail Thumbnail;
   ThumbnailTools::RenderThumbnail(SkeletalMesh, 256, 256, ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush, nullptr, &Thumbnail);
   ```
3. Cache it on the ObjectDefinition's package:
   ```cpp
   UPackage* Package = Def->GetOutermost();
   Package->SetThumbnail(Def->GetFullName(), &Thumbnail);
   Package->MarkPackageDirty();
   ```
4. Save the package (already done by the generator)

## Affected files

- Generator: `Plugins/Editor/ProjectDefinitionGenerator/` (add thumbnail capture step)
- Renderer: `Plugins/Editor/ProjectPlacementEditor/.../DefinitionThumbnailRenderer.cpp`
  (currently skips SkeletalMesh - remove skip once pre-generation is in place)

## References

- `ThumbnailTools::RenderThumbnail()` in `Engine/Source/Editor/UnrealEd/Public/ThumbnailTools.h`
- `ThumbnailTools::CacheThumbnail()` for package-level caching
- `FObjectThumbnail` in `Engine/Source/Runtime/Core/Public/Misc/ObjectThumbnail.h`
