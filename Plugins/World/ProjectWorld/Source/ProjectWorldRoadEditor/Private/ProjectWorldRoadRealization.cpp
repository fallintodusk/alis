// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRoadRealization.h"
#include "ProjectWorldRoadSurface.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "UObject/Linker.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldRoadRealization, Log, All);

namespace ProjectWorldRoadRealization
{
	namespace
	{
		const FString RoadCellTagPrefix(TEXT("ProjectWorld.RoadCell="));
		const FString RoadSemanticTagPrefix(TEXT("ProjectWorld.RoadSemantic="));
		const FName RoadTag(TEXT("ProjectWorld.Road.v1"));

		struct FRoadSettings
		{
			TSet<FString> SelectedClasses;
			double SurfaceOffsetMeters = 0.15;
			double MaximumSegmentLengthMeters = 7.5;
		};

		FString SanitizeToken(const FString& Value)
		{
			FString Result = Value;
			for (TCHAR& Character : Result)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
				{
					Character = TEXT('_');
				}
			}
			return Result.Left(96);
		}

		void SetTag(AActor* Actor, const FString& Prefix, const FString& Value)
		{
			Actor->Tags.RemoveAll([&Prefix](const FName& Tag)
			{
				return Tag.ToString().StartsWith(Prefix);
			});
			Actor->Tags.Add(FName(*(Prefix + Value)));
		}

