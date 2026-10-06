// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldMeshTerrainLayoutReceipt.h"
#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldTerrainRuntimeRole.h"

#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "Interface_CollisionDataProviderCore.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshPartition.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "Modifiers/MeshPartitionMeshProvider.h"
#include "UObject/UnrealType.h"
#include "Utilities/ProjectSha256.h"

namespace ProjectWorldMeshTerrainVerifyProjection
{
	const FName AuthoringTag(TEXT("ProjectWorld.MeshTerrain.Authoring.v1"));
	const FName PartitionTag(TEXT("ProjectWorld.MeshTerrain.Partition.v1"));

	FString CellId(const AActor& Actor)
	{
		const FString Prefix(TEXT("ProjectWorld.MeshTerrain.Cell="));
		for (const FName& Tag : Actor.Tags)
		{
			const FString Value = Tag.ToString();
			if (Value.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return Value.RightChop(Prefix.Len());
			}
		}
		return FString();
	}

	FString MeshDigest(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		FString Data = FString::Printf(TEXT("vertices=%d|triangles=%d|"), Mesh.VertexCount(), Mesh.TriangleCount());
		for (int32 VertexId : Mesh.VertexIndicesItr())
		{
			const FVector3d Position = Mesh.GetVertex(VertexId);
			const FVector2f UV = Mesh.GetVertexUV(VertexId);
			Data += FString::Printf(TEXT("v%d=%.5f,%.5f,%.5f:%.6f,%.6f;"),
				VertexId, Position.X, Position.Y, Position.Z, UV.X, UV.Y);
		}
		for (int32 TriangleId : Mesh.TriangleIndicesItr())
		{
			const auto Triangle = Mesh.GetTriangle(TriangleId);
			Data += FString::Printf(TEXT("t%d=%d,%d,%d;"), TriangleId, Triangle.A, Triangle.B, Triangle.C);
		}
		if (Mesh.Attributes() != nullptr)
		{
			for (int32 LayerIndex = 0; LayerIndex < Mesh.Attributes()->NumWeightLayers(); ++LayerIndex)
			{
				const auto* Layer = Mesh.Attributes()->GetWeightLayer(LayerIndex);
				Data += FString::Printf(TEXT("weight=%s;"), *Layer->GetName().ToString());
				for (int32 VertexId : Mesh.VertexIndicesItr())
				{
					float Weight = 0.0f;
					Layer->GetValue(VertexId, &Weight);
					Data += FString::Printf(TEXT("%.6f,"), Weight);
				}
			}
		}
		return ProjectWorldProjection::HashText(Data);
	}

	FString SectionKey(const AActor& Section)
	{
		FString Variant(TEXT("none"));
		if (const FStructProperty* BuildInfoProperty = FindFProperty<FStructProperty>(
			Section.GetClass(), TEXT("BuildInfo")))
		{
			const void* BuildInfo = BuildInfoProperty->ContainerPtrToValuePtr<void>(&Section);
			if (const FNameProperty* VariantProperty = FindFProperty<FNameProperty>(
				BuildInfoProperty->Struct, TEXT("BuildVariantName")))
			{
				Variant = VariantProperty->GetPropertyValue_InContainer(BuildInfo).ToString();
			}
		}
		const FBox Bounds = Section.GetComponentsBoundingBox(true);
		return FString::Printf(TEXT("%s:%.1f,%.1f,%.1f:%.1f,%.1f,%.1f"), *Variant,
			Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z, Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z);
	}

