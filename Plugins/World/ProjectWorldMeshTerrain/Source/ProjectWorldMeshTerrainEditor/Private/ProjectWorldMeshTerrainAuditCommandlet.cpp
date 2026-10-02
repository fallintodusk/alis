// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldMeshTerrainAuditCommandlet.h"

#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainTransformer.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainRuntimeRole.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "MeshPartition.h"
#include "MeshPartitionDefinition.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "String/LexFromString.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldMeshTerrainAudit, Log, All);

namespace
{
	constexpr double FallbackSurfaceToleranceCentimeters = 5.0;

	struct FSurfaceProbe
	{
		FString Actor;
		FBoxSphereBounds Bounds;
		TArray<FVector> Vertices;
	};

	struct FHeightProbeResult
	{
		int32 ExpectedSamples = 0;
		int32 MatchedSamples = 0;
		int32 MismatchedSamples = 0;
		double MaximumErrorCentimeters = 0.0;
	};

	FIntPoint SurfaceKey(const FVector& Position)
	{
		constexpr double CoordinateScale = 10.0;
		return FIntPoint(
			FMath::RoundToInt(Position.X * CoordinateScale),
			FMath::RoundToInt(Position.Y * CoordinateScale));
	}

	FHeightProbeResult CompareCanonicalHeights(
		const FProjectWorldCanonicalBundle& Bundle,
		const TArray<FSurfaceProbe>& Surfaces,
		const double ToleranceCentimeters)
	{
		TMap<FIntPoint, double> ActualHeights;
		for (const FSurfaceProbe& Surface : Surfaces)
		{
			for (const FVector& Vertex : Surface.Vertices)
			{
				double& Height = ActualHeights.FindOrAdd(SurfaceKey(Vertex), Vertex.Z);
				Height = FMath::Max(Height, Vertex.Z);
			}
		}

		TMap<FIntPoint, double> ExpectedHeights;
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
			{
				for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
				{
					const int32 Index = Row * Cell.Terrain.SamplesX + Column;
					const FVector Expected = FProjectWorldCanonicalLoader::CanonicalToUnreal(
						Bundle,
						FVector(
							Cell.Terrain.Bounds.X + Column * Cell.Terrain.SampleSpacing.X,
							FProjectWorldCanonicalLoader::TerrainRowNorthing(Cell.Terrain, Row),
							Cell.Terrain.HeightsMeters[Index]));
					ExpectedHeights.FindOrAdd(SurfaceKey(Expected), Expected.Z);
				}
			}
		}

