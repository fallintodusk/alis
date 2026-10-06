// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldVerifyInternal.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "Utilities/ProjectSha256.h"
#include "WorldPartition/HLOD/HLODLayer.h"

namespace ProjectWorldProjection::ProjectionDetail
{
	class FDigest
	{
	public:
		void Line(const FString& Text)
		{
			const FTCHARToUTF8 Utf8(*Text);
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			Bytes.Add(static_cast<uint8>('\n'));
		}

		FString Finish() const
		{
			FString Hash;
			FProjectSha256::HashBuffer(Bytes, Hash);
			return Hash;
		}

	private:
		TArray<uint8> Bytes;
	};

	FString Quantized(double Value, double Scale)
	{
		return FMath::IsFinite(Value)
			? FString::Printf(TEXT("%lld"), FMath::RoundToInt64(Value * Scale))
			: FString(TEXT("nan"));
	}

	FString Triple(double X, double Y, double Z, double Scale)
	{
		return Quantized(X, Scale) + TEXT(",") + Quantized(Y, Scale) + TEXT(",") + Quantized(Z, Scale);
	}

	FString Flag(bool bValue)
	{
		return bValue ? TEXT("1") : TEXT("0");
	}

	// Positions are centimetres; contract v1 keeps 0.1 mm.
	constexpr double PositionScale = 100.0;
	constexpr double UnitScale = 1.0e4;
	constexpr double QuaternionScale = 1.0e6;
	constexpr double ScaleScale = 1.0e5;
	constexpr double UvScale = 1.0e5;
	constexpr double ColorScale = 1.0e4;
}

FProjectWorldProjectionRecord& FProjectWorldProjection::Add(const FString& Kind, const FString& Key)
{
	for (const FProjectWorldProjectionRecord& Existing : Records)
	{
		if (Existing.Kind == Kind && Existing.Key == Key)
		{
			Errors.Add(FString::Printf(TEXT("Duplicate projection record: %s:%s"), *Kind, *Key));
			break;
		}
	}
	FProjectWorldProjectionRecord* Record = new FProjectWorldProjectionRecord();
	Record->Kind = Kind;
	Record->Key = Key;
	Records.Add(Record);
	return *Record;
}

TArray<const FProjectWorldProjectionRecord*> FProjectWorldProjection::Sorted() const
{
	TArray<const FProjectWorldProjectionRecord*> Result;
	for (const FProjectWorldProjectionRecord& Record : Records)
	{
		Result.Add(&Record);
	}
	Result.Sort([](const FProjectWorldProjectionRecord& Left, const FProjectWorldProjectionRecord& Right)
	{
		const int32 KindOrder = Left.Kind.Compare(Right.Kind, ESearchCase::CaseSensitive);
		return KindOrder != 0 ? KindOrder < 0 : ProjectWorldVerifyJson::Less(Left.Key, Right.Key);
	});
	return Result;
}

bool FProjectWorldProjection::IsValid(FString& OutError) const
{
	if (!Errors.IsEmpty())
	{
		OutError = FString::Join(Errors, TEXT("; "));
		return false;
	}
	return true;
}

FString FProjectWorldProjection::ToCanonicalJson() const
{
	using namespace ProjectWorldVerifyJson;
	FString Result(TEXT("["));
	bool bFirstRecord = true;
	for (const FProjectWorldProjectionRecord* Record : Sorted())
	{
		Result += bFirstRecord ? TEXT("{") : TEXT(",{");
		bFirstRecord = false;
		Result += TEXT("\"kind\":") + Quote(Record->Kind) + TEXT(",\"key\":") + Quote(Record->Key) + TEXT(",\"fields\":{");
		bool bFirstField = true;
		for (const FString& Name : SortedKeys(Record->Fields))
		{
			Result += (bFirstField ? TEXT("") : TEXT(",")) + Quote(Name) + TEXT(":") + Quote(Record->Fields.FindChecked(Name));
			bFirstField = false;
		}
		Result += TEXT("}}");
	}
	Result += TEXT("]");
	return Result;
}

FString FProjectWorldProjection::Sha256() const
{
	return ProjectWorldProjection::HashText(ToCanonicalJson());
}

