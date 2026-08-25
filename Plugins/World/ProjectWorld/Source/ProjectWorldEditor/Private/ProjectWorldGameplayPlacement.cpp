// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldGameplayPlacement.h"

#include "ProjectServiceLocator.h"
#include "ProjectWorldActor.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldDataRoots.h"
#include "ProjectWorldGeneratedGeometry.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"
#include "ProjectWorldSchemaReference.h"
#include "Interfaces/IPickupSource.h"
#include "Services/IObjectSpawnService.h"
#include "Utilities/ProjectSha256.h"

#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldGameplayPlacement, Log, All);

namespace ProjectWorldGameplayPlacement
{
	namespace
	{
		const FName PlacementTag(TEXT("ProjectWorld.GameplayPlacement.v1"));
		const FString ObjectTagPrefix(TEXT("ProjectWorld.GameplayObject="));
		const FString SemanticTagPrefix(TEXT("ProjectWorld.GameplaySemantic="));
		const TCHAR* ExpectedSchemaFilename = TEXT("project_world_gameplay_placement.schema.json");

		bool IsIdentifier(const FString& Value)
		{
			if (Value.IsEmpty()) return false;
			for (const TCHAR Character : Value)
			{
				const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
				const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
				if (!bLowerAscii && !bDigitAscii && Character != TEXT('_')) return false;
			}
			return true;
		}

		bool HasOnlyFields(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> Allowed)
		{
			TSet<FString> Fields;
			for (const TCHAR* Field : Allowed) Fields.Add(Field);
			for (const auto& Pair : Object->Values)
			{
				if (!Fields.Contains(FString(Pair.Key.ToView()))) return false;
			}
			return Object->Values.Num() == Fields.Num();
		}

		void AppendToken(FString& Target, const FString& Value)
		{
			Target += FString::Printf(TEXT("%d:"), Value.Len()) + Value;
		}

		void AppendNumber(FString& Target, double Value)
		{
			AppendToken(Target, FString::Printf(TEXT("%.17g"), Value));
		}

		bool HashText(const FString& Text, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Text);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		FString SanitizeName(const FString& Value)
		{
			FString Result = Value;
			for (TCHAR& Character : Result)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_')) Character = TEXT('_');
			}
			return Result.Left(96);
		}

