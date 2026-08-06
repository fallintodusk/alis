// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldSemanticEvidence.h"

#include "ProjectWorldRealizationService.h"
#include "Utilities/ProjectSha256.h"

#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "Misc/PackageName.h"
#include "ProceduralMeshComponent.h"

namespace ProjectWorldSemanticEvidence
{
	const FName GeneratedTag(TEXT("ProjectWorld.Generated.v1"));

	FString StableTransform(const FTransform& Transform)
	{
		const FVector Location = Transform.GetLocation();
		const FQuat Rotation = Transform.GetRotation();
		const FVector Scale = Transform.GetScale3D();
		return FString::Printf(
			TEXT("L=%.9g,%.9g,%.9g|R=%.9g,%.9g,%.9g,%.9g|S=%.9g,%.9g,%.9g"),
			Location.X,
			Location.Y,
			Location.Z,
			Rotation.X,
			Rotation.Y,
			Rotation.Z,
			Rotation.W,
			Scale.X,
			Scale.Y,
			Scale.Z);
	}

	bool HashBytes(const void* Data, int32 ByteCount, FString& OutHash)
	{
		TArray<uint8> Bytes;
		Bytes.Append(static_cast<const uint8*>(Data), ByteCount);
		return FProjectSha256::HashBuffer(Bytes, OutHash);
	}

	bool AppendLandscape(
		ALandscape* Landscape,
		FProjectWorldRealizationResult& OutResult,
		TArray<FString>& Records,
		FString& OutError)
	{
		Records.Add(FString::Printf(
			TEXT("landscape|components=%d|component_quads=%d|subsections=%d|section_quads=%d"),
			Landscape->LandscapeComponents.Num(),
			Landscape->ComponentSizeQuads,
			Landscape->NumSubsections,
			Landscape->SubsectionSizeQuads));
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		int32 MinimumX = 0;
		int32 MinimumY = 0;
		int32 MaximumX = 0;
		int32 MaximumY = 0;
		if (LandscapeInfo == nullptr ||
			!LandscapeInfo->GetLandscapeExtent(MinimumX, MinimumY, MaximumX, MaximumY))
		{
			OutError = TEXT("Cannot read generated Landscape extent for semantic evidence.");
			return false;
		}
		const int32 Width = MaximumX - MinimumX + 1;
		const int32 Height = MaximumY - MinimumY + 1;
		for (const ULandscapeEditLayerBase* Layer : Landscape->GetEditLayersConst())
		{
			TArray<uint16> LayerHeights;
			LayerHeights.SetNumUninitialized(Width * Height);
			FLandscapeEditDataInterface LayerData(LandscapeInfo, Layer->GetGuid(), false);
			LayerData.GetHeightDataFast(
				MinimumX,
				MinimumY,
				MaximumX,
				MaximumY,
				LayerHeights.GetData(),
				Width);
			FString LayerHash;
			if (!HashBytes(
				LayerHeights.GetData(),
				LayerHeights.Num() * sizeof(uint16),
				LayerHash))
			{
				OutError = TEXT("Cannot hash generated Landscape edit-layer heights.");
				return false;
			}
			Records.Add(FString::Printf(
				TEXT("layer|%s|%s"),
				*Layer->GetName().ToString(),
				*LayerHash));
			if (Layer->GetName() == FName(TEXT("Authored Corrections")))
			{
				OutResult.AuthoredCorrectionLayerHash = LayerHash;
			}
		}
		return true;
	}