namespace ProjectWorldProjection
{
	FString HashText(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		FString Hash;
		FProjectSha256::HashBuffer(Bytes, Hash);
		return Hash;
	}

	FString Transform(const FTransform& Value)
	{
		FQuat Rotation = Value.GetRotation().GetNormalized();
		// q and -q are one rotation; keep the representative with w >= 0.
		if (Rotation.W < 0.0 || (Rotation.W == 0.0 &&
			(Rotation.X < 0.0 || (Rotation.X == 0.0 && (Rotation.Y < 0.0 || (Rotation.Y == 0.0 && Rotation.Z < 0.0))))))
		{
			Rotation = FQuat(-Rotation.X, -Rotation.Y, -Rotation.Z, -Rotation.W);
		}
		const FVector Translation = Value.GetTranslation();
		const FVector Scale = Value.GetScale3D();
		return TEXT("t=") + ProjectionDetail::Triple(Translation.X, Translation.Y, Translation.Z, ProjectionDetail::PositionScale) +
			TEXT(";r=") + ProjectionDetail::Triple(Rotation.X, Rotation.Y, Rotation.Z, ProjectionDetail::QuaternionScale) + TEXT(",") +
			ProjectionDetail::Quantized(Rotation.W, ProjectionDetail::QuaternionScale) +
			TEXT(";s=") + ProjectionDetail::Triple(Scale.X, Scale.Y, Scale.Z, ProjectionDetail::ScaleScale);
	}

	FString Tags(const TArray<FName>& ActorTags, TConstArrayView<FString> ExcludedPrefixes)
	{
		TArray<FString> Values;
		for (const FName& Tag : ActorTags)
		{
			const FString Text = Tag.ToString();
			if (!ExcludedPrefixes.ContainsByPredicate([&Text](const FString& Prefix)
			{
				return Text.StartsWith(Prefix, ESearchCase::CaseSensitive);
			}))
			{
				Values.Add(Text);
			}
		}
		Values.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		return FString::Join(Values, TEXT(","));
	}

	FString RootRelative(const FString& PackageName, const FString& ArtifactRoot)
	{
		return !ArtifactRoot.IsEmpty() && PackageName.StartsWith(ArtifactRoot, ESearchCase::CaseSensitive)
			? PackageName.RightChop(ArtifactRoot.Len())
			: PackageName;
	}

	FString ObjectPath(const UObject* Object, const FString& ArtifactRoot)
	{
		return Object != nullptr ? RootRelative(Object->GetPathName(), ArtifactRoot) : FString(TEXT("none"));
	}

	FString EnumName(const UEnum* Enum, int64 Value)
	{
		return Enum != nullptr ? Enum->GetNameStringByValue(Value) : FString::Printf(TEXT("%lld"), Value);
	}