		bool ReadTag(const AActor* Actor, const FString& Prefix, FString& OutValue)
		{
			for (const FName& Tag : Actor->Tags)
			{
				const FString Text = Tag.ToString();
				if (Text.StartsWith(Prefix))
				{
					OutValue = Text.RightChop(Prefix.Len());
					return !OutValue.IsEmpty();
				}
			}
			return false;
		}

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("%d:"), Value.Len());
			Target += Value;
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		bool ResolveSettings(
			const FProjectWorldRealizationLayer& Layer,
			FRoadSettings& OutSettings,
			FString& OutError)
		{
			TSharedPtr<FJsonObject> Settings;
			const TArray<TSharedPtr<FJsonValue>>* Classes = nullptr;
			FString CollisionPolicy;
			FString StructureFallback;
			FString IntersectionPolicy;
			bool bNanite = false;
			if (!FJsonSerializer::Deserialize(
				TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) ||
				!Settings.IsValid() ||
				!Settings->TryGetArrayField(TEXT("selected_classes"), Classes) || Classes == nullptr || Classes->IsEmpty() ||
				!Settings->TryGetNumberField(TEXT("surface_offset_m"), OutSettings.SurfaceOffsetMeters) ||
				!Settings->TryGetNumberField(TEXT("maximum_segment_length_m"), OutSettings.MaximumSegmentLengthMeters) ||
				!Settings->TryGetBoolField(TEXT("nanite"), bNanite) || !bNanite ||
				!Settings->TryGetStringField(TEXT("collision"), CollisionPolicy) || CollisionPolicy != TEXT("complex_as_simple") ||
				!Settings->TryGetStringField(TEXT("structure_fallback"), StructureFallback) || StructureFallback != TEXT("terrain_drape") ||
				!Settings->TryGetStringField(TEXT("intersection_policy"), IntersectionPolicy) || IntersectionPolicy != TEXT("overlap_same_owner") ||
				OutSettings.SurfaceOffsetMeters <= 0.0 || OutSettings.SurfaceOffsetMeters > 2.0 ||
				OutSettings.MaximumSegmentLengthMeters <= 0.0 || OutSettings.MaximumSegmentLengthMeters > 30.0)
			{
				OutError = TEXT("Road layer has no executable v1 settings contract.");
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Classes)
			{
				FString RoadClass;
				if (!Value.IsValid() || !Value->TryGetString(RoadClass) || RoadClass.IsEmpty() ||
					OutSettings.SelectedClasses.Contains(RoadClass))
				{
					OutError = TEXT("Road selected_classes contains an invalid or duplicate value.");
					return false;
				}
				OutSettings.SelectedClasses.Add(RoadClass);
			}
			return true;
		}

		TArray<FString> CellRoadFeatureIds(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FRoadSettings& Settings)
		{
			TSet<FString> CandidateIds;
			CandidateIds.Append(Cell.OwnedFeatureIds);
			CandidateIds.Append(Cell.ReferencedFeatureIds);
			TArray<FString> Result;
			for (const FString& FeatureId : CandidateIds)
			{
				const FProjectWorldCanonicalFeature* Feature = Bundle.Features.Find(FeatureId);
				if (Feature != nullptr && Feature->FeatureClass == TEXT("road") &&
					Settings.SelectedClasses.Contains(Feature->RoadClass) && Feature->WidthMeters > 0.0)
				{
					Result.Add(FeatureId);
				}
			}
			Result.Sort();
			return Result;
		}

		FVector2D RoadTangent(
			const FProjectWorldCanonicalFeature& Feature,
			const FVector2D& Point,
			double Tolerance, FString& Context)
		{
			FVector2D Sum = FVector2D::ZeroVector;
			AppendToken(Context, TEXT("tangent"));
			AppendNumber(Context, Point.X);
			AppendNumber(Context, Point.Y);
			FVector2D FirstDirection = FVector2D::ZeroVector;
			auto AccumulatePart = [&](const TArray<FVector2D>& Part)
			{
				for (int32 Index = 0; Index + 1 < Part.Num(); ++Index)
				{
					const FVector2D Segment = Part[Index + 1] - Part[Index];
					const double LengthSquared = Segment.SquaredLength();
					if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
					{
						continue;
					}
					const double Alpha = FMath::Clamp(
						FVector2D::DotProduct(Point - Part[Index], Segment) / LengthSquared,
						0.0,
						1.0);
					const double DistanceSquared = (Point - (Part[Index] + Segment * Alpha)).SquaredLength();
					const FVector2D Direction = Segment.GetSafeNormal();
					if (DistanceSquared <= Tolerance * Tolerance)
					{
						if (FirstDirection.IsNearlyZero()) FirstDirection = Direction;
						Sum += Direction;
						AppendNumber(Context, Part[Index].X);
						AppendNumber(Context, Part[Index].Y);
						AppendNumber(Context, Part[Index + 1].X);
						AppendNumber(Context, Part[Index + 1].Y);
					}
				}
			};
			if (Feature.GeometryParts.IsEmpty())
			{
				AccumulatePart(Feature.GeometryPoints);
			}
			else
			{
				for (const TArray<FVector2D>& Part : Feature.GeometryParts)
				{
					AccumulatePart(Part);
				}
			}
			return Sum.IsNearlyZero() ? FirstDirection : Sum.GetSafeNormal();
		}

		bool BuildCellMesh(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell,
			const FProjectWorldRealizationLayer& Layer,
			const FRoadSettings& Settings,
			FMeshDescription& OutDescription,
			FVector& OutOrigin,
			FString& OutInputHash,
			int32& OutTriangles, FString& OutError,
			const ProjectWorldRoadSurface::FContext& Context, bool bEmitMesh = true)
		{
			FString Identity(TEXT("project_road_cell_input_v2"));
			AppendToken(Identity, Cell.CellId);
			AppendToken(Identity, Layer.ContractHash);
			AppendNumber(Identity, Bundle.CoordinateQuantizationMeters);
			AppendNumber(Identity, Bundle.HeightOriginMeters);
			AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.X);
			AppendNumber(Identity, Bundle.EngineGeoreferenceOriginMeters.Y);
			TArray<FProjectWorldSurfaceTriangle> Ribbon;
			OutOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
			for (const FString& FeatureId : CellRoadFeatureIds(Bundle, Cell, Settings))
			{
				const FProjectWorldCanonicalFeature& Feature = Bundle.Features.FindChecked(FeatureId);
				AppendToken(Identity, Feature.FeatureId);
				AppendToken(Identity, Feature.RoadClass);
				AppendNumber(Identity, Feature.WidthMeters);
				const double HalfWidth = Feature.WidthMeters * 0.5;
				for (const FProjectWorldCanonicalRepresentation& Representation : Feature.Representations)
				{
					if (Representation.CellId != Cell.CellId || Representation.Kind != TEXT("road_fragment"))
					{
						continue;
					}
					for (const TArray<FVector2D>& Part : Representation.Parts)
					{
						AppendToken(Identity, FString::FromInt(Part.Num()));
						for (const FVector2D& Point : Part) { AppendNumber(Identity, Point.X); AppendNumber(Identity, Point.Y); }
						for (int32 PointIndex = 0; PointIndex + 1 < Part.Num(); ++PointIndex)
						{
							const FVector2D Start = Part[PointIndex];
							const FVector2D End = Part[PointIndex + 1];
							const int32 Steps = FMath::Max(
								1,
								FMath::CeilToInt((End - Start).Length() / Settings.MaximumSegmentLengthMeters));
							for (int32 Step = 0; Step < Steps; ++Step)
							{
								const FVector2D A = FMath::Lerp(Start, End, static_cast<double>(Step) / Steps);
								const FVector2D B = FMath::Lerp(Start, End, static_cast<double>(Step + 1) / Steps);
								const FVector2D TangentA = RoadTangent(Feature, A, Bundle.CoordinateQuantizationMeters * 2.0, Identity);
								const FVector2D TangentB = RoadTangent(Feature, B, Bundle.CoordinateQuantizationMeters * 2.0, Identity);
								if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
								{ OutError = TEXT("Road fragment has no bounded canonical tangent context."); return false; }
								const FVector2D SideA(-TangentA.Y * HalfWidth, TangentA.X * HalfWidth);
								const FVector2D SideB(-TangentB.Y * HalfWidth, TangentB.X * HalfWidth);
								const double HeightA = Settings.SurfaceOffsetMeters;
								const double HeightB = Settings.SurfaceOffsetMeters;
								const FVector LeftA(A + SideA, HeightA);
								const FVector RightA(A - SideA, HeightA);
								const FVector LeftB(B + SideB, HeightB);
								const FVector RightB(B - SideB, HeightB);
								Ribbon.Add(FProjectWorldSurfaceTriangle{{LeftA, RightA, LeftB}});
								Ribbon.Add(FProjectWorldSurfaceTriangle{{RightA, RightB, LeftB}});
								OutTriangles += 2;
							}
						}
					}
				}
			}
			if (OutTriangles == 0 && !CellRoadFeatureIds(Bundle, Cell, Settings).IsEmpty())
			{ OutError = TEXT("Selected road has no realizable cell fragment."); return false; }
			TSet<FString> ConsumedTerrain;
			if (!ProjectWorldRoadSurface::Conform(Bundle, Cell, Context, Ribbon, OutOrigin, Settings.SurfaceOffsetMeters,
				Layer.DependencyHaloCells, bEmitMesh ? &OutDescription : nullptr, ConsumedTerrain, OutError)) return false;
			TArray<FString> TerrainIds = ConsumedTerrain.Array();
			TerrainIds.Sort();
			for (const FString& Id : TerrainIds)
			{
				AppendToken(Identity, Id);
				for (int32 Y = Cell.CellY - Layer.DependencyHaloCells; Y <= Cell.CellY + Layer.DependencyHaloCells; ++Y)
				for (int32 X = Cell.CellX - Layer.DependencyHaloCells; X <= Cell.CellX + Layer.DependencyHaloCells; ++X)
				{
					const auto* Found = Context.Cells.Find(FIntPoint(X, Y));
					if (Found && (*Found)->CellId == Id) AppendToken(Identity, (*Found)->Terrain.ArtifactHash);
				}
			}
			OutTriangles = OutDescription.Triangles().Num();
			return HashText(Identity, OutInputHash);
		}

		AStaticMeshActor* FindCellActor(UWorld* World, const FString& CellId, FString& OutError)
		{
			AStaticMeshActor* Result = nullptr;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				FString CandidateCell;
				FString Semantic;
				if (!ReadActorIdentity(*It, CandidateCell, Semantic) || CandidateCell != CellId)
				{
					continue;
				}
				if (Result != nullptr)
				{
					OutError = FString::Printf(TEXT("Road cell actor identity is duplicated: %s"), *CellId);
					return nullptr;
				}
				Result = *It;
			}
			if (Result == nullptr)
			{
				const FString ActorName = SanitizeToken(TEXT("ProjectWorld_Road_") + CellId);
				AStaticMeshActor* NamedActor = FindObject<AStaticMeshActor>(World->PersistentLevel, *ActorName);
				FString NamedCell;
				FString NamedSemantic;
				if (ReadActorIdentity(NamedActor, NamedCell, NamedSemantic) && NamedCell == CellId)
				{
					Result = NamedActor;
				}
			}
			return Result;
		}

		bool SaveAsset(UPackage* Package, UObject* Asset)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.TopLevelFlags = RF_Public | RF_Standalone;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, Asset, *Filename, Arguments);
		}

		bool SaveExternalActor(AActor* Actor)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Package == nullptr)
			{
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, nullptr, *Filename, Arguments);
		}

		bool RemoveCellOutput(UWorld* World, AStaticMeshActor* Actor, const FString& PackageName)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Actor != nullptr && !World->EditorDestroyActor(Actor, true))
			{
				return false;
			}
			if (Package != nullptr)
			{
				ResetLoaders(Package);
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			return !IFileManager::Get().FileExists(*Filename) || IFileManager::Get().Delete(*Filename, false, true);
		}
	}

	bool ValidateSettings(const FString& NormalizedSettings, FString& OutError)
	{
		static const TCHAR* Fields[] = {
			TEXT("selected_classes"), TEXT("surface_offset_m"),
			TEXT("maximum_segment_length_m"), TEXT("nanite"), TEXT("collision"),
			TEXT("structure_fallback"), TEXT("intersection_policy")};
		TSharedPtr<FJsonObject> Settings;
		if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(
			NormalizedSettings, MakeArrayView(Fields), Settings, OutError))
		{
			return false;
		}
		FProjectWorldRealizationLayer Layer;
		Layer.NormalizedSettings = NormalizedSettings;
		FRoadSettings TypedSettings;
		return ResolveSettings(Layer, TypedSettings, OutError);
	}

	bool HashActorSemantic(const FString& CellId, const FString& MeshSemantic,
		FString& OutHash)
	{
		return HashText(TEXT("project_road_actor_v1|") + CellId + TEXT("|") + MeshSemantic, OutHash);
	}

	bool HashCellInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		FString& OutHash,
		FString& OutError)
	{
		FRoadSettings Settings;
		if (!ResolveSettings(Layer, Settings, OutError))
		{
			return false;
		}
		FMeshDescription Description;
		FVector Origin;
		int32 Triangles = 0;
		ProjectWorldRoadSurface::FContext Context;
		return ProjectWorldRoadSurface::Index(Bundle, Context, OutError) &&
			BuildCellMesh(Bundle, Cell, Layer, Settings, Description, Origin, OutHash, Triangles, OutError, Context, false);
	}

	bool HashUnitInputs(const FProjectWorldCanonicalBundle& Bundle, const FProjectWorldRealizationLayer& Layer,
		TMap<FString, FString>& OutUnits, FString& OutError)
	{
		ProjectWorldRoadSurface::FContext Context;
		FRoadSettings Settings;
		if (!ResolveSettings(Layer, Settings, OutError) || !ProjectWorldRoadSurface::Index(Bundle, Context, OutError)) return false;
		TMap<FString, FString> Candidate;
		for (const auto& Cell : Bundle.Cells)
		{
			FMeshDescription Unused;
			FVector Origin;
			FString Hash;
			int32 Triangles = 0;
			if (!BuildCellMesh(Bundle, Cell, Layer, Settings, Unused, Origin, Hash, Triangles, OutError, Context, false)) return false;
			Candidate.Add(Cell.CellId, Hash);
		}
		OutUnits = MoveTemp(Candidate);
		return true;
	}

	bool BuildDescription(const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell, const FProjectWorldRealizationLayer& Layer,
		FMeshDescription& OutDescription, FVector& OutOrigin, FString& OutError)
	{
		FRoadSettings Settings;
		FString Semantic;
		int32 Triangles = 0;
		ProjectWorldRoadSurface::FContext Context;
		return ResolveSettings(Layer, Settings, OutError) && ProjectWorldRoadSurface::Index(Bundle, Context, OutError) &&
			BuildCellMesh(Bundle, Cell, Layer, Settings, OutDescription, OutOrigin, Semantic, Triangles, OutError, Context);
	}

	bool ExpectsCellOutput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldCanonicalCell& Cell,
		const FProjectWorldRealizationLayer& Layer,
		bool& bOutExpected,
		FString& OutError)
	{
		FRoadSettings Settings;
		if (!ResolveSettings(Layer, Settings, OutError))
		{
			return false;
		}
		bOutExpected = !CellRoadFeatureIds(Bundle, Cell, Settings).IsEmpty();
		return true;
	}

	bool ReadActorIdentity(const AActor* Actor, FString& OutCellId, FString& OutSemanticHash)
	{
		return Actor != nullptr && Actor->Tags.Contains(RoadTag) &&
			ReadTag(Actor, RoadCellTagPrefix, OutCellId) &&
			ReadTag(Actor, RoadSemanticTagPrefix, OutSemanticHash);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		UMaterialInterface* RoadMaterial,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([](const auto& Candidate)
		{
			return Candidate.GeneratorId == TEXT("project_road_mesh") && Candidate.GeneratorVersion == 1;
		});
		if (Layer == nullptr)
		{
			return true;
		}
		FRoadSettings Settings;
		if (RoadMaterial == nullptr || !ResolveSettings(*Layer, Settings, OutError))
		{
			OutError = RoadMaterial == nullptr ? TEXT("Road material is unavailable.") : OutError;
			return false;
		}
		FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
			[Layer](const auto& Candidate) { return Candidate.LayerId == Layer->LayerId; });
		if (Inventory == nullptr)
		{
			OutError = TEXT("Road realization has no authenticated dirty inventory.");
			return false;
		}
		ProjectWorldRoadSurface::FContext Context;
		if (!ProjectWorldRoadSurface::Index(Bundle, Context, OutError)) return false;
		const bool bWholeDirty = Inventory->FinalDirtyUnits.Contains(TEXT("*"));
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			const FString AssetName = TEXT("SM_ProjectWorldRoad_") + SanitizeToken(Cell.CellId);
			const FString PackageName = Layer->ArtifactRoot + TEXT("Cells/") + AssetName;
			AStaticMeshActor* Actor = FindCellActor(World, Cell.CellId, OutError);
			if (!OutError.IsEmpty())
			{
				return false;
			}
			const bool bAssetExists = IFileManager::Get().FileExists(*FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension()));
			const bool bExpected = !CellRoadFeatureIds(Bundle, Cell, Settings).IsEmpty();
			const UStaticMesh* ExistingMesh = Actor != nullptr && Actor->GetStaticMeshComponent() != nullptr
				? Actor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			const bool bPresentationMatches = ExistingMesh != nullptr &&
				ExistingMesh->GetStaticMaterials().Num() > 0 &&
				ExistingMesh->GetStaticMaterials()[0].MaterialInterface == RoadMaterial;
			if (!bWholeDirty && !Inventory->FinalDirtyUnits.Contains(Cell.CellId) &&
				!(bExpected && (Actor == nullptr || !bAssetExists || !bPresentationMatches)))
			{
				continue;
			}

			FMeshDescription Description;
			FVector ActorOrigin = FVector::ZeroVector;
			FString Semantic;
			int32 TriangleCount = 0;
			FString InputHash;
			if (!BuildCellMesh(Bundle, Cell, *Layer, Settings, Description, ActorOrigin, InputHash, TriangleCount, OutError, Context) ||
				!HashText(TEXT("project_road_cell_mesh_v2|") + Cell.CellId + TEXT("|") + InputHash, Semantic))
			{
				OutError = FString::Printf(TEXT("Cannot build road mesh for cell %s: %s"), *Cell.CellId, *OutError);
				return false;
			}
			FString ExistingCell;
			FString ExistingSemantic;
			if (TriangleCount > 0 && Actor != nullptr && bAssetExists &&
				ReadActorIdentity(Actor, ExistingCell, ExistingSemantic) && ExistingSemantic == Semantic &&
				bPresentationMatches)
			{
				continue;
			}
			if (TriangleCount == 0)
			{
				const bool bRemovedActor = Actor != nullptr;
				if (!RemoveCellOutput(World, Actor, PackageName))
				{
					OutError = FString::Printf(TEXT("Cannot retire road output for cell: %s"), *Cell.CellId);
					return false;
				}
				if (bRemovedActor)
				{
					++OutResult.RemovedActorCount;
					++OutResult.SelfSavedActorMutationCount;
				}
				continue;
			}

			UPackage* Package = CreatePackage(*PackageName);
			UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, *AssetName);
			if (Mesh == nullptr)
			{
				Mesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
			}
			Mesh->Modify();
			Mesh->GetStaticMaterials().Reset();
			Mesh->GetStaticMaterials().Add(FStaticMaterial(RoadMaterial, TEXT("Road"), TEXT("Road")));
			FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
			Nanite.bEnabled = true;
			Mesh->SetNaniteSettings(Nanite);
			Mesh->bGenerateMeshDistanceField = false;
			Mesh->bHasNavigationData = false;
			Mesh->SetNumSourceModels(1);
			Mesh->GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale = 0.0f;
			UStaticMesh::FBuildMeshDescriptionsParams BuildParameters;
			BuildParameters.bUseHashAsGuid = true;
			if (!Mesh->BuildFromMeshDescriptions({&Description}, BuildParameters))
			{
				OutError = FString::Printf(TEXT("Cannot build persistent road mesh for cell: %s"), *Cell.CellId);
				return false;
			}
			Mesh->CreateBodySetup();
			Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
			FAssetRegistryModule::AssetCreated(Mesh);
			if (!SaveAsset(Package, Mesh))
			{
				OutError = FString::Printf(TEXT("Cannot save persistent road mesh for cell: %s"), *Cell.CellId);
				return false;
			}

			const bool bUpdating = Actor != nullptr;
			if (Actor == nullptr)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*SanitizeToken(TEXT("ProjectWorld_Road_") + Cell.CellId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|road|") + Cell.CellId);
				Actor = World->SpawnActor<AStaticMeshActor>(
					AStaticMeshActor::StaticClass(), ActorOrigin, FRotator::ZeroRotator, SpawnParameters);
				if (Actor != nullptr)
				{
					Actor->SetPackageExternal(true);
				}
			}
			if (Actor == nullptr || Actor->GetStaticMeshComponent() == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create road cell actor: %s"), *Cell.CellId);
				return false;
			}
			Actor->Modify();
			Actor->Tags.Reset();
			Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.Add(RoadTag);
			SetTag(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			SetTag(Actor, RoadCellTagPrefix, Cell.CellId);
			SetTag(Actor, RoadSemanticTagPrefix, Semantic);
			Actor->SetActorLabel(TEXT("ProjectWorld Roads ") + Cell.CellId);
			Actor->SetActorLocation(ActorOrigin, false, nullptr, ETeleportType::TeleportPhysics);
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->SetHLODLayer(nullptr);
			UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
			Component->Modify();
			Component->SetStaticMesh(Mesh);
			Component->SetMobility(EComponentMobility::Static);
			Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Component->SetCanEverAffectNavigation(false);
			Actor->MarkPackageDirty();
			const bool bMapPackageIsTemporary =
				World->PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
			if (!bMapPackageIsTemporary && !SaveExternalActor(Actor))
			{
				OutError = FString::Printf(TEXT("Cannot save road cell actor: %s"), *Cell.CellId);
				return false;
			}
			OutResult.MutableProducerMetric(TEXT("project_road_mesh"), TEXT("triangles_written")) += TriangleCount;
			++OutResult.SelfSavedActorMutationCount;
			if (bUpdating) ++OutResult.UpdatedActorCount;
			else ++OutResult.CreatedActorCount;
		}
		FAssetCompilingManager::Get().FinishAllCompilation();
		return true;
	}
}
