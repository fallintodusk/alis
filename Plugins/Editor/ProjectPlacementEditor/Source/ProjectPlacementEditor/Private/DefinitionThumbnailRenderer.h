// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "DefinitionThumbnailRenderer.generated.h"

/**
 * Base thumbnail renderer for definition assets.
 * Subclasses provide GetMeshForThumbnail() - this class handles caching and delegation.
 *
 * Rendering strategy:
 * - Resolve mesh from definition (static or skeletal) and delegate to mesh renderer.
 * - Guard against recursion if mesh resolution accidentally points back to definition.
 */
UCLASS(Abstract)
class UDefinitionThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
		FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;

	virtual bool CanVisualizeAsset(UObject* Object) override;

	/** Clears runtime thumbnail renderer caches (mesh resolve + warning dedupe). */
	static void ResetRuntimeCaches();

	// IMPORTANT: Do not use EThumbnailRenderFrequency::Once here.
	// In UE5 thumbnail pool flow, Once can skip the actual RenderThumbnail() enqueue path
	// for custom renderers, so Draw() is never called.
	// Realtime is required for stable tile behavior with UE hover/scroll thumbnail flow.
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;

protected:
	/** Return mesh to use for thumbnail. Result is weak-cached and re-resolved if cache expires. */
	virtual UObject* GetMeshForThumbnail(UObject* Object) const PURE_VIRTUAL(UDefinitionThumbnailRenderer::GetMeshForThumbnail, return nullptr;);

	/** Return unique ID for caching (e.g., ItemId, ObjectId). */
	virtual FName GetCacheKey(UObject* Object) const PURE_VIRTUAL(UDefinitionThumbnailRenderer::GetCacheKey, return NAME_None;);
};