	void AppendProceduralMesh(
		const FGuid& ActorGuid,
		UProceduralMeshComponent* ProceduralMesh,
		TArray<FString>& Records)
	{
		for (int32 SectionIndex = 0; SectionIndex < ProceduralMesh->GetNumSections(); ++SectionIndex)
		{
			const FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(SectionIndex);
			if (Section == nullptr)
			{
				continue;
			}
			FString Geometry = FString::Printf(
				TEXT("mesh|%s|section=%d|"),
				*ActorGuid.ToString(EGuidFormats::Digits),
				SectionIndex);
			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				Geometry += FString::Printf(
					TEXT("v=%.9g,%.9g,%.9g;"),
					Vertex.Position.X,
					Vertex.Position.Y,
					Vertex.Position.Z);
			}
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				Geometry += FString::Printf(TEXT("i=%u;"), Index);
			}
			Records.Add(MoveTemp(Geometry));
		}
	}

	bool Capture(
		UWorld* World,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		TArray<FString> Records;
		TSet<FString> PackageNames;
		TSet<FString> PackageFiles;
		auto AddPackageFile = [&PackageFiles](const FString& Filename)
		{
			FString Normalized = FPaths::ConvertRelativePathToFull(Filename);
			FPaths::NormalizeFilename(Normalized);
			PackageFiles.Add(MoveTemp(Normalized));
		};
		PackageNames.Add(World->GetOutermost()->GetName());
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor->Tags.Contains(GeneratedTag))
			{
				continue;
			}

			TArray<FString> Tags;
			for (const FName& Tag : Actor->Tags)
			{
				Tags.Add(Tag.ToString());
			}
			Tags.Sort();
			Records.Add(FString::Printf(
				TEXT("actor|%s|%s|%s|spatial=%d|auto_hlod=%d|%s"),
				*Actor->GetActorGuid().ToString(EGuidFormats::DigitsWithHyphensLower),
				*Actor->GetClass()->GetPathName(),
				*StableTransform(Actor->GetActorTransform()),
				Actor->GetIsSpatiallyLoaded() ? 1 : 0,
				Actor->bEnableAutoLODGeneration ? 1 : 0,
				*FString::Join(Tags, TEXT(","))));
			PackageNames.Add(Actor->GetOutermost()->GetName());

			TInlineComponentArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				UProceduralMeshComponent* ProceduralMesh =
					Cast<UProceduralMeshComponent>(Component);
				const FString RuntimeState = ProceduralMesh == nullptr
					? FString()
					: FString::Printf(
						TEXT("|collision=%d|object=%d|nav=%d|complex=%d|mobility=%d"),
						static_cast<int32>(ProceduralMesh->GetCollisionEnabled()),
						static_cast<int32>(ProceduralMesh->GetCollisionObjectType()),
						ProceduralMesh->CanEverAffectNavigation() ? 1 : 0,
						ProceduralMesh->bUseComplexAsSimpleCollision ? 1 : 0,
						static_cast<int32>(ProceduralMesh->GetMobility()));
				Records.Add(FString::Printf(
					TEXT("component|%s|%s|sections=%d%s"),
					*Actor->GetActorGuid().ToString(EGuidFormats::Digits),
					*Component->GetClass()->GetPathName(),
					ProceduralMesh == nullptr ? -1 : ProceduralMesh->GetNumSections(),
					*RuntimeState));
				if (ProceduralMesh != nullptr)
				{
					AppendProceduralMesh(Actor->GetActorGuid(), ProceduralMesh, Records);
				}
			}

			if (ALandscape* Landscape = Cast<ALandscape>(Actor))
			{
				if (!AppendLandscape(Landscape, OutResult, Records, OutError))
				{
					return false;
				}
			}
		}

		Records.Sort();
		const FString SemanticText = FString::Join(Records, TEXT("\n"));
		FTCHARToUTF8 Utf8(*SemanticText);
		TArray<uint8> SemanticBytes;
		SemanticBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		if (!FProjectSha256::HashBuffer(SemanticBytes, OutResult.SemanticFingerprint))
		{
			OutError = TEXT("Cannot hash generated world semantics.");
			return false;
		}

		for (const FString& PackageName : PackageNames)
		{
			FString Filename;
			if (FPackageName::DoesPackageExist(PackageName, &Filename))
			{
				AddPackageFile(Filename);
			}
		}

		const FString MapPackageName = World->GetOutermost()->GetName();
		FString MapFilename;
		if (FPackageName::DoesPackageExist(MapPackageName, &MapFilename))
		{
			const FString MapDirectory = FPaths::GetPath(MapFilename);
			const FString MapBaseName = FPaths::GetBaseFilename(MapFilename);
			TArray<FString> MapSiblings;
			IFileManager::Get().FindFiles(
				MapSiblings,
				*(FPaths::Combine(MapDirectory, MapBaseName) + TEXT("*")),
				true,
				false);
			for (const FString& Sibling : MapSiblings)
			{
				AddPackageFile(FPaths::Combine(MapDirectory, Sibling));
			}

			FString RelativePackagePath = MapPackageName;
			RelativePackagePath.RemoveFromStart(TEXT("/"));
			int32 MountSeparator = INDEX_NONE;
			if (RelativePackagePath.FindChar(TEXT('/'), MountSeparator))
			{
				RelativePackagePath.RightChopInline(MountSeparator + 1);
				FString RelativeMapFilename = RelativePackagePath + FPackageName::GetMapPackageExtension();
				FPaths::MakePlatformFilename(RelativeMapFilename);
				FString NormalizedMapFilename = MapFilename;
				FPaths::MakePlatformFilename(NormalizedMapFilename);
				if (NormalizedMapFilename.EndsWith(RelativeMapFilename))
				{
					FString ContentRoot = NormalizedMapFilename.LeftChop(RelativeMapFilename.Len());
					ContentRoot.RemoveFromEnd(TEXT("/"));
					ContentRoot.RemoveFromEnd(TEXT("\\"));
					for (const TCHAR* ExternalRootName : {TEXT("__ExternalActors__"), TEXT("__ExternalObjects__")})
					{
						const FString ExternalRoot = FPaths::Combine(
							ContentRoot,
							ExternalRootName,
							RelativePackagePath);
						TArray<FString> ExternalFiles;
						IFileManager::Get().FindFilesRecursive(
							ExternalFiles,
							*ExternalRoot,
							TEXT("*"),
							true,
							false,
							false);
						for (const FString& ExternalFile : ExternalFiles)
						{
							AddPackageFile(ExternalFile);
						}
					}
				}
			}
		}

		OutResult.GeneratedSourceBytes = 0;
		for (const FString& Filename : PackageFiles)
		{
			const int64 FileSize = IFileManager::Get().FileSize(*Filename);
			if (FileSize > 0)
			{
				OutResult.GeneratedSourceBytes += FileSize;
			}
		}
		return true;
	}
}
