#include "ProjectWorldMeshTerrainProducer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshAttributeUtil.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "MeshPartition.h"
#include "MeshPartitionComponent.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionModifierComponent.h"
#include "Modifiers/MeshPartitionMeshProvider.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainRuntimeRole.h"
#include "ProjectWorldMeshTerrainPartition.h"
#include "ProjectWorldMeshTerrainBuildPipeline.h"
#include "ProjectWorldMeshTerrainTransformer.h"
#include "ProjectWorldRealizationService.h"
#include "Serialization/JsonReader.h"
#include "UObject/Linker.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldMeshTerrainProducer
{
	const TCHAR* SharedDefinitionObjectPath =
		TEXT("/ProjectWorldMeshTerrain/Terrain/MPD_ProjectTerrain_Shared_v1.MPD_ProjectTerrain_Shared_v1");
	const FName GroundChannel(TEXT("ground"));
	const FName HydroTransitionChannel(TEXT("hydro_transition"));

	namespace
	{
		const FName AuthoringTag(TEXT("ProjectWorld.MeshTerrain.Authoring.v1"));
		const FName PartitionTag(TEXT("ProjectWorld.MeshTerrain.Partition.v1"));
		const FString OwnedTagNamespace(TEXT("ProjectWorld.MeshTerrain."));
		const FString CellTagPrefix(TEXT("ProjectWorld.MeshTerrain.Cell="));
		const FString InputTagPrefix(TEXT("ProjectWorld.MeshTerrain.Input="));
		const FString MaterialTagPrefix(TEXT("ProjectWorld.MeshTerrain.Material="));
		const FString ProducerTagPrefix(TEXT("ProjectWorld.MeshTerrain.Producer="));

		bool ImportProperty(UObject* Object, const FName Name, const TCHAR* Value, FString& OutError)
		{
			FProperty* Property = Object != nullptr ? Object->GetClass()->FindPropertyByName(Name) : nullptr;
			if (Property == nullptr || Property->ImportText_Direct(
				Value, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None) == nullptr)
			{
				OutError = FString::Printf(TEXT("Cannot configure shared Mesh Partition property: %s"), *Name.ToString());
				return false;
			}
			return true;
		}

		bool SetTransformerBool(
			FInstancedStruct& Transformer,
			const FName PropertyName,
			const bool Value,
			FString& OutError)
		{
			FBoolProperty* Property = FindFProperty<FBoolProperty>(
				Transformer.GetScriptStruct(), PropertyName);
			if (Property == nullptr)
			{
				OutError = FString::Printf(
					TEXT("Cannot configure Mesh Partition transformer property: %s"),
					*PropertyName.ToString());
				return false;
			}
			Property->SetPropertyValue_InContainer(Transformer.GetMutableMemory(), Value);
			return true;
		}

		bool SetTransformerFloat(
			FInstancedStruct& Transformer,
			const FName PropertyName,
			const float Value,
			FString& OutError)
		{
			FFloatProperty* Property = FindFProperty<FFloatProperty>(
				Transformer.GetScriptStruct(), PropertyName);
			if (Property == nullptr)
			{
				OutError = FString::Printf(
					TEXT("Cannot configure Mesh Partition transformer property: %s"),
					*PropertyName.ToString());
				return false;
			}
			Property->SetPropertyValue_InContainer(Transformer.GetMutableMemory(), Value);
			return true;
		}

		bool MakeReflectedTransformer(
			const TCHAR* StructPath,
			FInstancedStruct& OutTransformer,
			FString& OutError)
		{
			UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, StructPath);
			if (Struct == nullptr)
			{
				OutError = FString::Printf(
					TEXT("Cannot resolve Mesh Partition transformer struct: %s"), StructPath);
				return false;
			}
			OutTransformer.InitializeAs(Struct);
			return true;
		}

		UE::MeshPartition::UTransformerPipeline* CreatePipeline(
			UE::MeshPartition::UMeshPartitionDefinition* Definition,
			const FName Name,
			const bool bCollision,
			const TOptional<bool> UseNanite,
			FString& OutError)
		{
			using namespace UE::MeshPartition;
			UTransformerPipeline* Pipeline = FindObject<UTransformerPipeline>(Definition, *Name.ToString());
			if (Pipeline != nullptr && !Pipeline->IsA<UProjectWorldMeshTerrainBuildPipeline>())
			{
				Pipeline->ClearFlags(RF_Public | RF_Standalone);
				Pipeline->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				Pipeline = nullptr;
			}
			if (Pipeline == nullptr)
			{
				Pipeline = NewObject<UProjectWorldMeshTerrainBuildPipeline>(
					Definition, Name, RF_Public | RF_Transactional);
			}
			FArrayProperty* Property = FindFProperty<FArrayProperty>(
				UTransformerPipeline::StaticClass(), TEXT("Transformers"));
			auto* Transformers = Property != nullptr
				? Property->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(Pipeline)
				: nullptr;
			if (Transformers == nullptr)
			{
				OutError = TEXT("Cannot configure Mesh Partition transformer pipeline.");
				return nullptr;
			}
			Transformers->Reset();
			if (bCollision)
			{
				FInstancedStruct Collision;
				if (!MakeReflectedTransformer(
					TEXT("/Script/MeshPartitionEditor.CollisionTransformer"), Collision, OutError))
				{
					return nullptr;
				}
				Transformers->Add(MoveTemp(Collision));
			}
			if (UseNanite.IsSet())
			{
				FInstancedStruct StaticMesh;
				if (!MakeReflectedTransformer(
						TEXT("/Script/MeshPartitionEditor.StaticMeshTransformer"), StaticMesh, OutError) ||
					!SetTransformerBool(StaticMesh, TEXT("bUseNanite"), UseNanite.GetValue(), OutError) ||
					(UseNanite.GetValue() && !SetTransformerFloat(
						StaticMesh, TEXT("NaniteFallbackPercentTriangles"), 1.0f, OutError)))
				{
					return nullptr;
				}
				Transformers->Add(MoveTemp(StaticMesh));
			}
			Transformers->Add(FInstancedStruct::Make<FProjectWorldMeshTerrainTransformer>());
			return Pipeline;
		}

		bool ConfigureRuntimeSettings(
			UE::MeshPartition::UMeshPartitionDefinition* Definition,
			FString& OutError)
		{
			using namespace UE::MeshPartition;
			const FStructProperty* Property = FindFProperty<FStructProperty>(
				Definition->GetClass(), TEXT("PerPlatformRuntimeSettings"));
			FPerPlatformRuntimeSettings* Settings = Property != nullptr
				? Property->ContainerPtrToValuePtr<FPerPlatformRuntimeSettings>(Definition)
				: nullptr;
			if (Settings == nullptr)
			{
				OutError = TEXT("Cannot configure Mesh Partition platform runtime settings.");
				return false;
			}
			Settings->Default.BuildVariantNames = {TEXT("collision"), TEXT("fallback")};
			Settings->PerPlatform.Reset();
			Settings->PerPlatform.Add(TEXT("Windows"), {{TEXT("collision"), TEXT("nanite")}});
			Settings->PerPlatform.Add(TEXT("WindowsClient"), {{TEXT("collision"), TEXT("nanite")}});
			Settings->PerPlatform.Add(TEXT("WindowsServer"), {{TEXT("collision")}});
			Settings->PerPlatform.Add(TEXT("LinuxServer"), {{TEXT("collision")}});
			return true;
		}

		bool ConfigureDefinition(
			UE::MeshPartition::UMeshPartitionDefinition* Definition,
			UMaterialInterface* Material,
			FString& OutError)
		{
			FObjectPropertyBase* MaterialProperty = FindFProperty<FObjectPropertyBase>(
				Definition->GetClass(), TEXT("Material"));
			if (Material == nullptr || MaterialProperty == nullptr)
			{
				OutError = TEXT("Shared Mesh Partition definition cannot resolve the accepted terrain material.");
				return false;
			}
			MaterialProperty->SetObjectPropertyValue_InContainer(Definition, Material);
			if (!ImportProperty(Definition, TEXT("ModifierTypePriorities"), TEXT("(Base)"), OutError) ||
				!ImportProperty(Definition, TEXT("ChannelMap"),
					TEXT("(ChannelDescs=((Name=\"ground\"),(Name=\"hydro_transition\")))"), OutError) ||
				!ImportProperty(Definition, TEXT("ChannelTexelSize"), TEXT("3000.0"), OutError) ||
				!ImportProperty(Definition, TEXT("ChannelUVLayoutMethod"), TEXT("ReferenceBoxProject"), OutError) ||
				!ImportProperty(Definition, TEXT("CompiledSectionBuildVariants"),
					TEXT("((Name=\"collision\",MaxSectionComplexity=2048,bSplitSectionsToMatchWorldPartitionRuntimeGrid=False),(Name=\"nanite\",MaxSectionComplexity=2048,bSplitSectionsToMatchWorldPartitionRuntimeGrid=False),(Name=\"fallback\",MaxSectionComplexity=2048,bSplitSectionsToMatchWorldPartitionRuntimeGrid=False))"), OutError) ||
				!ConfigureRuntimeSettings(Definition, OutError))
			{
				return false;
			}
			auto* VariantsProperty = FindFProperty<FArrayProperty>(
				Definition->GetClass(), TEXT("CompiledSectionBuildVariants"));
			auto* Variants = VariantsProperty != nullptr
				? VariantsProperty->ContainerPtrToValuePtr<TArray<UE::MeshPartition::FCompiledSectionBuildVariant>>(Definition)
				: nullptr;
			if (Variants == nullptr || Variants->Num() != 3)
			{
				OutError = TEXT("Shared Mesh Partition definition has an invalid build variant array.");
				return false;
			}
			(*Variants)[0].TransformerPipeline = CreatePipeline(
				Definition, TEXT("CollisionPipeline"), true, {}, OutError);
			(*Variants)[1].TransformerPipeline = CreatePipeline(
				Definition, TEXT("NanitePipeline"), false, true, OutError);
			(*Variants)[2].TransformerPipeline = CreatePipeline(
				Definition, TEXT("FallbackPipeline"), false, false, OutError);
			return (*Variants)[0].TransformerPipeline != nullptr &&
				(*Variants)[1].TransformerPipeline != nullptr &&
				(*Variants)[2].TransformerPipeline != nullptr;
		}

		bool ValidateDefinition(
			const UE::MeshPartition::UMeshPartitionDefinition* Definition,
			const FString& TerrainMaterialObjectPath,
			FString& OutError)
		{
			using namespace UE::MeshPartition;
			auto Reject = [&OutError](const TCHAR* Message)
			{
				OutError = Message;
				return false;
			};
			if (Definition == nullptr || Definition->GetMaterial() == nullptr ||
				Definition->GetMaterial()->GetPathName() != TerrainMaterialObjectPath ||
				Definition->GetModifierTypePriorities() != TArray<FName>({TEXT("Base")}) ||
				Definition->GetChannelTexelSize() != 3000.0f ||
				Definition->GetChannelUVLayoutMethod() != EChannelCollectionUVLayoutMethod::ReferenceBoxProject ||
				Definition->GetChannelMap().GetNumChannels() != 2 ||
				Definition->GetChannelMap().FindChannel(GroundChannel) != 0 ||
				Definition->GetChannelMap().FindChannel(HydroTransitionChannel) != 1 ||
				!Definition->GetPhysicalMaterialChannels().IsEmpty() ||
				Definition->GetDefaultPhysicalMaterial() != nullptr)
			{
				return Reject(TEXT("Shared Mesh Partition definition material or channel ABI is stale."));
			}
			if (Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("Windows"), TEXT("Windows")) !=
					TArray<FName>({TEXT("collision"), TEXT("nanite")}) ||
				Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("WindowsClient"), TEXT("Windows")) !=
					TArray<FName>({TEXT("collision"), TEXT("nanite")}) ||
				Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("WindowsServer"), TEXT("Windows")) !=
					TArray<FName>({TEXT("collision")}) ||
				Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("LinuxServer"), TEXT("Linux")) !=
					TArray<FName>({TEXT("collision")}) ||
				Definition->GetCompiledSectionBuildVariantNamesForPlatform(TEXT("Unknown"), TEXT("Unknown")) !=
					TArray<FName>({TEXT("collision"), TEXT("fallback")}))
			{
				return Reject(TEXT("Shared Mesh Partition definition platform policy is stale."));
			}
			const TConstArrayView<FCompiledSectionBuildVariant> Variants =
				Definition->GetCompiledSectionBuildVariants();
			const TArray<FName> ExpectedNames = {TEXT("collision"), TEXT("nanite"), TEXT("fallback")};
			const TArray<FString> ExpectedFirstTransformers = {
				TEXT("/Script/MeshPartitionEditor.CollisionTransformer"),
				TEXT("/Script/MeshPartitionEditor.StaticMeshTransformer"),
				TEXT("/Script/MeshPartitionEditor.StaticMeshTransformer")};
			if (Variants.Num() != ExpectedNames.Num())
			{
				return Reject(TEXT("Shared Mesh Partition definition build variants are stale."));
			}
			for (int32 Index = 0; Index < Variants.Num(); ++Index)
			{
				const FCompiledSectionBuildVariant& Variant = Variants[Index];
				const UTransformerPipeline* Pipeline = Variant.TransformerPipeline;
				if (Variant.Name != ExpectedNames[Index] || Variant.MaxSectionComplexity != 2048.0 ||
					Variant.bSplitSectionsToMatchWorldPartitionRuntimeGrid || Pipeline == nullptr ||
					!Pipeline->IsEditorOnly())
				{
					return Reject(TEXT("Shared Mesh Partition definition variant policy is stale."));
				}
				const TArray<TInstancedStruct<FTransformer>>& Transformers = Pipeline->GetTransformers();
				if (Transformers.Num() != 2 || Transformers[0].GetScriptStruct() == nullptr ||
					Transformers[0].GetScriptStruct()->GetPathName() != ExpectedFirstTransformers[Index] ||
					Transformers[1].GetScriptStruct() != FProjectWorldMeshTerrainTransformer::StaticStruct())
				{
					return Reject(TEXT("Shared Mesh Partition definition transformer pipeline is stale."));
				}
				if (Index > 0)
				{
					const FBoolProperty* UseNanite = FindFProperty<FBoolProperty>(
						Transformers[0].GetScriptStruct(), TEXT("bUseNanite"));
					const FBoolProperty* AffectsNavigation = FindFProperty<FBoolProperty>(
						Transformers[0].GetScriptStruct(), TEXT("bCanEverAffectNavigation"));
					if (UseNanite == nullptr ||
						UseNanite->GetPropertyValue_InContainer(Transformers[0].GetMemory()) != (Index == 1) ||
						AffectsNavigation == nullptr ||
						AffectsNavigation->GetPropertyValue_InContainer(Transformers[0].GetMemory()))
					{
						return Reject(TEXT("Shared Mesh Partition definition Nanite policy is stale."));
					}
					if (Index == 1)
					{
						const FFloatProperty* FallbackPercent = FindFProperty<FFloatProperty>(
							Transformers[0].GetScriptStruct(), TEXT("NaniteFallbackPercentTriangles"));
						if (FallbackPercent == nullptr ||
							FallbackPercent->GetPropertyValue_InContainer(Transformers[0].GetMemory()) != 1.0f)
						{
							return Reject(TEXT("Shared Mesh Partition definition fallback policy is stale."));
						}
					}
				}
				else
				{
					const FBoolProperty* AffectsNavigation = FindFProperty<FBoolProperty>(
						Transformers[0].GetScriptStruct(), TEXT("bCanEverAffectNavigation"));
					const FBoolProperty* FastCook = FindFProperty<FBoolProperty>(
						Transformers[0].GetScriptStruct(), TEXT("bFastCook"));
					const FBoolProperty* DisableActiveEdgePrecompute = FindFProperty<FBoolProperty>(
						Transformers[0].GetScriptStruct(), TEXT("bDisableActiveEdgePrecompute"));
					if (AffectsNavigation == nullptr || FastCook == nullptr ||
						DisableActiveEdgePrecompute == nullptr ||
						!AffectsNavigation->GetPropertyValue_InContainer(Transformers[0].GetMemory()) ||
						FastCook->GetPropertyValue_InContainer(Transformers[0].GetMemory()) ||
						DisableActiveEdgePrecompute->GetPropertyValue_InContainer(Transformers[0].GetMemory()))
					{
						return Reject(TEXT("Shared Mesh Partition definition collision policy is stale."));
					}
				}
			}
			return true;
		}

		bool SaveDefinition(UE::MeshPartition::UMeshPartitionDefinition* Definition, FString& OutError)
		{
			UPackage* Package = Definition != nullptr ? Definition->GetPackage() : nullptr;
			if (Package == nullptr)
			{
				OutError = TEXT("Shared Mesh Partition definition has no package.");
				return false;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Arguments;
			Arguments.TopLevelFlags = RF_Public | RF_Standalone;
			Arguments.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Definition, *Filename, Arguments))
			{
				OutError = FString::Printf(TEXT("Cannot save shared Mesh Partition definition: %s"), *Filename);
				return false;
			}
			return true;
		}

		FString FindTagValue(const TArray<FName>& Tags, const FString& Prefix)
		{
			for (const FName Tag : Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(Prefix))
				{
					return Value.RightChop(Prefix.Len());
				}
			}
			return FString();
		}

		FString FindTagValue(const AActor* Actor, const FString& Prefix)
		{
			return Actor != nullptr ? FindTagValue(Actor->Tags, Prefix) : FString();
		}

		bool HashText(const FString& Value, FString& OutHash)
		{
			FTCHARToUTF8 Utf8(*Value);
			TArray<uint8> Bytes;
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			return FProjectSha256::HashBuffer(Bytes, OutHash);
		}

		bool AddActorArtifact(
			const AActor* Actor,
			const FString& SemanticHash,
			FProjectWorldLayerInventory& Inventory,
			FString& OutError)
		{
			if (Actor == nullptr)
			{
				OutError = TEXT("Mesh Terrain inventory found a null actor.");
				return false;
			}
			FString Filename = FPackageName::LongPackageNameToFilename(
				Actor->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
			Filename = FPaths::ConvertRelativePathToFull(Filename);
			if (!FPaths::FileExists(Filename))
			{
				OutError = FString::Printf(TEXT("Mesh Terrain actor package was not saved: %s"), *Filename);
				return false;
			}
			FString Relative = Filename;
			const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || Relative.StartsWith(TEXT("..")))
			{
				OutError = FString::Printf(TEXT("Mesh Terrain actor package escapes the project: %s"), *Filename);
				return false;
			}
			Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
			FString Digest;
			if (!FProjectSha256::HashFile(Filename, Digest))
			{
				OutError = FString::Printf(TEXT("Cannot hash Mesh Terrain actor package: %s"), *Filename);
				return false;
			}
			Inventory.Artifacts.Add({Relative, TEXT("external_actor"), Digest, SemanticHash});
			return true;
		}

		UE::Geometry::FDynamicMesh3 BuildCellMesh(
			const FProjectWorldCanonicalBundle& Bundle,
			const FProjectWorldCanonicalCell& Cell)
		{
			using namespace UE::Geometry;
			FDynamicMesh3 Mesh;
			Mesh.EnableVertexUVs(FVector2f::ZeroVector);
			for (int32 Row = 0; Row < Cell.Terrain.SamplesY; ++Row)
			{
				for (int32 Column = 0; Column < Cell.Terrain.SamplesX; ++Column)
				{
					const int32 Index = Row * Cell.Terrain.SamplesX + Column;
					const FVector Position = FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle, FVector(
						Cell.Terrain.Bounds.X + Column * Cell.Terrain.SampleSpacing.X,
						FProjectWorldCanonicalLoader::TerrainRowNorthing(Cell.Terrain, Row),
						Cell.Terrain.HeightsMeters[Index]));
					const int32 VertexId = Mesh.AppendVertex(FVector3d(Position));
					Mesh.SetVertexUV(VertexId, FVector2f(
						static_cast<float>(Column) / static_cast<float>(Cell.Terrain.SamplesX - 1),
						static_cast<float>(Row) / static_cast<float>(Cell.Terrain.SamplesY - 1)));
				}
			}
			ProjectWorldMeshTerrainProducer::AppendRendererFacingGridTriangles(
				Mesh, Cell.Terrain.SamplesX, Cell.Terrain.SamplesY);
			Mesh.EnableAttributes();
			CopyVertexUVsToOverlay(Mesh, *Mesh.Attributes()->PrimaryUV());
			Mesh.Attributes()->SetNumWeightLayers(Cell.Terrain.SurfaceRoles.Num());
			for (int32 LayerIndex = 0; LayerIndex < Cell.Terrain.SurfaceRoles.Num(); ++LayerIndex)
			{
				FDynamicMeshWeightAttribute* Attribute = Mesh.Attributes()->GetWeightLayer(LayerIndex);
				const FName Role = Cell.Terrain.SurfaceRoles[LayerIndex];
				Attribute->SetName(Role);
				Attribute->Initialize();
				const TArray<float>& Weights = Cell.Terrain.SurfaceWeights.FindChecked(Role);
				for (int32 VertexId = 0; VertexId < Weights.Num(); ++VertexId)
				{
					Attribute->SetValue(VertexId, &Weights[VertexId]);
				}
			}
			return Mesh;
		}

		UE::MeshPartition::AMeshPartition* FindPartition(UWorld* World)
		{
			for (TActorIterator<UE::MeshPartition::AMeshPartition> It(World); It; ++It)
			{
				if (It->Tags.Contains(PartitionTag) && It->IsA<AProjectWorldMeshTerrainPartition>())
				{
					return *It;
				}
			}
			return nullptr;
		}

		bool EnsureEditorComponent(UE::MeshPartition::AMeshPartition* Partition, FString& OutError)
		{
			if (Partition->GetMeshPartitionComponent() != nullptr)
			{
				return true;
			}
			UClass* EditorComponentClass = LoadClass<UE::MeshPartition::UMeshPartitionComponent>(
				nullptr, TEXT("/Script/MeshPartitionEditor.MeshPartitionEditorComponent"));
			if (EditorComponentClass == nullptr)
			{
				OutError = TEXT("Mesh Partition editor component class is unavailable.");
				return false;
			}
			auto* Component = NewObject<UE::MeshPartition::UMeshPartitionComponent>(
				Partition, EditorComponentClass, TEXT("MeshPartitionEditorComponent"), RF_Transactional);
			Partition->SetMeshPartitionComponent(Component);
			return true;
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
			UPackage* Package = Actor != nullptr ? Actor->GetExternalPackage() : nullptr;
			const FString Filename = Package != nullptr
				? FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension())
				: FString();
			if (Actor != nullptr && !World->EditorDestroyActor(Actor, true))
			{
				return false;
			}
			if (Package != nullptr)
			{
				ResetLoaders(Package);
			}
			return Filename.IsEmpty() || !IFileManager::Get().FileExists(*Filename) ||
				IFileManager::Get().Delete(*Filename, false, true);
		}

		bool Apply(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			const FString&,
			UMaterialInterface* Material,
			const FString& ProducerFingerprint,
			FProjectWorldRealizationResult& OutResult,
			FString& OutError)
		{
			bool bValidFingerprint = ProducerFingerprint.Len() == 64;
			for (const TCHAR Character : ProducerFingerprint)
			{
				bValidFingerprint &= (Character >= TEXT('0') && Character <= TEXT('9')) ||
					(Character >= TEXT('a') && Character <= TEXT('f'));
			}
			if (!bValidFingerprint)
			{
				OutError = TEXT("Mesh Terrain requires the realization request's producer fingerprint.");
				return false;
			}
			if (World == nullptr || Material == nullptr ||
				!ValidateSharedDefinition(Material != nullptr ? Material->GetPathName() : FString(), OutError))
			{
				if (World != nullptr && Material == nullptr)
				{
					OutError = TEXT("Mesh Terrain requires ProjectWorld's authenticated terrain.default material.");
				}
				return false;
			}
			UE::MeshPartition::UMeshPartitionDefinition* Definition =
				LoadObject<UE::MeshPartition::UMeshPartitionDefinition>(nullptr, SharedDefinitionObjectPath);
			if (Definition == nullptr)
			{
				OutError = TEXT("Shared Mesh Partition definition did not reload.");
				return false;
			}
			UE::MeshPartition::AMeshPartition* Partition = FindPartition(World);
			bool bPartitionCreated = false;
			TArray<UE::MeshPartition::AMeshPartition*> ExtraPartitions;
			for (TActorIterator<UE::MeshPartition::AMeshPartition> It(World); It; ++It)
			{
				if (It->Tags.Contains(PartitionTag) && *It != Partition)
				{
					ExtraPartitions.Add(*It);
				}
			}
			for (UE::MeshPartition::AMeshPartition* ExtraPartition : ExtraPartitions)
			{
				if (!RemoveExternalActor(World, ExtraPartition))
				{
					OutError = TEXT("Superseded Mesh Terrain partition actor could not be removed.");
					return false;
				}
				++OutResult.RemovedActorCount;
			}
			if (Partition == nullptr)
			{
				Partition = World->SpawnActor<AProjectWorldMeshTerrainPartition>();
				Partition->Tags.Add(AuthoringTag);
				Partition->Tags.Add(PartitionTag);
				Partition->SetActorLabel(TEXT("ProjectWorld Mesh Terrain"));
				Partition->SetIsSpatiallyLoaded(false);
				bPartitionCreated = true;
				++OutResult.CreatedActorCount;
			}
			if (!EnsureEditorComponent(Partition, OutError))
			{
				return false;
			}
			Partition->SetMeshPartitionDefinition(Definition);
			Partition->MarkPackageDirty();
			if (bPartitionCreated && !SaveExternalActor(Partition))
			{
				OutError = TEXT("Mesh Terrain partition actor package could not be saved.");
				return false;
			}
			if (bPartitionCreated)
			{
				++OutResult.SelfSavedActorMutationCount;
			}
			TSet<FString> ExpectedCells;
			for (const FProjectWorldCanonicalCell& Cell : Bundle.Cells)
			{
				ExpectedCells.Add(Cell.CellId);
				AActor* Existing = nullptr;
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (It->Tags.Contains(AuthoringTag) && FindTagValue(*It, CellTagPrefix) == Cell.CellId)
					{
						Existing = *It;
						break;
					}
				}
				const FString MaterialPath = Material->GetPathName();
				if (!bPartitionCreated && Existing != nullptr && MatchesBaseIdentity(
					Existing->Tags, Cell.CellId, Cell.Terrain.ArtifactHash, MaterialPath,
					ProducerFingerprint))
				{
					continue;
				}
				AActor* BaseActor = Existing != nullptr ? Existing : World->SpawnActor<AActor>();
				UE::MeshPartition::UMeshProviderModifier* Provider = Existing != nullptr
					? Cast<UE::MeshPartition::UMeshProviderModifier>(Existing->GetRootComponent())
					: NewObject<UE::MeshPartition::UMeshProviderModifier>(BaseActor, NAME_None, RF_Transactional);
				if (BaseActor == nullptr || Provider == nullptr)
				{
					OutError = FString::Printf(
						TEXT("Mesh Terrain base actor has an incompatible provider for cell: %s"),
						*Cell.CellId);
					return false;
				}
				BaseActor->Tags.RemoveAllSwap([](const FName Tag)
				{
				return Tag.ToString().StartsWith(OwnedTagNamespace);
				});
				BaseActor->Tags.Append(BuildBaseIdentityTags(
					Cell.CellId, Cell.Terrain.ArtifactHash, MaterialPath, ProducerFingerprint));
				BaseActor->SetActorLabel(TEXT("ProjectWorld Mesh Terrain Base ") + Cell.CellId);
				BaseActor->SetIsSpatiallyLoaded(true);
				Provider->SetIgnoreChanged(true);
				Provider->SetAffectedMeshPartition(Partition);
				Provider->SetMesh(BuildCellMesh(Bundle, Cell));
				Provider->SetMaterial(0, Material != nullptr ? Material : Definition->GetMaterial());
				if (Existing == nullptr)
				{
					BaseActor->SetRootComponent(Provider);
					BaseActor->AddInstanceComponent(Provider);
					Provider->RegisterComponent();
				}
				Provider->SetIgnoreChanged(false);
				Provider->PostEditChange();
				BaseActor->MarkPackageDirty();
				if (!SaveExternalActor(BaseActor))
				{
					OutError = FString::Printf(
						TEXT("Mesh Terrain base actor package could not be saved for cell: %s"),
						*Cell.CellId);
					return false;
				}
				if (Existing != nullptr)
				{
					++OutResult.UpdatedActorCount;
				}
				else
				{
					++OutResult.CreatedActorCount;
				}
				++OutResult.SelfSavedActorMutationCount;
			}
			TArray<AActor*> Orphans;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const FString CellId = FindTagValue(*It, CellTagPrefix);
				if (It->Tags.Contains(AuthoringTag) && !CellId.IsEmpty() && !ExpectedCells.Contains(CellId))
				{
					Orphans.Add(*It);
				}
			}
			for (AActor* Orphan : Orphans)
			{
				if (!RemoveExternalActor(World, Orphan))
				{
					OutError = TEXT("Orphaned Mesh Terrain base actor could not be removed.");
					return false;
				}
				++OutResult.RemovedActorCount;
			}
			OutResult.TerrainSectionCount = Bundle.Cells.Num();
			return true;
		}

		bool Delete(
			UWorld* World,
			const FProjectWorldCanonicalBundle&,
			FProjectWorldRealizationResult& OutResult,
			FString& OutError)
		{
			if (World == nullptr)
			{
				OutError = TEXT("Mesh Terrain delete has no world.");
				return false;
			}
			TArray<AActor*> Actors;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->Tags.Contains(AuthoringTag))
				{
					Actors.Add(*It);
				}
			}
			for (AActor* Actor : Actors)
			{
				if (!RemoveExternalActor(World, Actor))
				{
					OutError = TEXT("Mesh Terrain authoring actor could not be deleted.");
					return false;
				}
				++OutResult.RemovedActorCount;
			}
			return true;
		}

		bool CaptureArtifacts(
			UWorld* World,
			const FProjectWorldCanonicalBundle& Bundle,
			FProjectWorldLayerInventory& Inventory,
			FProjectWorldRealizationResult& OutResult,
			FString& OutError)
		{
			UE::MeshPartition::AMeshPartition* Partition = FindPartition(World);
			if (Partition == nullptr || Partition->GetMeshPartitionDefinition() == nullptr ||
				Partition->GetMeshPartitionDefinition()->GetPathName() != SharedDefinitionObjectPath ||
				Partition->GetIsSpatiallyLoaded())
			{
				OutError = TEXT("Mesh Terrain partition ownership or shared definition is invalid.");
				return false;
			}
			FString PartitionSemantic;
			if (!HashText(FString(TEXT("project_mesh_terrain_partition_v1|")) + SharedDefinitionObjectPath,
				PartitionSemantic) || !AddActorArtifact(Partition, PartitionSemantic, Inventory, OutError))
			{
				return false;
			}

			TSet<FString> ActualCells;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const FString CellId = FindTagValue(*It, CellTagPrefix);
				if (CellId.IsEmpty())
				{
					continue;
				}
				const FProjectWorldCanonicalCell* Cell = Bundle.Cells.FindByPredicate([&CellId](const auto& Candidate)
				{
					return Candidate.CellId == CellId;
				});
				const FString InputHash = FindTagValue(*It, InputTagPrefix);
				const UE::MeshPartition::UMeshProviderModifier* Provider =
					Cast<UE::MeshPartition::UMeshProviderModifier>(It->GetRootComponent());
				const UE::Geometry::FDynamicMesh3* Mesh = Provider != nullptr ? Provider->GetMesh() : nullptr;
				const FString MaterialPath = Partition->GetMeshPartitionDefinition()->GetMaterial()->GetPathName();
				if (Cell == nullptr || ActualCells.Contains(CellId) || !MatchesBaseIdentity(
					It->Tags, CellId, Cell->Terrain.ArtifactHash, MaterialPath,
					Inventory.GeneratorFingerprint) ||
					!It->GetIsSpatiallyLoaded() || Provider == nullptr || Provider->GetAffectedMeshPartition() != Partition ||
					Mesh == nullptr || Mesh->VertexCount() != Cell->Terrain.SamplesX * Cell->Terrain.SamplesY ||
					Mesh->Attributes() == nullptr || Mesh->Attributes()->NumWeightLayers() != 2)
				{
					OutError = FString::Printf(
						TEXT("Mesh Terrain base ownership is invalid for cell: %s input=%s producer=%s material_tag=%s expected_material=%s"),
						*CellId,
						*InputHash,
						*FindTagValue(*It, ProducerTagPrefix),
						*FindTagValue(*It, MaterialTagPrefix),
						*MaterialPath);
					return false;
				}
				const auto* Ground = Mesh->Attributes()->GetWeightLayer(0);
				const auto* Hydro = Mesh->Attributes()->GetWeightLayer(1);
				if (Ground == nullptr || Hydro == nullptr || Ground->GetName() != GroundChannel ||
					Hydro->GetName() != HydroTransitionChannel)
				{
					OutError = FString::Printf(TEXT("Mesh Terrain channel layout is invalid for cell: %s"), *CellId);
					return false;
				}
				ActualCells.Add(CellId);
				FString Semantic;
			if (!HashText(FString::Printf(TEXT("project_mesh_terrain_base_v2|%s|%s|%s"),
				*CellId, *InputHash, *MaterialPath),
					Semantic) || !AddActorArtifact(*It, Semantic, Inventory, OutError))
				{
					return false;
				}
			}
			if (ActualCells.Num() != Bundle.Cells.Num())
			{
				OutError = TEXT("Mesh Terrain base inventory does not exactly cover canonical cells.");
				return false;
			}
			const UClass* CompiledSectionClass =
				LoadClass<AActor>(nullptr, TEXT("/Script/MeshPartition.CompiledSection"));
			if (CompiledSectionClass != nullptr)
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					if (!It->IsA(CompiledSectionClass) ||
						!ProjectWorldTerrainRuntimeRole::HasRole(**It))
					{
						continue;
					}
					if (!It->GetIsSpatiallyLoaded())
					{
						OutError = TEXT("Compiled Mesh Terrain section is not spatially loaded.");
						return false;
					}
					FString Semantic;
					if (!HashText(FString(TEXT("project_mesh_terrain_compiled_v1|")) +
						It->GetPackage()->GetName(), Semantic) ||
						!AddActorArtifact(*It, Semantic, Inventory, OutError))
					{
						return false;
					}
				}
			}
			OutResult.TerrainSectionCount = ActualCells.Num();
			return true;
		}
	}

	TArray<FName> BuildBaseIdentityTags(const FString& CellId, const FString& Input,
		const FString& MaterialPath, const FString& ProducerFingerprint)
	{
		return {
			AuthoringTag,
			FName(*(CellTagPrefix + CellId)),
			FName(*(InputTagPrefix + Input)),
			FName(*(MaterialTagPrefix + MaterialPath)),
			FName(*(ProducerTagPrefix + ProducerFingerprint))};
	}

	bool MatchesBaseIdentity(
		const TArray<FName>& Tags,
		const FString& ExpectedCellId,
		const FString& ExpectedInput,
		const FString& ExpectedMaterial,
		const FString& ExpectedProducerFingerprint)
	{
		const TArray<FName> Expected = BuildBaseIdentityTags(
			ExpectedCellId, ExpectedInput, ExpectedMaterial, ExpectedProducerFingerprint);
		TArray<FName> Actual;
		for (const FName Tag : Tags)
		{
			if (Tag.ToString().StartsWith(OwnedTagNamespace))
			{
				Actual.Add(Tag);
			}
		}
		if (Actual.Num() != Expected.Num())
		{
			return false;
		}
		for (const FName Tag : Expected)
		{
			if (!Actual.Contains(Tag))
			{
				return false;
			}
		}
		return true;
	}

	bool EnsureSharedDefinition(UMaterialInterface* TerrainMaterial, FString& OutError)
	{
		if (TerrainMaterial == nullptr)
		{
			OutError = TEXT("Shared Mesh Partition definition requires an authenticated terrain material.");
			return false;
		}
		UE::MeshPartition::UMeshPartitionDefinition* Definition =
			LoadObject<UE::MeshPartition::UMeshPartitionDefinition>(nullptr, SharedDefinitionObjectPath);
		if (Definition == nullptr)
		{
			const FString DefinitionObjectPath(SharedDefinitionObjectPath);
			const FString PackageName = FPackageName::ObjectPathToPackageName(DefinitionObjectPath);
			const FString AssetName = FPackageName::ObjectPathToObjectName(DefinitionObjectPath);
			UPackage* Package = CreatePackage(*PackageName);
			const UE::MeshPartition::UMeshPartitionDefinition* Default =
				UE::MeshPartition::UMeshPartitionDefinition::GetDefaultMegaMeshDefinition();
			Definition = Default != nullptr
				? Cast<UE::MeshPartition::UMeshPartitionDefinition>(StaticDuplicateObject(Default, Package, *AssetName))
				: NewObject<UE::MeshPartition::UMeshPartitionDefinition>(
					Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			Definition->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Definition);
		}
		if (!ConfigureDefinition(Definition, TerrainMaterial, OutError) ||
			!SaveDefinition(Definition, OutError))
		{
			return false;
		}
		return true;
	}

	bool ValidateSharedDefinition(const FString& TerrainMaterialObjectPath, FString& OutError)
	{
		const UE::MeshPartition::UMeshPartitionDefinition* Definition =
			LoadObject<UE::MeshPartition::UMeshPartitionDefinition>(nullptr, SharedDefinitionObjectPath);
		if (Definition == nullptr)
		{
			OutError = TEXT("Versioned shared Mesh Partition definition is missing; realization cannot author it.");
			return false;
		}
		return ValidateDefinition(Definition, TerrainMaterialObjectPath, OutError);
	}

	bool ApplyLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		const FString& Settings, UMaterialInterface* Material, const FString& ProducerFingerprint,
		FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		return Apply(World, Bundle, Settings, Material, ProducerFingerprint, OutResult, OutError);
	}

	bool DeleteLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldRealizationResult& OutResult, FString& OutError)
	{
		return Delete(World, Bundle, OutResult, OutError);
	}

	bool CaptureLayer(UWorld* World, const FProjectWorldCanonicalBundle& Bundle,
		FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
		FString& OutError)
	{
		return CaptureArtifacts(World, Bundle, Inventory, OutResult, OutError);
	}
}

void ProjectWorldMeshTerrainProducer::AppendRendererFacingGridTriangles(
	UE::Geometry::FDynamicMesh3& Mesh,
	const int32 SamplesX,
	const int32 SamplesY)
{
	for (int32 Row = 0; Row < SamplesY - 1; ++Row)
	{
		for (int32 Column = 0; Column < SamplesX - 1; ++Column)
		{
			const int32 A = Row * SamplesX + Column;
			const int32 B = A + 1;
			const int32 C = A + SamplesX;
			const int32 D = C + 1;
			Mesh.AppendTriangle(A, C, B);
			Mesh.AppendTriangle(B, C, D);
		}
	}
}