		bool ReadTag(const AActor* Actor, const FString& Prefix, FString& OutValue)
		{
			if (Actor == nullptr) return false;
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

		bool ResolveSourcePath(
			const FProjectWorldRealizationProfile& Profile,
			const FProjectWorldRealizationLayer& Layer,
			FString& OutPath,
			FString& OutError)
		{
			TSharedPtr<FJsonObject> Settings;
			FString RelativePath;
			FProjectWorldDataRoots Roots;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Layer.NormalizedSettings), Settings) ||
				!Settings.IsValid() || !Settings->TryGetStringField(TEXT("placement_source"), RelativePath) ||
				RelativePath.IsEmpty() || !FPaths::IsRelative(RelativePath) || RelativePath.Contains(TEXT("..")) ||
				RelativePath.Contains(TEXT("\\")) || !RelativePath.StartsWith(TEXT("GameplayPlacement/")) ||
				!FProjectWorldDataRoots::Resolve(Profile.WorldDataPluginName, Roots, OutError))
			{
				OutError = OutError.IsEmpty() ? TEXT("Gameplay placement source setting is invalid.") : OutError;
				return false;
			}
			OutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Roots.DataRoot, RelativePath));
			if (!FPaths::IsUnderDirectory(OutPath, Roots.DataRoot) || !FPaths::FileExists(OutPath))
			{
				OutError = FString::Printf(TEXT("Gameplay placement source is missing or outside its owner: %s"), *RelativePath);
				return false;
			}
			return true;
		}

		const FProjectWorldCanonicalCell* FindCell(
			const FProjectWorldCanonicalBundle& Bundle,
			int32 CellX,
			int32 CellY)
		{
			return Bundle.Cells.FindByPredicate([CellX, CellY](const FProjectWorldCanonicalCell& Cell)
			{
				return Cell.CellX == CellX && Cell.CellY == CellY;
			});
		}

		bool SaveExternalActor(AActor* Actor)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Package == nullptr) return false;
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.SaveFlags = SAVE_NoError;
			return UPackage::SavePackage(Package, nullptr, *Filename, Arguments);
		}

		bool HasPickupCapability(const AActor* Actor)
		{
			if (Actor->Implements<UPickupSource>()) return true;
			TArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			return Components.ContainsByPredicate([](const UActorComponent* Component)
			{
				return Component != nullptr && Component->Implements<UPickupSource>();
			});
		}

		bool HasCollisionPrimitive(const AActor* Actor)
		{
			TArray<UPrimitiveComponent*> Components;
			Actor->GetComponents(Components);
			return Components.ContainsByPredicate([](const UPrimitiveComponent* Component)
			{
				return Component != nullptr && Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
			});
		}

		bool ValidateActor(
			const AActor* Actor,
			const FProjectWorldGameplayPlacement& Placement,
			const FGuid& ExpectedDataId,
			FString& OutError)
		{
			const AProjectWorldActor* WorldActor = Cast<AProjectWorldActor>(Actor);
			if (WorldActor == nullptr || WorldActor->DataId != ExpectedDataId ||
				WorldActor->ObjectDefinitionId != Placement.DefinitionId || !Actor->GetIsSpatiallyLoaded() ||
				!Actor->IsPackageExternal() || Actor->bEnableAutoLODGeneration || Actor->GetHLODLayer() != nullptr ||
				!HasCollisionPrimitive(Actor) || !HasPickupCapability(Actor))
			{
				OutError = FString::Printf(
					TEXT("Gameplay placement actor violates identity, spatial, collision, or capability policy: %s"),
					*Placement.ObjectId);
				return false;
			}
			return true;
		}

		bool AddArtifact(
			const AActor* Actor,
			const FString& Semantic,
			FProjectWorldLayerInventory& Inventory,
			FString& OutError)
		{
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			if (Package == nullptr)
			{
				OutError = TEXT("Gameplay placement actor has no external package.");
				return false;
			}
			FString Filename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension()));
			FString Relative = Filename;
			const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FString Digest;
			if (!FPaths::FileExists(Filename) || !FPaths::MakePathRelativeTo(Relative, *ProjectRoot) ||
				Relative.StartsWith(TEXT("..")) || !FProjectSha256::HashFile(Filename, Digest))
			{
				OutError = FString::Printf(TEXT("Gameplay placement package cannot be authenticated: %s"), *Filename);
				return false;
			}
			Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
			Inventory.Artifacts.Add({Relative, TEXT("external_actor"), Digest, Semantic});
			return true;
		}
	}

	bool Load(
		const FProjectWorldRealizationProfile& Profile,
		const FProjectWorldRealizationLayer& Layer,
		FProjectWorldGameplayPlacementSet& OutSet,
		FString& OutError)
	{
		OutSet = FProjectWorldGameplayPlacementSet();
		OutError.Reset();
		if (!ResolveSourcePath(Profile, Layer, OutSet.SourcePath, OutError)) return false;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *OutSet.SourcePath) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() ||
			!HasOnlyFields(Root, {TEXT("$schema"), TEXT("schema_version"), TEXT("placement_set_id"),
				TEXT("world_data_plugin"), TEXT("canonical_profile_id"), TEXT("placements")}))
		{
			OutError = TEXT("Gameplay placement document is unreadable or has unknown fields.");
			return false;
		}
		FString Schema;
		double SchemaVersion = 0.0;
		const TArray<TSharedPtr<FJsonValue>>* Placements = nullptr;
		if (!Root->TryGetStringField(TEXT("$schema"), Schema) ||
			!ProjectWorldSchemaReference::ResolvesToCanonical(
				OutSet.SourcePath, Schema, ExpectedSchemaFilename, OutError) ||
			!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1.0 ||
			!Root->TryGetStringField(TEXT("placement_set_id"), OutSet.PlacementSetId) || !IsIdentifier(OutSet.PlacementSetId) ||
			!Root->TryGetStringField(TEXT("world_data_plugin"), OutSet.WorldDataPluginName) ||
			!Root->TryGetStringField(TEXT("canonical_profile_id"), OutSet.CanonicalProfileId) ||
			OutSet.WorldDataPluginName != Profile.WorldDataPluginName ||
			OutSet.CanonicalProfileId != Profile.CanonicalProfileId ||
			!Root->TryGetArrayField(TEXT("placements"), Placements) || Placements == nullptr || Placements->IsEmpty())
		{
			OutError = OutError.IsEmpty() ? TEXT("Gameplay placement document identity is invalid.") : OutError;
			return false;
		}
		TSet<FString> ObjectIds;
		for (const TSharedPtr<FJsonValue>& Value : *Placements)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FProjectWorldGameplayPlacement Placement;
			FString DefinitionId;
			double CellX = 0.0;
			double CellY = 0.0;
			if (!Object.IsValid() || !HasOnlyFields(Object, {TEXT("object_id"), TEXT("definition_id"),
					TEXT("cell_x"), TEXT("cell_y"), TEXT("easting_m"), TEXT("northing_m"),
					TEXT("surface_offset_m"), TEXT("yaw_degrees")}) ||
				!Object->TryGetStringField(TEXT("object_id"), Placement.ObjectId) || !IsIdentifier(Placement.ObjectId) ||
				ObjectIds.Contains(Placement.ObjectId) || !Object->TryGetStringField(TEXT("definition_id"), DefinitionId) ||
				!DefinitionId.StartsWith(TEXT("ObjectDefinition:")) ||
				!Object->TryGetNumberField(TEXT("cell_x"), CellX) || CellX != FMath::FloorToDouble(CellX) ||
				!Object->TryGetNumberField(TEXT("cell_y"), CellY) || CellY != FMath::FloorToDouble(CellY) ||
				!Object->TryGetNumberField(TEXT("easting_m"), Placement.EastingMeters) ||
				!Object->TryGetNumberField(TEXT("northing_m"), Placement.NorthingMeters) ||
				!Object->TryGetNumberField(TEXT("surface_offset_m"), Placement.SurfaceOffsetMeters) ||
				!Object->TryGetNumberField(TEXT("yaw_degrees"), Placement.YawDegrees) ||
				!FMath::IsFinite(Placement.EastingMeters) || !FMath::IsFinite(Placement.NorthingMeters) ||
				!FMath::IsFinite(Placement.SurfaceOffsetMeters) || Placement.SurfaceOffsetMeters < -10.0 ||
				Placement.SurfaceOffsetMeters > 100.0 || !FMath::IsFinite(Placement.YawDegrees) ||
				Placement.YawDegrees < -360.0 || Placement.YawDegrees > 360.0)
			{
				OutError = TEXT("Gameplay placement record is invalid, duplicated, or contains mutable/unknown fields.");
				return false;
			}
			Placement.CellX = static_cast<int32>(CellX);
			Placement.CellY = static_cast<int32>(CellY);
			Placement.DefinitionId = FPrimaryAssetId::FromString(DefinitionId);
			if (!Placement.DefinitionId.IsValid())
			{
				OutError = FString::Printf(TEXT("Gameplay placement definition ID is invalid: %s"), *DefinitionId);
				return false;
			}
			ObjectIds.Add(Placement.ObjectId);
			OutSet.Placements.Add(MoveTemp(Placement));
		}
		OutSet.Placements.Sort([](const auto& Left, const auto& Right) { return Left.ObjectId < Right.ObjectId; });
		return true;
	}

	bool Capture(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldLayerInventory& Inventory,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([&Inventory](const auto& Candidate)
		{
			return Candidate.LayerId == Inventory.LayerId &&
				Candidate.GeneratorId == TEXT("project_gameplay_placement") && Candidate.GeneratorVersion == 1;
		});
		FProjectWorldGameplayPlacementSet Set;
		if (Layer == nullptr || !Load(Profile, *Layer, Set, OutError)) return false;
		TMap<FString, const FProjectWorldGameplayPlacement*> Placements;
		for (const FProjectWorldGameplayPlacement& Placement : Set.Placements)
		{
			Placements.Add(Placement.ObjectId, &Placement);
		}
		TSet<FString> ActualIds;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(PlacementTag)) continue;
			FString ObjectId;
			FString CellId;
			FString Semantic;
			const FProjectWorldGameplayPlacement* const* Placement = nullptr;
			FTransform ExpectedTransform;
			FString ExpectedCell;
			FString ExpectedSemantic;
			if (!ReadTag(*It, ObjectTagPrefix, ObjectId) || ActualIds.Contains(ObjectId) ||
				(Placement = Placements.Find(ObjectId)) == nullptr ||
				!BuildInput(Bundle, *Layer, **Placement, ExpectedCell, ExpectedTransform, ExpectedSemantic, OutError) ||
				!ReadTag(*It, TEXT("ProjectWorld.Cell="), CellId) || CellId != ExpectedCell ||
				!ReadTag(*It, SemanticTagPrefix, Semantic) || Semantic != ExpectedSemantic ||
				It->GetActorGuid() != ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|gameplay_actor|") + ObjectId) ||
				!It->GetActorTransform().Equals(ExpectedTransform, 0.01) ||
				!ValidateActor(*It, **Placement, ProjectWorldGeneratedGeometry::StableGuid(
					Bundle.GridId + TEXT("|gameplay_data|") + ObjectId), OutError) ||
				!AddArtifact(*It, Semantic, Inventory, OutError))
			{
				OutError = OutError.IsEmpty()
					? FString::Printf(TEXT("Gameplay placement inventory is invalid: %s"), *ObjectId)
					: OutError;
				return false;
			}
			ActualIds.Add(ObjectId);
		}
		if (ActualIds.Num() != Placements.Num())
		{
			OutError = TEXT("Gameplay placement inventory does not exactly cover the designer placement set.");
			return false;
		}
		OutResult.GameplayPlacementActorCount = ActualIds.Num();
		return true;
	}

	bool BuildInput(
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationLayer& Layer,
		const FProjectWorldGameplayPlacement& Placement,
		FString& OutCellId,
		FTransform& OutTransform,
		FString& OutHash,
		FString& OutError)
	{
		const FProjectWorldCanonicalCell* Cell = FindCell(Bundle, Placement.CellX, Placement.CellY);
		if (Cell == nullptr || Placement.EastingMeters < Cell->Bounds.X || Placement.EastingMeters > Cell->Bounds.Z ||
			Placement.NorthingMeters < Cell->Bounds.Y || Placement.NorthingMeters > Cell->Bounds.W)
		{
			OutError = FString::Printf(
				TEXT("Gameplay placement is outside its declared canonical cell: %s"), *Placement.ObjectId);
			return false;
		}
		TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
		FString DefinitionIdentity;
		FText ProviderError;
		if (!SpawnService.IsValid() ||
			!SpawnService->GetDefinitionIdentity(Placement.DefinitionId, DefinitionIdentity, &ProviderError))
		{
			OutError = ProviderError.IsEmpty()
				? TEXT("ObjectDefinition spawn provider is unavailable for gameplay placement.")
				: ProviderError.ToString();
			return false;
		}
		const double Height = ProjectWorldGeneratedGeometry::SampleTerrain(
			*Cell, Placement.EastingMeters, Placement.NorthingMeters) + Placement.SurfaceOffsetMeters;
		const FVector Location = FProjectWorldCanonicalLoader::CanonicalToUnreal(
			Bundle, FVector(Placement.EastingMeters, Placement.NorthingMeters, Height));
		OutTransform = FTransform(FRotator(0.0, Placement.YawDegrees, 0.0), Location);
		OutCellId = Cell->CellId;
		FString Identity(TEXT("project_gameplay_placement_input_v1"));
		AppendToken(Identity, Layer.ContractHash);
		AppendToken(Identity, Placement.ObjectId);
		AppendToken(Identity, Placement.DefinitionId.ToString());
		AppendToken(Identity, DefinitionIdentity);
		AppendToken(Identity, Cell->CellId);
		AppendToken(Identity, Cell->Terrain.ArtifactHash);
		AppendNumber(Identity, Placement.EastingMeters);
		AppendNumber(Identity, Placement.NorthingMeters);
		AppendNumber(Identity, Placement.SurfaceOffsetMeters);
		AppendNumber(Identity, Placement.YawDegrees);
		return HashText(Identity, OutHash);
	}

	bool Apply(
		UWorld* World,
		const FProjectWorldCanonicalBundle& Bundle,
		const FProjectWorldRealizationProfile& Profile,
		FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		const FProjectWorldRealizationLayer* Layer = Profile.Layers.FindByPredicate([](const auto& Candidate)
		{
			return Candidate.GeneratorId == TEXT("project_gameplay_placement") && Candidate.GeneratorVersion == 1;
		});
		if (Layer == nullptr) return true;
		FProjectWorldLayerInventory* Inventory = OutResult.LayerInventories.FindByPredicate(
			[Layer](const auto& Candidate) { return Candidate.LayerId == Layer->LayerId; });
		FProjectWorldGameplayPlacementSet Set;
		if (Inventory == nullptr || !Load(Profile, *Layer, Set, OutError))
		{
			OutError = Inventory == nullptr ? TEXT("Gameplay placement has no dirty inventory.") : OutError;
			return false;
		}

		struct FPreparedPlacement
		{
			const FProjectWorldGameplayPlacement* Placement = nullptr;
			FString CellId;
			FTransform Transform;
			FString Semantic;
			AActor* ExistingActor = nullptr;
			bool bSpawn = false;
		};
		TArray<FPreparedPlacement> Prepared;
		TMap<FString, int32> PreparedById;
		for (const FProjectWorldGameplayPlacement& Placement : Set.Placements)
		{
			FPreparedPlacement& Entry = Prepared.AddDefaulted_GetRef();
			Entry.Placement = &Placement;
			if (!BuildInput(Bundle, *Layer, Placement, Entry.CellId, Entry.Transform, Entry.Semantic, OutError)) return false;
			PreparedById.Add(Placement.ObjectId, Prepared.Num() - 1);
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(PlacementTag)) continue;
			FString ObjectId;
			if (!ReadTag(*It, ObjectTagPrefix, ObjectId) || !PreparedById.Contains(ObjectId)) continue;
			FPreparedPlacement& Entry = Prepared[PreparedById.FindChecked(ObjectId)];
			if (Entry.ExistingActor != nullptr)
			{
				OutError = FString::Printf(TEXT("Gameplay placement actor identity is duplicated: %s"), *ObjectId);
				return false;
			}
			Entry.ExistingActor = *It;
		}

		TSet<AActor*> DestroyActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(PlacementTag)) continue;
			FString ObjectId;
			if (!ReadTag(*It, ObjectTagPrefix, ObjectId) || !PreparedById.Contains(ObjectId)) DestroyActors.Add(*It);
		}
		const bool bWholeDirty = Inventory->FinalDirtyUnits.Contains(TEXT("*"));
		for (FPreparedPlacement& Entry : Prepared)
		{
			FString ExistingSemantic;
			const bool bSelected = bWholeDirty || Inventory->FinalDirtyUnits.Contains(Entry.Placement->ObjectId);
			const bool bMatches = ReadTag(Entry.ExistingActor, SemanticTagPrefix, ExistingSemantic) &&
				ExistingSemantic == Entry.Semantic;
			Entry.bSpawn = Entry.ExistingActor == nullptr || bSelected || !bMatches;
			if (Entry.bSpawn && Entry.ExistingActor != nullptr) DestroyActors.Add(Entry.ExistingActor);
		}

		TArray<FString> RetiredPackageFiles;
		for (AActor* Actor : DestroyActors)
		{
			if (UPackage* Package = Actor->GetExternalPackage())
			{
				RetiredPackageFiles.Add(FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension()));
			}
			const FName RetiredName = MakeUniqueObjectName(
				Actor->GetOuter(), Actor->GetClass(), TEXT("ProjectWorld_RetiredGameplay"));
			if (!Actor->Rename(
				*RetiredName.ToString(), Actor->GetOuter(), REN_DontCreateRedirectors | REN_NonTransactional))
			{
				OutError = FString::Printf(TEXT("Cannot release gameplay placement actor name: %s"), *Actor->GetName());
				return false;
			}
			if (!World->EditorDestroyActor(Actor, true))
			{
				OutError = FString::Printf(TEXT("Cannot retire gameplay placement actor: %s"), *Actor->GetName());
				return false;
			}
			++OutResult.RemovedActorCount;
		}
		if (!DestroyActors.IsEmpty()) CollectGarbage(RF_NoFlags);
		for (const FString& Filename : RetiredPackageFiles)
		{
			if (IFileManager::Get().FileExists(*Filename) && !IFileManager::Get().Delete(*Filename, false, true))
			{
				OutError = FString::Printf(TEXT("Cannot remove retired gameplay placement package: %s"), *Filename);
				return false;
			}
		}

		TSharedPtr<IObjectSpawnService> SpawnService = FProjectServiceLocator::Resolve<IObjectSpawnService>();
		if (!SpawnService.IsValid())
		{
			OutError = TEXT("ObjectDefinition spawn provider is unavailable for gameplay placement.");
			return false;
		}
		for (FPreparedPlacement& Entry : Prepared)
		{
			if (!Entry.bSpawn) continue;
			const FGuid ActorGuid = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|gameplay_actor|") + Entry.Placement->ObjectId);
			const FGuid DataId = ProjectWorldGeneratedGeometry::StableGuid(
				Bundle.GridId + TEXT("|gameplay_data|") + Entry.Placement->ObjectId);
			FText SpawnError;
			AActor* Actor = SpawnService->SpawnFromDefinitionWithIdentity(
				World,
				Entry.Placement->DefinitionId,
				Entry.Transform,
				FName(*SanitizeName(TEXT("ProjectWorld_Gameplay_") + Entry.Placement->ObjectId)),
				ActorGuid,
				&SpawnError);
			AProjectWorldActor* WorldActor = Cast<AProjectWorldActor>(Actor);
			if (WorldActor == nullptr)
			{
				OutError = SpawnError.IsEmpty() ? TEXT("ObjectDefinition provider returned no ProjectWorld actor.") : SpawnError.ToString();
				return false;
			}
			Actor->SetPackageExternal(true);
			Actor->Modify();
			WorldActor->DataId = DataId;
			Actor->Tags.AddUnique(ProjectWorldGeneratedGeometry::GeneratedTag);
			Actor->Tags.AddUnique(PlacementTag);
			SetTag(Actor, TEXT("ProjectWorld.Grid="), Bundle.GridId);
			SetTag(Actor, TEXT("ProjectWorld.Cell="), Entry.CellId);
			SetTag(Actor, ObjectTagPrefix, Entry.Placement->ObjectId);
			SetTag(Actor, SemanticTagPrefix, Entry.Semantic);
			SetTag(Actor, TEXT("ProjectWorld.GameplayDefinition="), Entry.Placement->DefinitionId.ToString());
			Actor->SetActorLabel(TEXT("ProjectWorld Gameplay ") + Entry.Placement->ObjectId);
			Actor->SetIsSpatiallyLoaded(true);
			Actor->bEnableAutoLODGeneration = false;
			Actor->SetHLODLayer(nullptr);
			Actor->MarkPackageDirty();
			if (!ValidateActor(Actor, *Entry.Placement, DataId, OutError)) return false;
			const bool bTemporaryWorld = World->PersistentLevel->GetPackage()->GetName().StartsWith(TEXT("/Temp/"));
			if (!bTemporaryWorld && !SaveExternalActor(Actor))
			{
				OutError = FString::Printf(TEXT("Cannot save gameplay placement actor: %s"), *Entry.Placement->ObjectId);
				return false;
			}
			++OutResult.GameplayPlacementRewriteCount;
			++OutResult.SelfSavedActorMutationCount;
			++OutResult.CreatedActorCount;
			UE_LOG(
				LogProjectWorldGameplayPlacement,
				Display,
				TEXT("[ProjectWorldGameplayPlacement::Apply] Placement realized - object=%s definition=%s cell=%s"),
				*Entry.Placement->ObjectId,
				*Entry.Placement->DefinitionId.ToString(),
				*Entry.CellId);
		}
		return true;
	}
}
