// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVegetationRealization.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

namespace ProjectWorldVegetationRealization
{
	namespace
	{
		const FName VegetationTag(TEXT("ProjectWorld.Vegetation=v1"));
		const FString CellTagPrefix(TEXT("ProjectWorld.VegetationCell="));
		const FString SemanticTagPrefix(TEXT("ProjectWorld.VegetationSemantic="));

		bool ReadTag(const AActor* Actor, const FString& Prefix, FString& OutValue)
		{
			if (Actor == nullptr)
			{
				return false;
			}
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

		void SetTag(AActor* Actor, const FString& Prefix, const FString& Value)
		{
			Actor->Tags.RemoveAll([&Prefix](const FName& Tag) { return Tag.ToString().StartsWith(Prefix); });
			Actor->Tags.Add(FName(*(Prefix + Value)));
		}

		FString SanitizeToken(FString Value)
		{
			for (TCHAR& Character : Value)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
				{
					Character = TEXT('_');
				}
			}
			return Value;
		}

		bool ResolveMeshes(const FProjectWorldRealizationLayer& Layer, TArray<UStaticMesh*>& OutMeshes, FString& OutError)
		{
			TSharedPtr<FJsonObject> Settings;
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) ||
				!Settings.IsValid() || !Settings->TryGetArrayField(TEXT("mesh_assets"), Values) || Values == nullptr)
			{
				OutError = TEXT("Vegetation mesh settings are unavailable.");
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Path;
				UStaticMesh* Mesh = nullptr;
				if (!Value.IsValid() || !Value->TryGetString(Path) ||
					(Mesh = LoadObject<UStaticMesh>(nullptr, *Path)) == nullptr || !Mesh->GetNaniteSettings().bEnabled)
				{
					OutError = FString::Printf(TEXT("Vegetation mesh is missing or not Nanite-enabled: %s"), *Path);
					return false;
				}
				OutMeshes.Add(Mesh);
			}
			return !OutMeshes.IsEmpty();
		}

		AActor* FindCellActor(UWorld* World, const FString& CellId, FString& OutError)
		{
			AActor* Result = nullptr;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				FString CandidateCell;
				FString Semantic;
				if (!ReadActorIdentity(*It, CandidateCell, Semantic) || CandidateCell != CellId)
				{
					continue;
				}
				if (Result != nullptr)
				{
					OutError = FString::Printf(TEXT("Vegetation cell actor identity is duplicated: %s"), *CellId);
					return nullptr;
				}
				Result = *It;
			}
			return Result;
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

		bool RemoveExternalActor(UWorld* World, AActor* Actor)
		{
			const UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			const FString Filename = Package != nullptr
				? FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension())
				: FString();
			if (Actor != nullptr && !World->EditorDestroyActor(Actor, true))
			{
				return false;
			}
			return Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename) ||
				IFileManager::Get().Delete(*Filename, false, true);
		}

		bool ReplaceComponents(
			AActor* Actor,
			const FVector& ActorOrigin,
			const TArray<UStaticMesh*>& Meshes,
			const TArray<FProjectWorldVegetationInstance>& Instances,
			FString& OutError)
		{
			TArray<UHierarchicalInstancedStaticMeshComponent*> Existing;
			Actor->GetComponents(Existing);
			Actor->SetRootComponent(nullptr);
			for (UHierarchicalInstancedStaticMeshComponent* Component : Existing)
			{
				Component->DestroyComponent();
			}
			for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
			{
				int32 InstanceCount = 0;
				for (const FProjectWorldVegetationInstance& Instance : Instances)
				{
					InstanceCount += Instance.MeshIndex == MeshIndex ? 1 : 0;
				}
				if (InstanceCount == 0)
				{
					continue;
				}
				const FName Name(*FString::Printf(TEXT("Vegetation_%02d"), MeshIndex));
				UHierarchicalInstancedStaticMeshComponent* Component =
					NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, Name, RF_Transactional);
				if (Component == nullptr)
				{
					OutError = TEXT("Cannot create a vegetation HISM component.");
					return false;
				}
				Component->SetStaticMesh(Meshes[MeshIndex]);
				Component->SetMobility(EComponentMobility::Static);
				Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Component->SetCanEverAffectNavigation(false);
				Component->SetGenerateOverlapEvents(false);
				Actor->AddInstanceComponent(Component);
				if (Actor->GetRootComponent() == nullptr)
				{
					Actor->SetRootComponent(Component);
					// AActor has no independent transform. Seed the new root before
					// registration so the cell origin is serialized by OFPA.
					Component->SetRelativeLocation(ActorOrigin);
				}
				else
				{
					Component->SetupAttachment(Actor->GetRootComponent());
				}
				Component->RegisterComponent();
				for (const FProjectWorldVegetationInstance& Instance : Instances)
				{
					if (Instance.MeshIndex == MeshIndex)
					{
						Component->AddInstance(Instance.Transform, false);
					}
				}
			}
			return Actor->GetRootComponent() != nullptr;
		}
	}

	bool ReadActorIdentity(const AActor* Actor, FString& OutCellId, FString& OutSemanticHash)
	{
		return Actor != nullptr && Actor->Tags.Contains(VegetationTag) &&
			ReadTag(Actor, CellTagPrefix, OutCellId) && ReadTag(Actor, SemanticTagPrefix, OutSemanticHash);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldAuthoredOverlaySet& AuthoredOverlaySet,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([](const auto& Candidate)
		{
			return Candidate.GeneratorId == TEXT("project_vegetation_instances") && Candidate.GeneratorVersion == 1;
		});
		if (Layer == nullptr)
		{
			return true;
		}
		FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
			[Layer](const auto& Candidate) { return Candidate.LayerId == Layer->LayerId; });
		TArray<UStaticMesh*> Meshes;
		if (Inventory == nullptr || !ResolveMeshes(*Layer, Meshes, OutError))
		{
			OutError = Inventory == nullptr ? TEXT("Vegetation realization has no authenticated dirty inventory.") : OutError;
			return false;
		}
		const bool bWholeDirty = Inventory->FinalDirtyUnits.Contains(TEXT("*"));
		for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
		{
			AActor* Actor = FindCellActor(World, Cell.CellId, OutError);
			if (!OutError.IsEmpty())
			{
				return false;
			}
			TArray<FProjectWorldVegetationInstance> Instances;
			FString Semantic;
			if (!BuildCellInstances(
				Bundle, Cell, *Layer, Profile, AuthoredOverlaySet, Instances, nullptr, Semantic, OutError))
			{
				return false;
			}
			if (!bWholeDirty && !Inventory->FinalDirtyUnits.Contains(Cell.CellId) && !(Actor == nullptr && !Instances.IsEmpty()))
			{
				continue;
			}
			FString ExistingCell;
			FString ExistingSemantic;
			if (!bWholeDirty && !Instances.IsEmpty() &&
				ReadActorIdentity(Actor, ExistingCell, ExistingSemantic) && ExistingSemantic == Semantic)
			{
				continue;
			}
			if (Instances.IsEmpty())
			{
				if (Actor != nullptr)
				{
					if (!RemoveExternalActor(World, Actor))
					{
						OutError = FString::Printf(TEXT("Cannot retire vegetation output for cell: %s"), *Cell.CellId);
						return false;
					}
					++OutResult.RemovedActorCount;
					++OutResult.SelfSavedActorMutationCount;
				}
				continue;
			}
			const FVector ActorOrigin = FProjectWorldCanonicalLoader::CanonicalToUnreal(
				Bundle, FVector(Cell.Bounds.X, Cell.Bounds.W, Bundle.HeightOriginMeters));
			const bool bUpdating = Actor != nullptr;
			if (Actor == nullptr)
			{
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = FName(*SanitizeToken(TEXT("ProjectWorld_Vegetation_") + Cell.CellId));
				SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
				SpawnParameters.OverrideActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|vegetation|") + Cell.CellId);
				Actor = World->SpawnActor<AActor>(AActor::StaticClass(), ActorOrigin, FRotator::ZeroRotator, SpawnParameters);
				if (Actor != nullptr)
				{
					Actor->SetPackageExternal(true);
				}
			}
			if (Actor == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot create vegetation cell actor: %s"), *Cell.CellId);
				return false;
			}
			Actor->Modify();
			if (!ReplaceComponents(Actor, ActorOrigin, Meshes, Instances, OutError))
			{
				return false;
			}
			Actor->Tags.Reset();
			Actor->Tags.Add(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.Add(VegetationTag);
			SetTag(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			SetTag(Actor, CellTagPrefix, Cell.CellId);
			SetTag(Actor, SemanticTagPrefix, Semantic);
			Actor->SetActorLabel(TEXT("ProjectWorld Vegetation ") + Cell.CellId);
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->SetHLODLayer(nullptr);
			Actor->MarkPackageDirty();
			const bool bTemporary = World->PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
			if (!bTemporary && !SaveExternalActor(Actor))
			{
				OutError = FString::Printf(TEXT("Cannot save vegetation cell actor: %s"), *Cell.CellId);
				return false;
			}
			OutResult.VegetationInstanceRewriteCount += Instances.Num();
			++OutResult.SelfSavedActorMutationCount;
			if (bUpdating) ++OutResult.UpdatedActorCount;
			else ++OutResult.CreatedActorCount;
		}
		return true;
	}
}
