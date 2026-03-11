// Copyright ALIS. All Rights Reserved.

#include "ObjectDefinitionThumbnailRenderer.h"
#include "Data/ObjectDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Crc.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjDefThumbnail, Log, All);
static TSet<FName> LoggedMeshResolveFailureKeys;

namespace
{
FName BuildStableThumbnailKey(const UObjectDefinition* Def, const UObject* Object)
{
	if (!Object)
	{
		return NAME_None;
	}

	const FString IdPart = (Def && !Def->ObjectId.IsNone()) ? Def->ObjectId.ToString() : Object->GetName();
	const uint32 PathHash = FCrc::StrCrc32(*Object->GetPathName());
	return FName(*FString::Printf(TEXT("%s_%08X"), *IdPart, PathHash));
}
}

UObject* UObjectDefinitionThumbnailRenderer::GetMeshForThumbnail(UObject* Object) const
{
	UObjectDefinition* Def = Cast<UObjectDefinition>(Object);
	const FName LogKey = BuildStableThumbnailKey(Def, Object);

	if (!Def)
	{
		UE_LOG(LogObjDefThumbnail, Warning,
			TEXT("[GetMesh] '%s': not an ObjectDefinition"),
			*GetNameSafe(Object));
		return nullptr;
	}

	if (Def->Meshes.Num() == 0)
	{
		UE_LOG(LogObjDefThumbnail, Warning,
			TEXT("[GetMesh] '%s': no meshes (Def=%d, MeshCount=%d)"),
			*GetNameSafe(Object),
			Def != nullptr,
			Def ? Def->Meshes.Num() : 0);
		return nullptr;
	}

	FString LastFailureReason;
	for (int32 MeshIndex = 0; MeshIndex < Def->Meshes.Num(); ++MeshIndex)
	{
		const FObjectMeshEntry& MeshEntry = Def->Meshes[MeshIndex];
		const FSoftObjectPath& AssetPath = MeshEntry.Asset.ToSoftObjectPath();
		UObject* Loaded = MeshEntry.Asset.LoadSynchronous();

		if (!Loaded)
		{
			LastFailureReason = FString::Printf(TEXT("failed to load path='%s' (index=%d)"), *AssetPath.ToString(), MeshIndex);
			continue;
		}

		if (Loaded->IsA<UStaticMesh>() || Loaded->IsA<USkeletalMesh>())
		{
			LoggedMeshResolveFailureKeys.Remove(LogKey);
			return Loaded;
		}

		LastFailureReason = FString::Printf(
			TEXT("unsupported class='%s' path='%s' (index=%d)"),
			*Loaded->GetClass()->GetName(),
			*AssetPath.ToString(),
			MeshIndex);
	}

	if (!LoggedMeshResolveFailureKeys.Contains(LogKey))
	{
		LoggedMeshResolveFailureKeys.Add(LogKey);
		UE_LOG(LogObjDefThumbnail, Warning,
			TEXT("[GetMesh] '%s': no supported mesh found in %d entries (%s)"),
			*Def->ObjectId.ToString(),
			Def->Meshes.Num(),
			LastFailureReason.IsEmpty() ? TEXT("unknown reason") : *LastFailureReason);
	}

	return nullptr;
}

FName UObjectDefinitionThumbnailRenderer::GetCacheKey(UObject* Object) const
{
	return BuildStableThumbnailKey(Cast<UObjectDefinition>(Object), Object);
}
