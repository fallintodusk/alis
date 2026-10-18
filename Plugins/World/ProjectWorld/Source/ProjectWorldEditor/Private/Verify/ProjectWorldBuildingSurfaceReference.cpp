// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldBuildingSurfaceReference.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"

namespace
{
	TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
	{
		return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y),
			MakeShared<FJsonValueNumber>(Value.Z)};
	}

	struct FSurface
	{
		int32 Triangle = INDEX_NONE;
		double Area = 0.0;
		FVector LocalTarget = FVector::ZeroVector;
		FVector LocalNormal = FVector::ZeroVector;
	};

	bool Sample(UStaticMeshComponent& Component, TArray<TSharedPtr<FJsonValue>>& Samples, FString& OutError)
	{
		UStaticMesh* Mesh = Component.GetStaticMesh();
		const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
		const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
		if (!Description || !Body || !Body->BodySetupGuid.IsValid() || !Component.IsPhysicsStateCreated() ||
			!Component.IsCollisionEnabled() || Component.GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			OutError = TEXT("Source building mesh/physics/collision identity is unavailable.");
			return false;
		}
		const FStaticMeshConstAttributes Attributes(*Description);
		const auto Positions = Attributes.GetVertexPositions();
		FSurface Surfaces[2];
		const int32 Stride = FMath::Max(1, Description->Triangles().Num() / 1024);
		int32 Ordinal = 0;
		for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
		{
			if (Ordinal++ % Stride != 0) continue;
			const auto Vertices = Description->GetTriangleVertices(Triangle);
			const FVector A(Positions[Vertices[0]]), B(Positions[Vertices[1]]), C(Positions[Vertices[2]]);
			const FVector Cross = FVector::CrossProduct(C - A, B - A);
			const double Area = Cross.Length();
			if (Area < 100.0) continue;
			const FVector Normal = Cross.GetSafeNormal();
			const int32 Kind = Normal.Z > 0.99 ? 1 : FMath::Abs(Normal.Z) < 0.01 ? 0 : INDEX_NONE;
			if (Kind != INDEX_NONE && Area > Surfaces[Kind].Area)
			{
				Surfaces[Kind] = {Triangle.GetValue(), Area, (A + B + C) / 3.0, Normal};
			}
		}
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			const FSurface& Surface = Surfaces[Kind];
			const FTransform Transform = Component.GetComponentTransform();
			const FVector Target = Transform.TransformPosition(Surface.LocalTarget);
			const FVector Normal = Transform.TransformVectorNoScale(Surface.LocalNormal).GetSafeNormal();
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectWorldBuildingSourceReference), true);
			const bool bTraceHit = Surface.Triangle != INDEX_NONE && Component.LineTraceComponent(
				Hit, Target + Normal * 100.0, Target - Normal * 100.0, Params);
			if (!bTraceHit || Hit.GetComponent() != &Component || Hit.ImpactPoint.ContainsNaN())
			{
				const FBodyInstance* Instance = Component.GetBodyInstance();
				OutError = FString::Printf(
					TEXT("A named source wall/roof ray has no current building physics reference hit: ")
					TEXT("mesh=%s component=%s kind=%s triangle=%d target_cm=(%.17g,%.17g,%.17g) ")
					TEXT("normal=(%.17g,%.17g,%.17g) registered=%d physics_state=%d body_valid=%d ")
					TEXT("collision_enabled=%d pawn_response=%d collision_trace_flag=%d trimeshes=%d ")
					TEXT("trace_complex=%d trace_hit=%d blocking_hit=%d start_penetrating=%d face_index=%d ")
					TEXT("impact_cm=(%.17g,%.17g,%.17g)."),
					*Mesh->GetPathName(), *Component.GetPathName(), Kind == 0 ? TEXT("wall") : TEXT("roof"),
					Surface.Triangle, Target.X, Target.Y, Target.Z, Normal.X, Normal.Y, Normal.Z,
					Component.IsRegistered(), Component.IsPhysicsStateCreated(), Instance && Instance->IsValidBodyInstance(),
					static_cast<int32>(Component.GetCollisionEnabled()), static_cast<int32>(Component.GetCollisionResponseToChannel(ECC_Pawn)),
					static_cast<int32>(Body->CollisionTraceFlag), Body->TriMeshGeometries.Num(), Params.bTraceComplex,
					bTraceHit, Hit.bBlockingHit, Hit.bStartPenetrating, Hit.FaceIndex,
					Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
				return false;
			}
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("kind"), Kind == 0 ? TEXT("wall") : TEXT("roof"));
			Row->SetStringField(TEXT("mesh"), Mesh->GetPathName());
			Row->SetStringField(TEXT("body_setup_guid"), Body->BodySetupGuid.ToString());
			Row->SetStringField(TEXT("component_transform"), Transform.ToString());
			Row->SetNumberField(TEXT("source_triangle"), Surface.Triangle);
			Row->SetArrayField(TEXT("source_local_target_cm"), VectorJson(Surface.LocalTarget));
			Row->SetArrayField(TEXT("source_local_normal"), VectorJson(Surface.LocalNormal));
			Row->SetArrayField(TEXT("source_target_cm"), VectorJson(Target));
			Row->SetArrayField(TEXT("normal"), VectorJson(Normal));
			Row->SetArrayField(TEXT("reference_hit_cm"), VectorJson(Hit.ImpactPoint));
			Row->SetNumberField(TEXT("source_error_cm"), FVector::Distance(Hit.ImpactPoint, Target));
			Samples.Add(MakeShared<FJsonValueObject>(Row));
		}
		return true;
	}
}

bool ProjectWorldBuildingSurfaceReference::Write(UWorld& World, const FString& Map,
	const FString& RuntimeHash, const FString& ResultPath, FString& OutError)
{
	TArray<AStaticMeshActor*> Actors;
	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		if (It->Tags.Contains(TEXT("ProjectWorld.BuildingMassing.v2"))) Actors.Add(*It);
	}
	Actors.Sort([](const AStaticMeshActor& A, const AStaticMeshActor& B)
	{
		const double ASize = A.GetActorLocation().SizeSquared2D(), BSize = B.GetActorLocation().SizeSquared2D();
		return ASize == BSize ? A.GetPathName() < B.GetPathName() : ASize < BSize;
	});
	if (Actors.Num() < 3 || RuntimeHash.Len() != 64)
	{
		OutError = TEXT("Building surface reference needs three generated source meshes and a runtime hash.");
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Samples;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Sample(*Actors[Index]->GetStaticMeshComponent(), Samples, OutError)) return false;
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("project-world-building-surface-reference:v1"));
	Root->SetStringField(TEXT("status"), TEXT("accepted"));
	Root->SetStringField(TEXT("map_package"), Map);
	Root->SetStringField(TEXT("runtime_profile_sha256"), RuntimeHash);
	Root->SetArrayField(TEXT("samples"), Samples);
	FString Json;
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)) &&
		FFileHelper::SaveStringToFile(Json, *ResultPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