	FString MeshDescription(const UStaticMesh& Mesh)
	{
		if (Mesh.GetNumSourceModels() == 0 || !Mesh.IsMeshDescriptionValid(0))
		{
			return TEXT("none");
		}
		const FMeshDescription* Description = Mesh.GetMeshDescription(0);
		if (Description == nullptr)
		{
			return TEXT("none");
		}
		FStaticMeshConstAttributes Attributes(*Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		const TVertexInstanceAttributesConstRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		const TVertexInstanceAttributesConstRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		const TVertexInstanceAttributesConstRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
		const TVertexInstanceAttributesConstRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		const TVertexInstanceAttributesConstRef<FVector2f> Uvs = Attributes.GetVertexInstanceUVs();
		const TPolygonGroupAttributesConstRef<FName> Slots = Attributes.GetPolygonGroupMaterialSlotNames();
		ProjectionDetail::FDigest Digest;
		Digest.Line(FString::Printf(TEXT("vertices=%d"), Description->Vertices().Num()));
		for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
		{
			const FVector3f Position = Positions.IsValid() ? Positions[Vertex] : FVector3f::ZeroVector;
			Digest.Line(FString::Printf(TEXT("v%d=%s"), Vertex.GetValue(),
				*ProjectionDetail::Triple(Position.X, Position.Y, Position.Z, ProjectionDetail::PositionScale)));
		}
		const int32 UvChannels = Uvs.IsValid() ? Uvs.GetNumChannels() : 0;
		Digest.Line(FString::Printf(TEXT("instances=%d uv_channels=%d"), Description->VertexInstances().Num(), UvChannels));
		for (const FVertexInstanceID Instance : Description->VertexInstances().GetElementIDs())
		{
			FString Line = FString::Printf(TEXT("i%d=%d"), Instance.GetValue(),
				Description->GetVertexInstanceVertex(Instance).GetValue());
			if (Normals.IsValid())
			{
				const FVector3f Normal = Normals[Instance];
				Line += TEXT(";n=") + ProjectionDetail::Triple(Normal.X, Normal.Y, Normal.Z, ProjectionDetail::UnitScale);
			}
			if (Tangents.IsValid())
			{
				const FVector3f Tangent = Tangents[Instance];
				Line += TEXT(";t=") + ProjectionDetail::Triple(Tangent.X, Tangent.Y, Tangent.Z, ProjectionDetail::UnitScale);
			}
			if (Signs.IsValid())
			{
				Line += TEXT(";b=") + ProjectionDetail::Quantized(Signs[Instance], 1.0);
			}
			if (Colors.IsValid())
			{
				const FVector4f Color = Colors[Instance];
				Line += TEXT(";c=") + ProjectionDetail::Triple(Color.X, Color.Y, Color.Z, ProjectionDetail::ColorScale) + TEXT(",") + ProjectionDetail::Quantized(Color.W, ProjectionDetail::ColorScale);
			}
			for (int32 Channel = 0; Channel < UvChannels; ++Channel)
			{
				const FVector2f Uv = Uvs.Get(Instance, Channel);
				Line += FString::Printf(TEXT(";uv%d=%s,%s"), Channel, *ProjectionDetail::Quantized(Uv.X, ProjectionDetail::UvScale), *ProjectionDetail::Quantized(Uv.Y, ProjectionDetail::UvScale));
			}
			Digest.Line(Line);
		}
		Digest.Line(FString::Printf(TEXT("groups=%d"), Description->PolygonGroups().Num()));
		for (const FPolygonGroupID Group : Description->PolygonGroups().GetElementIDs())
		{
			Digest.Line(FString::Printf(TEXT("g%d=%s"), Group.GetValue(),
				Slots.IsValid() ? *Slots[Group].ToString() : TEXT("")));
		}
		Digest.Line(FString::Printf(TEXT("polygons=%d"), Description->Polygons().Num()));
		for (const FPolygonID Polygon : Description->Polygons().GetElementIDs())
		{
			FString Line = FString::Printf(TEXT("p%d=%d:"), Polygon.GetValue(),
				Description->GetPolygonPolygonGroup(Polygon).GetValue());
			for (const FVertexInstanceID Instance : Description->GetPolygonVertexInstances(Polygon))
			{
				Line += FString::Printf(TEXT("%d,"), Instance.GetValue());
			}
			Digest.Line(Line);
		}
		return Digest.Finish();
	}

	FString RenderLod0(const UStaticMesh& Mesh)
	{
		if (Mesh.IsCompiling())
		{
			UStaticMesh* Compiling = const_cast<UStaticMesh*>(&Mesh);
			FStaticMeshCompilingManager::Get().FinishCompilation(MakeArrayView(&Compiling, 1));
		}
		const FStaticMeshRenderData* RenderData = Mesh.GetRenderData();
		if (RenderData == nullptr || RenderData->LODResources.IsEmpty())
		{
			return TEXT("none");
		}
		const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
		const FPositionVertexBuffer& Positions = Lod.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& Vertices = Lod.VertexBuffers.StaticMeshVertexBuffer;
		const FColorVertexBuffer& Colors = Lod.VertexBuffers.ColorVertexBuffer;
		ProjectionDetail::FDigest Digest;
		const uint32 VertexCount = Positions.GetNumVertices();
		const uint32 UvChannels = Vertices.GetNumVertices() == VertexCount ? Vertices.GetNumTexCoords() : 0;
		Digest.Line(FString::Printf(TEXT("vertices=%u tangent_vertices=%u uv_channels=%u colors=%u"),
			VertexCount, Vertices.GetNumVertices(), UvChannels, Colors.GetNumVertices()));
		for (uint32 Index = 0; Index < VertexCount; ++Index)
		{
			const FVector3f Position = Positions.VertexPosition(Index);
			FString Line = TEXT("p=") + ProjectionDetail::Triple(Position.X, Position.Y, Position.Z, ProjectionDetail::PositionScale);
			if (Vertices.GetNumVertices() == VertexCount)
			{
				const FVector4f TangentX = Vertices.VertexTangentX(Index);
				const FVector4f TangentZ = Vertices.VertexTangentZ(Index);
				Line += TEXT(";x=") + ProjectionDetail::Triple(TangentX.X, TangentX.Y, TangentX.Z, ProjectionDetail::UnitScale);
				Line += TEXT(";z=") + ProjectionDetail::Triple(TangentZ.X, TangentZ.Y, TangentZ.Z, ProjectionDetail::UnitScale) + TEXT(",") +
					ProjectionDetail::Quantized(TangentZ.W, 1.0);
			}
			for (uint32 Channel = 0; Channel < UvChannels; ++Channel)
			{
				const FVector2f Uv = Vertices.GetVertexUV(Index, Channel);
				Line += FString::Printf(TEXT(";uv%u=%s,%s"), Channel, *ProjectionDetail::Quantized(Uv.X, ProjectionDetail::UvScale), *ProjectionDetail::Quantized(Uv.Y, ProjectionDetail::UvScale));
			}
			if (Colors.GetNumVertices() == VertexCount)
			{
				Line += FString::Printf(TEXT(";c=%08x"), Colors.VertexColor(Index).DWColor());
			}
			Digest.Line(Line);
		}
		const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
		Digest.Line(FString::Printf(TEXT("indices=%d view=%d"), Lod.IndexBuffer.GetNumIndices(), Indices.Num()));
		FString IndexLine;
		for (int32 Index = 0; Index < Indices.Num(); ++Index)
		{
			IndexLine += FString::Printf(TEXT("%u,"), Indices[Index]);
			if ((Index + 1) % 96 == 0)
			{
				Digest.Line(IndexLine);
				IndexLine.Reset();
			}
		}
		Digest.Line(IndexLine);
		for (const FStaticMeshSection& Section : Lod.Sections)
		{
			Digest.Line(FString::Printf(TEXT("section=%d,%u,%u,%u,%u,%d,%d"), Section.MaterialIndex,
				Section.FirstIndex, Section.NumTriangles, Section.MinVertexIndex, Section.MaxVertexIndex,
				Section.bEnableCollision ? 1 : 0, Section.bCastShadow ? 1 : 0));
		}
		return Digest.Finish();
	}

	FString Instances(const UInstancedStaticMeshComponent& Component)
	{
		TArray<FString> Lines;
		for (int32 Index = 0; Index < Component.GetInstanceCount(); ++Index)
		{
			FTransform Instance;
			if (Component.GetInstanceTransform(Index, Instance, false))
			{
				Lines.Add(Transform(Instance));
			}
		}
		Lines.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		ProjectionDetail::FDigest Digest;
		Digest.Line(FString::Printf(TEXT("instances=%d"), Lines.Num()));
		for (const FString& Line : Lines)
		{
			Digest.Line(Line);
		}
		return Digest.Finish();
	}

	FString TriMesh(const TArray<FVector3f>& Vertices, const TArray<int32>& Indices)
	{
		ProjectionDetail::FDigest Digest;
		Digest.Line(FString::Printf(TEXT("vertices=%d indices=%d"), Vertices.Num(), Indices.Num()));
		for (const FVector3f& Vertex : Vertices)
		{
			Digest.Line(ProjectionDetail::Triple(Vertex.X, Vertex.Y, Vertex.Z, ProjectionDetail::PositionScale));
		}
		FString IndexLine;
		for (int32 Index = 0; Index < Indices.Num(); ++Index)
		{
			IndexLine += FString::Printf(TEXT("%d,"), Indices[Index]);
			if ((Index + 1) % 96 == 0)
			{
				Digest.Line(IndexLine);
				IndexLine.Reset();
			}
		}
		Digest.Line(IndexLine);
		return Digest.Finish();
	}

	void AddActorCommon(FProjectWorldProjectionRecord& Record, const AActor& Actor)
	{
		Record.Fields.Add(TEXT("class"), Actor.GetClass()->GetPathName());
		Record.Fields.Add(TEXT("name"), Actor.GetFName().ToString());
		Record.Fields.Add(TEXT("guid"), Actor.GetActorGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
		Record.Fields.Add(TEXT("label"), Actor.GetActorLabel(false));
		Record.Fields.Add(TEXT("tags"), Tags(Actor.Tags));
		Record.Fields.Add(TEXT("transform"), Transform(Actor.GetActorTransform()));
		Record.Fields.Add(TEXT("spatially_loaded"), ProjectionDetail::Flag(Actor.GetIsSpatiallyLoaded()));
		Record.Fields.Add(TEXT("auto_lod"), ProjectionDetail::Flag(Actor.bEnableAutoLODGeneration));
		Record.Fields.Add(TEXT("hlod_layer"), ObjectPath(Actor.GetHLODLayer(), FString()));
		Record.Fields.Add(TEXT("external_package"), ProjectionDetail::Flag(Actor.IsPackageExternal()));
	}

	void AddStaticMeshComponent(
		FProjectWorldProjectionRecord& Record,
		const FString& Prefix,
		const UStaticMeshComponent& Component,
		const FString& ArtifactRoot)
	{
		TArray<FString> Overrides;
		for (const TObjectPtr<UMaterialInterface>& Material : Component.OverrideMaterials)
		{
			Overrides.Add(ObjectPath(Material, ArtifactRoot));
		}
		Record.Fields.Add(Prefix + TEXT("class"), Component.GetClass()->GetPathName());
		Record.Fields.Add(Prefix + TEXT("mesh"), ObjectPath(Component.GetStaticMesh(), ArtifactRoot));
		Record.Fields.Add(Prefix + TEXT("mobility"), EnumName(StaticEnum<EComponentMobility::Type>(), Component.Mobility));
		Record.Fields.Add(Prefix + TEXT("collision_profile"), Component.GetCollisionProfileName().ToString());
		Record.Fields.Add(Prefix + TEXT("collision_enabled"),
			EnumName(StaticEnum<ECollisionEnabled::Type>(), Component.GetCollisionEnabled()));
		Record.Fields.Add(Prefix + TEXT("affects_navigation"), ProjectionDetail::Flag(Component.CanEverAffectNavigation()));
		Record.Fields.Add(Prefix + TEXT("overlap_events"), ProjectionDetail::Flag(Component.GetGenerateOverlapEvents()));
		Record.Fields.Add(Prefix + TEXT("relative_transform"), Transform(Component.GetRelativeTransform()));
		Record.Fields.Add(Prefix + TEXT("override_materials"), FString::Join(Overrides, TEXT(",")));
	}

	void AddBodySetup(FProjectWorldProjectionRecord& Record, const UBodySetup* BodySetup)
	{
		if (BodySetup == nullptr)
		{
			Record.Fields.Add(TEXT("body"), TEXT("none"));
			return;
		}
		Record.Fields.Add(TEXT("body"), TEXT("present"));
		// The flag's reflected enum lives in PhysicsCore, which this module does not link.
		const TCHAR* Trace = TEXT("Unknown");
		switch (BodySetup->CollisionTraceFlag)
		{
		case CTF_UseDefault: Trace = TEXT("CTF_UseDefault"); break;
		case CTF_UseSimpleAndComplex: Trace = TEXT("CTF_UseSimpleAndComplex"); break;
		case CTF_UseSimpleAsComplex: Trace = TEXT("CTF_UseSimpleAsComplex"); break;
		case CTF_UseComplexAsSimple: Trace = TEXT("CTF_UseComplexAsSimple"); break;
		default: break;
		}
		Record.Fields.Add(TEXT("body_trace"), Trace);
		Record.Fields.Add(TEXT("body_double_sided"), ProjectionDetail::Flag(BodySetup->bDoubleSidedGeometry));
		Record.Fields.Add(TEXT("body_simple_elements"), FString::FromInt(BodySetup->AggGeom.GetElementCount()));
	}

	void AddStaticMesh(
		FProjectWorldProjectionRecord& Record,
		const UStaticMesh& Mesh,
		const FString& ArtifactRoot)
	{
		TArray<FString> Materials;
		for (const FStaticMaterial& Material : Mesh.GetStaticMaterials())
		{
			Materials.Add(Material.MaterialSlotName.ToString() + TEXT("=") + ObjectPath(Material.MaterialInterface, ArtifactRoot));
		}
		Record.Fields.Add(TEXT("mesh_description"), MeshDescription(Mesh));
		Record.Fields.Add(TEXT("render_lod0"), RenderLod0(Mesh));
		Record.Fields.Add(TEXT("static_materials"), FString::Join(Materials, TEXT(",")));
		Record.Fields.Add(TEXT("nanite"), ProjectionDetail::Flag(Mesh.GetNaniteSettings().bEnabled));
		Record.Fields.Add(TEXT("distance_field"), ProjectionDetail::Flag(Mesh.bGenerateMeshDistanceField));
		Record.Fields.Add(TEXT("navigation_data"), ProjectionDetail::Flag(Mesh.bHasNavigationData));
		Record.Fields.Add(TEXT("source_models"), FString::FromInt(Mesh.GetNumSourceModels()));
		Record.Fields.Add(TEXT("distance_field_resolution_scale"), Mesh.GetNumSourceModels() > 0
			? FString::Printf(TEXT("%.9g"), Mesh.GetSourceModel(0).BuildSettings.DistanceFieldResolutionScale)
			: FString(TEXT("none")));
		AddBodySetup(Record, Mesh.GetBodySetup());
	}
}

namespace ProjectWorldVerifyRegistry
{
	struct FEntry
	{
		int32 ComparisonContractVersion = 0;
		FProjectWorldProjectFn Projection;
	};

	// Function-local storage: registrars run during static initialization in any order.
	TMap<FString, FEntry>& Entries()
	{
		static TMap<FString, FEntry> Storage;
		return Storage;
	}

	FString Key(const FString& GeneratorId, int32 GeneratorVersion)
	{
		return FString::Printf(TEXT("%s:v%d"), *GeneratorId, GeneratorVersion);
	}
}

namespace ProjectWorldVerify
{
	void RegisterProjection(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		int32 ComparisonContractVersion,
		FProjectWorldProjectFn Projection)
	{
		const FString Key = ProjectWorldVerifyRegistry::Key(GeneratorId, GeneratorVersion);
		check(!ProjectWorldVerifyRegistry::Entries().Contains(Key));
		ProjectWorldVerifyRegistry::Entries().Add(Key, {ComparisonContractVersion, MoveTemp(Projection)});
	}

	bool FindProjection(
		const FString& GeneratorId,
		int32 GeneratorVersion,
		int32& OutComparisonContractVersion,
		FProjectWorldProjectFn& OutProjection)
	{
		const ProjectWorldVerifyRegistry::FEntry* Entry =
			ProjectWorldVerifyRegistry::Entries().Find(ProjectWorldVerifyRegistry::Key(GeneratorId, GeneratorVersion));
		if (Entry == nullptr)
		{
			return false;
		}
		OutComparisonContractVersion = Entry->ComparisonContractVersion;
		OutProjection = Entry->Projection;
		return true;
	}

	void ForEachProjection(TFunctionRef<void(const FString&, int32, int32, const FProjectWorldProjectFn&)> Visitor)
	{
		TArray<FString> Keys;
		ProjectWorldVerifyRegistry::Entries().GetKeys(Keys);
		Keys.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		for (const FString& Key : Keys)
		{
			FString GeneratorId;
			FString Version;
			Key.Split(TEXT(":v"), &GeneratorId, &Version, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			const ProjectWorldVerifyRegistry::FEntry& Entry = ProjectWorldVerifyRegistry::Entries().FindChecked(Key);
			Visitor(GeneratorId, FCString::Atoi(*Version), Entry.ComparisonContractVersion, Entry.Projection);
		}
	}
}

FProjectWorldVerifyProjectionRegistrar::FProjectWorldVerifyProjectionRegistrar(
	const TCHAR* GeneratorId,
	int32 GeneratorVersion,
	int32 ComparisonContractVersion,
	FProjectWorldProjectFn Projection)
{
	ProjectWorldVerify::RegisterProjection(GeneratorId, GeneratorVersion, ComparisonContractVersion, MoveTemp(Projection));
}
