// Copyright ALIS. All Rights Reserved.

#include "DefinitionThumbnailRenderer.h"
#include "ThumbnailRendering/ThumbnailManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionThumbnail, Log, All);

// Shared cache across all definition thumbnail renderers
static TMap<FName, TWeakObjectPtr<UObject>> MeshCache;
static TSet<FName> LoggedMissingMeshKeys;
static TSet<FName> LoggedNoRendererKeys;

void UDefinitionThumbnailRenderer::ResetRuntimeCaches()
{
	MeshCache.Empty();
	LoggedMissingMeshKeys.Empty();
	LoggedNoRendererKeys.Empty();
}

void UDefinitionThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	if (!Object)
	{
		return;
	}

	FName CacheKey = GetCacheKey(Object);
	if (CacheKey.IsNone())
	{
		// Fallback to object path if definition-specific key is missing.
		CacheKey = FName(*Object->GetPathName());
	}

	if (CacheKey.IsNone())
	{
		UE_LOG(LogDefinitionThumbnail, Warning, TEXT("[Draw] CacheKey is None for '%s'"), *GetNameSafe(Object));
		return;
	}

	UThumbnailManager& ThumbnailMgr = UThumbnailManager::Get();
	UObject* MeshToRender = nullptr;

	if (TWeakObjectPtr<UObject>* Cached = MeshCache.Find(CacheKey))
	{
		MeshToRender = Cached->Get();
	}

	// Retry resolve when cache is empty/expired, otherwise some tiles can stay blank forever.
	if (!MeshToRender)
	{
		MeshToRender = GetMeshForThumbnail(Object);
		if (MeshToRender)
		{
			MeshCache.Add(CacheKey, MeshToRender);
			LoggedMissingMeshKeys.Remove(CacheKey);
		}
	}

	if (!MeshToRender && !LoggedMissingMeshKeys.Contains(CacheKey))
	{
		LoggedMissingMeshKeys.Add(CacheKey);
		UE_LOG(LogDefinitionThumbnail, Warning,
			TEXT("[Draw] '%s': failed to resolve mesh, using EditorCube fallback"),
			*CacheKey.ToString());
	}

	if (!MeshToRender)
	{
		MeshToRender = ThumbnailMgr.EditorCube;
	}

	if (!MeshToRender)
	{
		UE_LOG(LogDefinitionThumbnail, Warning,
			TEXT("[Draw] '%s': mesh is null, skipping"),
			*CacheKey.ToString());
		return;
	}

	// Delegate to mesh renderer (static/skeletal/etc). Guard recursion in case
	// a bad reference resolves back to definition asset type.
	FThumbnailRenderingInfo* RenderInfo = ThumbnailMgr.GetRenderingInfo(MeshToRender);
	if (RenderInfo && RenderInfo->Renderer && RenderInfo->Renderer != this)
	{
		RenderInfo->Renderer->Draw(MeshToRender, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
	}
	else
	{
		if (!LoggedNoRendererKeys.Contains(CacheKey))
		{
			LoggedNoRendererKeys.Add(CacheKey);
			UE_LOG(LogDefinitionThumbnail, Warning,
				TEXT("[Draw] '%s': no valid renderer for mesh='%s' (class=%s)"),
				*CacheKey.ToString(),
				*GetNameSafe(MeshToRender),
				MeshToRender ? *MeshToRender->GetClass()->GetName() : TEXT("null"));
		}
	}
}

bool UDefinitionThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	// Log first 5 calls to confirm our code is loaded and UE is querying us
	static int32 CanVisualizeCallCount = 0;
	if (CanVisualizeCallCount < 5)
	{
		UE_LOG(LogDefinitionThumbnail, Log,
			TEXT("[CanVisualize] #%d: '%s' -> true"),
			++CanVisualizeCallCount, *GetNameSafe(Object));
	}
	return Object != nullptr;
}

EThumbnailRenderFrequency UDefinitionThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	// Important engine behavior (UE5.7):
	// - SAssetTileItem::OnMouseEnter always calls AssetThumbnail->SetRealTime(true)
	// - FAssetThumbnailPool::LoadThumbnail with OnPropertyChange may skip enqueue when
	//   LastUpdateTime > 0 and then fail, which triggers OnThumbnailRenderFailed and
	//   flips tile back to generic class icon.
	// Realtime avoids that false failure path and keeps thumbnail visible on hover/scroll.
	return EThumbnailRenderFrequency::Realtime;
}