		FHeightProbeResult Result;
		Result.ExpectedSamples = ExpectedHeights.Num();
		for (const TPair<FIntPoint, double>& Expected : ExpectedHeights)
		{
			const double* Actual = ActualHeights.Find(Expected.Key);
			if (Actual == nullptr)
			{
				++Result.MismatchedSamples;
				continue;
			}
			++Result.MatchedSamples;
			const double Error = FMath::Abs(*Actual - Expected.Value);
			Result.MaximumErrorCentimeters = FMath::Max(Result.MaximumErrorCentimeters, Error);
			if (Error > ToleranceCentimeters)
			{
				++Result.MismatchedSamples;
			}
		}
		return Result;
	}

	bool IsSafeResultPath(const FString& ResultPath)
	{
		const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
		return FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(ResultPath), EvidenceRoot);
	}

	UWorld* LoadPrototypeWorld(const FString& MapPackagePath, FString& OutError)
	{
		if (!UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(MapPackagePath))
		{
			OutError = TEXT("Mesh Terrain audit map must stay under a ProjectWorld generated-content root.");
			return nullptr;
		}
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			MapPackagePath, FPackageName::GetMapPackageExtension());
		if (!IFileManager::Get().FileExists(*MapFilename))
		{
			OutError = FString::Printf(TEXT("Mesh Terrain audit map does not exist: %s"), *MapFilename);
			return nullptr;
		}

		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FString> ExternalActorFiles;
		IFileManager::Get().FindFilesRecursive(
			ExternalActorFiles,
			*FPackageName::LongPackageNameToFilename(ULevel::GetExternalActorsPath(MapPackagePath)),
			TEXT("*.uasset"), true, false);
		AssetRegistry.ScanFilesSynchronous(ExternalActorFiles, true);
		if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
		{
			OutError = TEXT("Cannot load the prototype World Partition map.");
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	bool WriteReceipt(const FString& ResultPath, const TSharedPtr<FJsonObject>& Receipt)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (!FJsonSerializer::Serialize(Receipt.ToSharedRef(), Writer))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultPath), true);
		return FFileHelper::SaveStringToFile(
			Json, *ResultPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FName ReadBuildVariant(const AActor* Section)
	{
		const FStructProperty* BuildInfoProperty =
			FindFProperty<FStructProperty>(Section->GetClass(), TEXT("BuildInfo"));
		if (BuildInfoProperty == nullptr)
		{
			return NAME_None;
		}
		const FNameProperty* VariantProperty =
			FindFProperty<FNameProperty>(BuildInfoProperty->Struct, TEXT("BuildVariantName"));
		if (VariantProperty == nullptr)
		{
			return NAME_None;
		}
		const void* BuildInfo = BuildInfoProperty->ContainerPtrToValuePtr<void>(Section);
		return VariantProperty->GetPropertyValue_InContainer(BuildInfo);
	}

	int32 ReadArrayCount(const AActor* Section, const FName PropertyName)
	{
		const FArrayProperty* Property =
			FindFProperty<FArrayProperty>(Section->GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return -1;
		}
		const void* Array = Property->ContainerPtrToValuePtr<void>(Section);
		return FScriptArrayHelper(Property, Array).Num();
	}

	bool HasObjectProperty(const AActor* Section, const FName PropertyName)
	{
		const FObjectPropertyBase* Property =
			FindFProperty<FObjectPropertyBase>(Section->GetClass(), PropertyName);
		return Property != nullptr && Property->GetObjectPropertyValue_InContainer(Section) != nullptr;
	}

	bool MaterialUsesTerrainSource(
		const UMaterialInterface* Material,
		FString& OutSourcePath)
	{
		const UE::MeshPartition::UMeshPartitionDefinition* Definition =
			LoadObject<UE::MeshPartition::UMeshPartitionDefinition>(
				nullptr, ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
		const FString AcceptedPath = Definition != nullptr && Definition->GetMaterial() != nullptr
			? Definition->GetMaterial()->GetPathName() : FString();
		const UMaterialInterface* Current = Material;
		while (Current != nullptr)
		{
			OutSourcePath = Current->GetPathName();
			if (!AcceptedPath.IsEmpty() && OutSourcePath == AcceptedPath)
			{
				return true;
			}
			const UMaterialInstance* Instance = Cast<UMaterialInstance>(Current);
			Current = Instance != nullptr ? Instance->Parent : nullptr;
		}
		return false;
	}

	bool BuildSurfaceProbe(
		const AActor* Section,
		const UStaticMeshComponent* Component,
		FSurfaceProbe& OutProbe)
	{
		const UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		const FStaticMeshRenderData* RenderData = Mesh != nullptr ? Mesh->GetRenderData() : nullptr;
		if (RenderData == nullptr || RenderData->LODResources.IsEmpty())
		{
			return false;
		}
		const FPositionVertexBuffer& Positions =
			RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
		if (Positions.GetNumVertices() == 0)
		{
			return false;
		}
		OutProbe.Actor = Section->GetPathName();
		OutProbe.Bounds = Component->Bounds;
		OutProbe.Vertices.Reserve(Positions.GetNumVertices());
		for (uint32 Index = 0; Index < Positions.GetNumVertices(); ++Index)
		{
			OutProbe.Vertices.Add(Component->GetComponentTransform().TransformPosition(
				FVector(Positions.VertexPosition(Index))));
		}
		return true;
	}

	double DirectedSurfaceError(const TArray<FVector>& From, const TArray<FVector>& To)
	{
		double MaximumDistanceSquared = 0.0;
		for (const FVector& Point : From)
		{
			double NearestDistanceSquared = TNumericLimits<double>::Max();
			for (const FVector& Candidate : To)
			{
				NearestDistanceSquared = FMath::Min(
					NearestDistanceSquared, FVector::DistSquared(Point, Candidate));
			}
			MaximumDistanceSquared = FMath::Max(MaximumDistanceSquared, NearestDistanceSquared);
		}
		return FMath::Sqrt(MaximumDistanceSquared);
	}
}

bool UProjectWorldMeshTerrainAuditCommandlet::IsSupportedMapPackagePath(
	const FString& MapPackagePath)
{
	return FPackageName::IsValidLongPackageName(MapPackagePath) &&
		(MapPackagePath.StartsWith(TEXT("/ProjectWorldTestData/Generated/")) ||
		 MapPackagePath.StartsWith(TEXT("/ProjectWorldData/Generated/")));
}

UProjectWorldMeshTerrainAuditCommandlet::UProjectWorldMeshTerrainAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldMeshTerrainAuditCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);
	const FString* MapPackagePath = Parameters.Find(TEXT("Map"));
	const FString* ResultValue = Parameters.Find(TEXT("Result"));
	const FString* CompileResultValue = Parameters.Find(TEXT("CompileResult"));
	const FString* ExpectedSectionsValue = Parameters.Find(TEXT("ExpectedSectionsPerVariant"));
	int32 ExpectedSectionsPerVariant = 0;
	if (MapPackagePath == nullptr || ResultValue == nullptr || CompileResultValue == nullptr ||
		ExpectedSectionsValue == nullptr ||
		!LexTryParseString(ExpectedSectionsPerVariant, **ExpectedSectionsValue) ||
		ExpectedSectionsPerVariant < 0)
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error,
			TEXT("Usage requires -Map=<prototype-package> -CompileResult=<accepted-result> -ExpectedSectionsPerVariant=<non-negative-int> -Result=<saved-validation-json>."));
		return 2;
	}

	const FString ResultPath = FPaths::ConvertRelativePathToFull(*ResultValue);
	if (!IsSafeResultPath(ResultPath))
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error,
			TEXT("Unsafe result path: %s"), *ResultPath);
		return 2;
	}
	IFileManager::Get().Delete(*ResultPath, false, true);

	FString Error;
	FProjectWorldCanonicalBundle Bundle;
	FProjectWorldCanonicalValidation CanonicalValidation;
	if (!FProjectWorldCanonicalLoader::Load(*CompileResultValue, Bundle, CanonicalValidation))
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error,
			TEXT("Canonical bundle rejected: code=%s detail=%s"),
			*CanonicalValidation.ErrorCode, *CanonicalValidation.Detail);
		return 3;
	}
	UWorld* World = LoadPrototypeWorld(*MapPackagePath, Error);
	if (World == nullptr || !World->IsPartitionedWorld())
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error, TEXT("Map load rejected: %s"), *Error);
		return 3;
	}
	TArray<FWorldPartitionReference> LoadedActorReferences;
	World->GetWorldPartition()->LoadAllActors(LoadedActorReferences);

	TMap<FName, int32> VariantCounts;
	TArray<TSharedPtr<FJsonValue>> SectionsJson;
	TArray<FString> Failures;
	TArray<FSurfaceProbe> NaniteSurfaces;
	TArray<FSurfaceProbe> FallbackSurfaces;
	int32 PartitionCount = 0;
	for (TActorIterator<UE::MeshPartition::AMeshPartition> It(World); It; ++It)
	{
		if (It->GetMeshPartitionDefinition() != nullptr &&
			It->GetMeshPartitionDefinition()->GetPathName() ==
				ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath)
		{
			++PartitionCount;
		}
	}

	const UClass* CompiledSectionClass =
		LoadClass<AActor>(nullptr, TEXT("/Script/MeshPartition.CompiledSection"));
	if (CompiledSectionClass == nullptr)
	{
		Failures.Add(TEXT("compiled section reflection class is unavailable"));
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Section = *It;
		if (CompiledSectionClass == nullptr || !Section->IsA(CompiledSectionClass))
		{
			continue;
		}
		const FName Variant = ReadBuildVariant(Section);
		++VariantCounts.FindOrAdd(Variant);
		const bool bTagged = ProjectWorldTerrainRuntimeRole::HasRole(*Section);
		const bool bSpatiallyLoaded = Section->GetIsSpatiallyLoaded();
		const bool bHasChannelTexture = HasObjectProperty(Section, TEXT("ChannelTexture"));
		const int32 ChannelTableBytes = ReadArrayCount(Section, TEXT("ChannelTable"));
		const bool bHasChannelTable = ChannelTableBytes > 0;
		const int32 CollisionComponents = ReadArrayCount(Section, TEXT("CollisionComponents"));
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(Section);
		TArray<UStaticMesh*> StaticMeshes;
		for (const UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (UStaticMesh* Mesh = Component->GetStaticMesh())
			{
				StaticMeshes.AddUnique(Mesh);
				FSurfaceProbe Probe;
				if ((Variant == TEXT("nanite") || Variant == TEXT("fallback")) &&
					BuildSurfaceProbe(Section, Component, Probe))
				{
					(Variant == TEXT("nanite") ? NaniteSurfaces : FallbackSurfaces).Add(MoveTemp(Probe));
				}
			}
		}
		int32 NaniteMeshes = 0;
		FString SectionMaterial;
		FString SectionMaterialSource;
		for (const UStaticMesh* Mesh : StaticMeshes)
		{
			NaniteMeshes += Mesh != nullptr && Mesh->IsNaniteEnabled() ? 1 : 0;
		}
		for (const UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (UMaterialInterface* Material = Component->GetMaterial(0))
			{
				SectionMaterial = Material->GetPathName();
				if ((Variant == TEXT("nanite") || Variant == TEXT("fallback")) &&
					!MaterialUsesTerrainSource(Material, SectionMaterialSource))
				{
					Failures.Add(FString::Printf(TEXT("%s uses unexpected material %s"),
						*Section->GetPathName(), *SectionMaterial));
				}
			}
		}

		if (!bTagged)
		{
			Failures.Add(FString::Printf(TEXT("%s lacks terrain runtime tag"), *Section->GetPathName()));
		}
		if (!bSpatiallyLoaded)
		{
			Failures.Add(FString::Printf(TEXT("%s is not spatially loaded"), *Section->GetPathName()));
		}
		if (!bHasChannelTexture || !bHasChannelTable)
		{
			Failures.Add(FString::Printf(TEXT("%s lacks compiled channel data"), *Section->GetPathName()));
		}
		if (Variant == TEXT("collision") && CollisionComponents == 0)
		{
			Failures.Add(TEXT("collision variant has no collision component"));
		}
		if (Variant == TEXT("nanite") && (StaticMeshes.IsEmpty() || NaniteMeshes != StaticMeshes.Num()))
		{
			Failures.Add(TEXT("nanite variant does not contain only Nanite-enabled meshes"));
		}
		if (Variant == TEXT("fallback") && (StaticMeshes.IsEmpty() || NaniteMeshes != 0))
		{
			Failures.Add(TEXT("fallback variant lacks a non-Nanite static mesh"));
		}

		TSharedPtr<FJsonObject> SectionJson = MakeShared<FJsonObject>();
		SectionJson->SetStringField(TEXT("actor"), Section->GetPathName());
		SectionJson->SetStringField(
			TEXT("actor_guid_base36"), Section->GetActorGuid().ToString(EGuidFormats::Base36Encoded));
		FString ArtifactPath = FPackageName::LongPackageNameToFilename(
			Section->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
		FPaths::MakePathRelativeTo(ArtifactPath, *FPaths::ProjectDir());
		SectionJson->SetStringField(TEXT("artifact_path"), ArtifactPath.Replace(TEXT("\\"), TEXT("/")));
		SectionJson->SetStringField(TEXT("variant"), Variant.ToString());
		SectionJson->SetBoolField(TEXT("runtime_tag"), bTagged);
		SectionJson->SetBoolField(TEXT("spatially_loaded"), bSpatiallyLoaded);
		SectionJson->SetBoolField(TEXT("channel_texture"), bHasChannelTexture);
		SectionJson->SetNumberField(TEXT("channel_table_bytes"), ChannelTableBytes);
		SectionJson->SetNumberField(TEXT("collision_components"), CollisionComponents);
		SectionJson->SetNumberField(TEXT("static_meshes"), StaticMeshes.Num());
		SectionJson->SetNumberField(TEXT("nanite_meshes"), NaniteMeshes);
		SectionJson->SetStringField(TEXT("material"), SectionMaterial);
		SectionJson->SetStringField(TEXT("material_source"), SectionMaterialSource);
		SectionsJson.Add(MakeShared<FJsonValueObject>(SectionJson));
	}

	for (const FName Variant : {FName(TEXT("collision")), FName(TEXT("nanite")), FName(TEXT("fallback"))})
	{
		if (ExpectedSectionsPerVariant > 0 &&
			VariantCounts.FindRef(Variant) != ExpectedSectionsPerVariant)
		{
			Failures.Add(FString::Printf(TEXT("expected exactly %d %s sections, found %d"),
				ExpectedSectionsPerVariant, *Variant.ToString(), VariantCounts.FindRef(Variant)));
		}
	}
	if (PartitionCount != 1)
	{
		Failures.Add(FString::Printf(TEXT("expected exactly one shared-definition partition, found %d"),
			PartitionCount));
	}
	if (VariantCounts.Num() != 3)
	{
		Failures.Add(FString::Printf(TEXT("expected exactly three build variants, found %d"),
			VariantCounts.Num()));
	}
	const double HeightToleranceCentimeters =
		FMath::Max(Bundle.HeightQuantizationMeters, 1.0 / 128.0) * 100.0;
	const FHeightProbeResult NaniteHeight = CompareCanonicalHeights(
		Bundle, NaniteSurfaces, HeightToleranceCentimeters);
	const FHeightProbeResult FallbackHeight = CompareCanonicalHeights(
		Bundle, FallbackSurfaces, HeightToleranceCentimeters);
	if (NaniteHeight.MismatchedSamples > 0 ||
		NaniteHeight.MatchedSamples != NaniteHeight.ExpectedSamples)
	{
		Failures.Add(FString::Printf(
			TEXT("Nanite terrain differs from %d canonical samples: matched=%d mismatched=%d max_error_cm=%.6f"),
			NaniteHeight.ExpectedSamples, NaniteHeight.MatchedSamples,
			NaniteHeight.MismatchedSamples, NaniteHeight.MaximumErrorCentimeters));
	}
	if (FallbackHeight.MismatchedSamples > 0 ||
		FallbackHeight.MatchedSamples != FallbackHeight.ExpectedSamples)
	{
		Failures.Add(FString::Printf(
			TEXT("fallback terrain differs from %d canonical samples: matched=%d mismatched=%d max_error_cm=%.6f"),
			FallbackHeight.ExpectedSamples, FallbackHeight.MatchedSamples,
			FallbackHeight.MismatchedSamples, FallbackHeight.MaximumErrorCentimeters));
	}
	double FallbackSurfaceMaximumErrorCentimeters = 0.0;
	if (NaniteSurfaces.Num() != FallbackSurfaces.Num())
	{
		Failures.Add(TEXT("Nanite and explicit fallback surface counts differ"));
	}
	else
	{
		TSet<int32> MatchedFallbacks;
		for (const FSurfaceProbe& Nanite : NaniteSurfaces)
		{
			int32 Match = INDEX_NONE;
			double BestBoundsError = TNumericLimits<double>::Max();
			for (int32 Index = 0; Index < FallbackSurfaces.Num(); ++Index)
			{
				if (MatchedFallbacks.Contains(Index))
				{
					continue;
				}
				const FSurfaceProbe& Fallback = FallbackSurfaces[Index];
				const double BoundsError = FVector::Distance(Nanite.Bounds.Origin, Fallback.Bounds.Origin) +
					FVector::Distance(Nanite.Bounds.BoxExtent, Fallback.Bounds.BoxExtent);
				if (BoundsError < BestBoundsError)
				{
					BestBoundsError = BoundsError;
					Match = Index;
				}
			}
			if (Match == INDEX_NONE || BestBoundsError > FallbackSurfaceToleranceCentimeters)
			{
				Failures.Add(FString::Printf(TEXT("no matching explicit fallback surface for %s"), *Nanite.Actor));
				continue;
			}
			MatchedFallbacks.Add(Match);
			const FSurfaceProbe& Fallback = FallbackSurfaces[Match];
			FallbackSurfaceMaximumErrorCentimeters = FMath::Max(
				FallbackSurfaceMaximumErrorCentimeters,
				FMath::Max(
					DirectedSurfaceError(Nanite.Vertices, Fallback.Vertices),
					DirectedSurfaceError(Fallback.Vertices, Nanite.Vertices)));
		}
		if (FallbackSurfaceMaximumErrorCentimeters > FallbackSurfaceToleranceCentimeters)
		{
			Failures.Add(FString::Printf(TEXT("SM5 fallback surface error %.6f cm exceeds %.6f cm"),
				FallbackSurfaceMaximumErrorCentimeters, FallbackSurfaceToleranceCentimeters));
		}
	}

	TSharedPtr<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Receipt->SetStringField(TEXT("schema"), TEXT("project-world-mesh-terrain-audit:v1"));
	Receipt->SetStringField(TEXT("map"), *MapPackagePath);
	Receipt->SetStringField(TEXT("shared_definition"),
		ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
	Receipt->SetStringField(TEXT("compile_result_sha256"), Bundle.CompileResultHash);
	Receipt->SetStringField(TEXT("status"), Failures.IsEmpty() ? TEXT("accepted") : TEXT("rejected"));
	Receipt->SetNumberField(TEXT("partition_count"), PartitionCount);
	Receipt->SetNumberField(TEXT("expected_sections_per_variant"), ExpectedSectionsPerVariant);
	Receipt->SetNumberField(TEXT("fallback_surface_tolerance_cm"), FallbackSurfaceToleranceCentimeters);
	Receipt->SetNumberField(TEXT("fallback_surface_maximum_error_cm"), FallbackSurfaceMaximumErrorCentimeters);
	Receipt->SetNumberField(TEXT("canonical_height_tolerance_cm"), HeightToleranceCentimeters);
	Receipt->SetNumberField(TEXT("canonical_height_expected_samples"), NaniteHeight.ExpectedSamples);
	Receipt->SetNumberField(TEXT("nanite_height_matched_samples"), NaniteHeight.MatchedSamples);
	Receipt->SetNumberField(TEXT("nanite_height_mismatched_samples"), NaniteHeight.MismatchedSamples);
	Receipt->SetNumberField(TEXT("nanite_height_maximum_error_cm"), NaniteHeight.MaximumErrorCentimeters);
	Receipt->SetNumberField(TEXT("fallback_height_matched_samples"), FallbackHeight.MatchedSamples);
	Receipt->SetNumberField(TEXT("fallback_height_mismatched_samples"), FallbackHeight.MismatchedSamples);
	Receipt->SetNumberField(TEXT("fallback_height_maximum_error_cm"), FallbackHeight.MaximumErrorCentimeters);
	Receipt->SetArrayField(TEXT("sections"), SectionsJson);
	TArray<TSharedPtr<FJsonValue>> FailuresJson;
	for (const FString& Failure : Failures)
	{
		FailuresJson.Add(MakeShared<FJsonValueString>(Failure));
	}
	Receipt->SetArrayField(TEXT("failures"), FailuresJson);
	if (!WriteReceipt(ResultPath, Receipt))
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error, TEXT("Receipt write failed: %s"), *ResultPath);
		return 4;
	}

	if (Failures.IsEmpty())
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Display,
			TEXT("Audit accepted: partitions=%d sections=%d failures=0 result=%s"),
			PartitionCount, SectionsJson.Num(), *ResultPath);
	}
	else
	{
		UE_LOG(LogProjectWorldMeshTerrainAudit, Error,
			TEXT("Audit rejected: partitions=%d sections=%d failures=%d result=%s"),
			PartitionCount, SectionsJson.Num(), Failures.Num(), *ResultPath);
	}
	return Failures.IsEmpty() ? 0 : 1;
}
