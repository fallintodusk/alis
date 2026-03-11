// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DefinitionThumbnailRenderer.h"
#include "ObjectDefinitionThumbnailRenderer.generated.h"

/**
 * Thumbnail renderer for UObjectDefinition.
 * Uses first supported mesh entry (static or skeletal) for thumbnail.
 * Cache key uses ObjectId plus short path fingerprint to avoid collisions.
 * TODO: Future - add explicit icon field to JSON schema.
 */
UCLASS()
class UObjectDefinitionThumbnailRenderer : public UDefinitionThumbnailRenderer
{
	GENERATED_BODY()

protected:
	virtual UObject* GetMeshForThumbnail(UObject* Object) const override;
	virtual FName GetCacheKey(UObject* Object) const override;
};