	bool TextureDigest(UTexture& Texture, FString& OutDigest, FString& OutError)
	{
		FTextureSource& Source = Texture.Source;
		if (Source.GetNumBlocks() < 1 || Source.GetNumLayers() < 1 || Source.GetNumMips() < 1)
		{
			OutError = TEXT("Compiled Mesh Terrain channel texture has no source mips.");
			return false;
		}
		FString Parts = FString::Printf(TEXT("%lldx%lld|blocks=%d|layers=%d|mips=%d|"),
			Source.GetSizeX(), Source.GetSizeY(), Source.GetNumBlocks(),
			Source.GetNumLayers(), Source.GetNumMips());
		for (int32 Block = 0; Block < Source.GetNumBlocks(); ++Block)
		{
			for (int32 Layer = 0; Layer < Source.GetNumLayers(); ++Layer)
			{
				Parts += FString::Printf(TEXT("format=%d;"), static_cast<int32>(Source.GetFormat(Layer)));
				for (int32 MipIndex = 0; MipIndex < Source.GetNumMips(); ++MipIndex)
				{
					TArray64<uint8> Mip;
					if (!Source.GetMipData(Mip, Block, Layer, MipIndex) || Mip.Num() > MAX_int32)
					{
						OutError = TEXT("Compiled Mesh Terrain channel texture mip is unreadable.");
						return false;
					}
					TArray<uint8> Bytes;
					Bytes.Append(Mip.GetData(), static_cast<int32>(Mip.Num()));
					FString Hash;
					if (!FProjectSha256::HashBuffer(Bytes, Hash))
					{
						OutError = TEXT("Compiled Mesh Terrain channel texture mip cannot be hashed.");
						return false;
					}
					Parts += Hash + TEXT(";");
				}
			}
		}
		OutDigest = ProjectWorldProjection::HashText(Parts);
		return true;
	}

	FString MaterialParameters(const UMaterialInstance& Material, const UTexture* Channel,
		const FString& ArtifactRoot)
	{
		TArray<FString> Values;
		for (const FScalarParameterValue& Parameter : Material.ScalarParameterValues)
		{
			Values.Add(FString::Printf(TEXT("scalar:%s:%d:%d=%.9g"),
				*Parameter.ParameterInfo.Name.ToString(), static_cast<int32>(Parameter.ParameterInfo.Association),
				Parameter.ParameterInfo.Index, Parameter.ParameterValue));
		}
		for (const FVectorParameterValue& Parameter : Material.VectorParameterValues)
		{
			const FLinearColor& Value = Parameter.ParameterValue;
			Values.Add(FString::Printf(TEXT("vector:%s:%d:%d=%.9g,%.9g,%.9g,%.9g"),
				*Parameter.ParameterInfo.Name.ToString(), static_cast<int32>(Parameter.ParameterInfo.Association),
				Parameter.ParameterInfo.Index, Value.R, Value.G, Value.B, Value.A));
		}
		for (const FTextureParameterValue& Parameter : Material.TextureParameterValues)
		{
			Values.Add(FString::Printf(TEXT("texture:%s:%d:%d=%s"),
				*Parameter.ParameterInfo.Name.ToString(), static_cast<int32>(Parameter.ParameterInfo.Association),
				Parameter.ParameterInfo.Index, Parameter.ParameterValue == Channel
					? TEXT("channel") : *ProjectWorldProjection::ObjectPath(Parameter.ParameterValue, ArtifactRoot)));
		}
		Values.Sort();
		return FString::Join(Values, TEXT(";"));
	}

	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		if (Scope.World == nullptr)
		{
			OutError = TEXT("Mesh Terrain verify requires a loaded map.");
			return false;
		}
		for (TActorIterator<UE::MeshPartition::AMeshPartition> It(Scope.World); It; ++It)
		{
			if (!It->Tags.Contains(PartitionTag))
			{
				continue;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("partition"), TEXT("mesh-terrain"));
			ProjectWorldProjection::AddActorCommon(Record, **It);
			Record.Fields.Remove(TEXT("name"));
			Record.Fields.Remove(TEXT("guid"));
			Record.Fields.Add(TEXT("definition"), ProjectWorldProjection::ObjectPath(
				It->GetMeshPartitionDefinition(), Scope.ArtifactRoot));
		}
		for (TActorIterator<AActor> It(Scope.World); It; ++It)
		{
			if (!It->Tags.Contains(AuthoringTag) || It->Tags.Contains(PartitionTag))
			{
				continue;
			}
			const FString Cell = CellId(**It);
			if (Cell.IsEmpty())
			{
				OutError = TEXT("Mesh Terrain base actor has no cell identity.");
				return false;
			}
			const auto* Provider = Cast<UE::MeshPartition::UMeshProviderModifier>(It->GetRootComponent());
			const UE::Geometry::FDynamicMesh3* Mesh = Provider != nullptr ? Provider->GetMesh() : nullptr;
			if (Mesh == nullptr)
			{
				OutError = TEXT("Mesh Terrain base actor has no provider mesh.");
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("base"), Cell);
			ProjectWorldProjection::AddActorCommon(Record, **It);
			Record.Fields.Remove(TEXT("name"));
			Record.Fields.Remove(TEXT("guid"));
			Record.Fields.Add(TEXT("tags"), ProjectWorldProjection::Tags(It->Tags,
				{TEXT("ProjectWorld.MeshTerrain.Input="), TEXT("ProjectWorld.MeshTerrain.Material="),
				 TEXT("ProjectWorld.MeshTerrain.Engine="), TEXT("ProjectWorld.MeshTerrain.AdapterCompiler="),
				 TEXT("ProjectWorld.MeshTerrain.Producer=")}));
			Record.Fields.Add(TEXT("mesh"), MeshDigest(*Mesh));
			Record.Fields.Add(TEXT("provider.class"), Provider->GetClass()->GetPathName());
			Record.Fields.Add(TEXT("provider.material"), ProjectWorldProjection::ObjectPath(
				Provider->GetMaterial(0), Scope.ArtifactRoot));
		}
		const UClass* SectionClass = LoadClass<AActor>(nullptr, TEXT("/Script/MeshPartition.CompiledSection"));
		for (TActorIterator<AActor> It(Scope.World); It; ++It)
		{
			if (SectionClass == nullptr || !It->IsA(SectionClass) ||
				!ProjectWorldTerrainRuntimeRole::HasRole(**It))
			{
				continue;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("section"), SectionKey(**It));
			Record.Fields.Add(TEXT("class"), It->GetClass()->GetPathName());
			Record.Fields.Add(TEXT("tags"), ProjectWorldProjection::Tags(It->Tags));
			Record.Fields.Add(TEXT("spatial"), It->GetIsSpatiallyLoaded() ? TEXT("true") : TEXT("false"));
			Record.Fields.Add(TEXT("transform"), ProjectWorldProjection::Transform(It->GetActorTransform()));
			TArray<UE::MeshPartition::UMeshPartitionStaticMeshComponent*> MeshComponents;
			It->GetComponents(MeshComponents);
			for (int32 Index = 0; Index < MeshComponents.Num(); ++Index)
			{
				const UStaticMeshComponent* Component = MeshComponents[Index];
				if (Component == nullptr || Component->GetStaticMesh() == nullptr)
				{
					continue;
				}
				const FString Prefix = FString::Printf(TEXT("mesh.%d."), Index);
				ProjectWorldProjection::AddStaticMeshComponent(Record, Prefix, *Component, Scope.ArtifactRoot);
				Record.Fields.Remove(Prefix + TEXT("mesh"));
				Record.Fields.Add(Prefix + TEXT("render"), ProjectWorldProjection::RenderLod0(*Component->GetStaticMesh()));
			}
			TArray<UE::MeshPartition::UMeshPartitionCollisionComponent*> CollisionComponents;
			It->GetComponents(CollisionComponents);
			for (int32 Index = 0; Index < CollisionComponents.Num(); ++Index)
			{
				UE::MeshPartition::UMeshPartitionCollisionComponent* Component = CollisionComponents[Index];
				FTriMeshCollisionData Data;
				if (Component == nullptr || !Component->GetPhysicsTriMeshData(&Data, true))
				{
					OutError = TEXT("Compiled Mesh Terrain section has no collision triangles.");
					return false;
				}
				TArray<int32> Indices;
				for (const FTriIndices& Triangle : Data.Indices)
				{
					Indices.Append({Triangle.v0, Triangle.v1, Triangle.v2});
				}
				Record.Fields.Add(FString::Printf(TEXT("collision.%d"), Index),
					ProjectWorldProjection::TriMesh(Data.Vertices, Indices));
			}
			if (const FArrayProperty* ChannelProperty = FindFProperty<FArrayProperty>(
				It->GetClass(), TEXT("ChannelTable")))
			{
				const void* Values = ChannelProperty->ContainerPtrToValuePtr<void>(*It);
				FScriptArrayHelper Array(ChannelProperty, Values);
				FString ChannelTable;
				for (int32 Index = 0; Index < Array.Num(); ++Index)
				{
					const uint8 Value = *Array.GetRawPtr(Index);
					ChannelTable += FString::Printf(TEXT("%u,"), Value);
				}
				Record.Fields.Add(TEXT("channel_table"), ProjectWorldProjection::HashText(ChannelTable));
			}
			const FObjectPropertyBase* TextureProperty = FindFProperty<FObjectPropertyBase>(
				It->GetClass(), TEXT("ChannelTexture"));
			UTexture* ChannelTexture = TextureProperty != nullptr ? Cast<UTexture>(
				TextureProperty->GetObjectPropertyValue_InContainer(*It)) : nullptr;
			FString ChannelDigest;
			if (ChannelTexture == nullptr || !TextureDigest(*ChannelTexture, ChannelDigest, OutError))
			{
				if (OutError.IsEmpty()) { OutError = TEXT("Compiled Mesh Terrain section has no channel texture."); }
				return false;
			}
			Record.Fields.Add(TEXT("channel_texture_source"), ChannelDigest);
			if (const FStructProperty* TexcoordProperty = FindFProperty<FStructProperty>(
				It->GetClass(), TEXT("ChannelTexcoordDesc")))
			{
				const FVector2f* Texcoord = TexcoordProperty->ContainerPtrToValuePtr<FVector2f>(*It);
				Record.Fields.Add(TEXT("channel_texcoord"), FString::Printf(TEXT("%.9g,%.9g"),
					Texcoord->X, Texcoord->Y));
			}
			if (const FObjectPropertyBase* MaterialProperty = FindFProperty<FObjectPropertyBase>(
				It->GetClass(), TEXT("MaterialInstance")))
			{
				const UMaterialInstance* Material = Cast<UMaterialInstance>(
					MaterialProperty->GetObjectPropertyValue_InContainer(*It));
				Record.Fields.Add(TEXT("material_parent"), ProjectWorldProjection::ObjectPath(
					Material != nullptr ? Material->Parent : nullptr, Scope.ArtifactRoot));
				Record.Fields.Add(TEXT("material_parameters"), Material != nullptr
					? MaterialParameters(*Material, ChannelTexture, Scope.ArtifactRoot) : TEXT("none"));
			}
		}
		if (Scope.CanonicalBundle != nullptr)
		{
			FProjectWorldMeshTerrainLayoutReceipt Receipt;
			if (!FProjectWorldMeshTerrainLayoutReceiptContract::Build(
				*Scope.CanonicalBundle, Receipt, OutError))
			{
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("receipt"), TEXT("layout"));
			Record.Fields.Add(TEXT("adapter"), TEXT("project_mesh_terrain:v1"));
			Record.Fields.Add(TEXT("layout_payload"),
				FProjectWorldMeshTerrainLayoutReceiptContract::GetLayoutPayload());
			Record.Fields.Add(TEXT("layout_sha256"), Receipt.LayoutSha256);
			Record.Fields.Add(TEXT("surface_id"), Receipt.CanonicalSurfaceContractId);
			Record.Fields.Add(TEXT("surface_version"),
				FString::FromInt(Receipt.CanonicalSurfaceContractVersion));
			Record.Fields.Add(TEXT("surface_sha256"), Receipt.CanonicalSurfaceContractSha256);
			Record.Fields.Add(TEXT("shared_definition"),
				ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
			Record.Fields.Add(TEXT("shared_definition_sha256"), Receipt.SharedDefinitionPackageSha256);
			Record.Fields.Add(TEXT("build_policy_sha256"), Receipt.BuildPolicySha256);
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_mesh_terrain"), 1, 2, &Project);
}
