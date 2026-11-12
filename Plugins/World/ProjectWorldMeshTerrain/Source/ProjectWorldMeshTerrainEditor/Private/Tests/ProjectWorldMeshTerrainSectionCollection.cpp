#include "ProjectWorldMeshTerrainSectionCollection.h"
#include "ProjectWorldMeshTerrainRegionDomain.h"
#include "ProjectWorldMeshTerrainCollisionFixture.h"
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "ProjectWorldTerrainRuntimeRole.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionCollisionComponent.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldTerrainSectionCollection
{
namespace
{
    TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (double Value : Values) Out.Add(MakeShared<FJsonValueNumber>(Value));
        return Out;
    }
    TArray<TSharedPtr<FJsonValue>> Matrix(const FTransform& Transform)
    {
        TArray<TSharedPtr<FJsonValue>> Out; const FMatrix M=Transform.ToMatrixWithScale();
        for (int32 I=0;I<4;++I) Out.Add(MakeShared<FJsonValueArray>(Numbers({M.M[I][0],M.M[I][1],M.M[I][2],M.M[I][3]})));
        return Out;
    }
}
bool Collect(FAutomationTestBase& Test,UWorld& WorldRef,const FProjectWorldCanonicalBundle& Bundle,
    TConstArrayView<UE::MeshPartition::ACompiledSection*> Sections,FName VariantName,
    bool StorageOnly,bool AllowAdjacent,FCollected& Out,TConstArrayView<FVector> RequiredPoints)
{
    Out=FCollected(); auto* World=&WorldRef;
    ProjectWorldRegionDomain::FIsolatedTerrain Isolated(Test,WorldRef,Bundle);
    if (!Isolated.IsValid()) return false;
    using ProjectWorldCompositeTerrainTests::ReadBuffer;
    ProjectWorldTerrainCollisionFixture::FInventory CollisionInventory;
    if (!CollisionInventory.Gather(Test,*World,Sections)) return false;
    auto& Actual=Out.Actual; auto& Uncompensated=Out.Uncompensated; auto& Components=Out.Components;
    auto& PerSection=Out.PerSection;
    for (auto* Section : Sections)
    {
        const int32 FirstFace=Actual.Num();
        if (!Test.TestTrue(TEXT("The project's final role transformer ran."),Section->ActorHasTag(ProjectWorldTerrainRuntimeRole::RoleTag()))) return false;
        for (const auto& Component : Section->GetMeshComponents())
        {
            UStaticMesh* Mesh=Component->GetStaticMesh();
            if (!Test.TestNotNull(TEXT("A configured visual variant has an actual built mesh."),Mesh) ||
                !Test.TestTrue(TEXT("The native mesh has render LOD data."),Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num()>0)) return false;
            const auto& LOD=Mesh->GetRenderData()->LODResources[0];
            const uint32 Count=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
            const uint32 Indices=LOD.IndexBuffer.GetNumIndices(),Stride=LOD.IndexBuffer.Is32Bit() ? 4 : 2;
            TArray<uint8> PositionBytes,IndexBytes;
            if (!Test.TestTrue(TEXT("Actual GPU buffers have positive bounded readable geometry."),Count>0 && Indices>0 && Indices%3==0 &&
                LOD.VertexBuffers.PositionVertexBuffer.GetStride()==sizeof(FVector3f) &&
                ReadBuffer(LOD.VertexBuffers.PositionVertexBuffer.VertexBufferRHI.GetReference(),Count*sizeof(FVector3f),PositionBytes) &&
                ReadBuffer(LOD.IndexBuffer.IndexBufferRHI.GetReference(),Indices*Stride,IndexBytes))) return false;
            auto ComponentRow=MakeShared<FJsonObject>(); ComponentRow->SetStringField(TEXT("kind"),TEXT("gpu_fallback"));
            ComponentRow->SetBoolField(TEXT("valid_nanite_data"),Mesh->HasValidNaniteData());
            ComponentRow->SetArrayField(TEXT("actor_transform"),Matrix(Section->GetActorTransform()));
            ComponentRow->SetArrayField(TEXT("component_transform"),Matrix(Component->GetComponentTransform()));
            TArray<FVector> Positions; TArray<TSharedPtr<FJsonValue>> Vertices,Faces;
            for (uint32 I=0;I<Count;++I)
            {
                FVector3f Local; FMemory::Memcpy(&Local,PositionBytes.GetData()+I*sizeof(Local),sizeof(Local));
                const FVector P=Component->GetComponentTransform().TransformPosition(FVector(Local)); Positions.Add(P);
                Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
            }
            for (uint32 I=0;I<Indices;I+=3)
            {
                uint32 IDs[3];
                for (uint32 J=0;J<3;++J)
                {
                    if (Stride==4) FMemory::Memcpy(&IDs[J],IndexBytes.GetData()+(I+J)*Stride,4);
                    else {uint16 Value;FMemory::Memcpy(&Value,IndexBytes.GetData()+(I+J)*Stride,2);IDs[J]=Value;}
                    if (IDs[J]>=uint32(Positions.Num())) {Test.AddError(TEXT("A GPU index escapes the actual buffer."));return false;}
                }
                FProjectWorldSurfaceTriangle Face;
                for (int32 J=0;J<3;++J) Face.Points[J]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Positions[IDs[J]]);
                FProjectWorldSurfaceTriangle WithoutFrame;
                for (int32 J=0;J<3;++J) WithoutFrame.Points[J]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,
                    Component->GetComponentTransform().InverseTransformPosition(Positions[IDs[J]]));
                Uncompensated.Add(WithoutFrame);
                Actual.Add(Face); Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(IDs[0]),double(IDs[1]),double(IDs[2])})));
            }
            ComponentRow->SetArrayField(TEXT("world_vertices_cm"),Vertices); ComponentRow->SetArrayField(TEXT("triangles"),Faces); Components.Add(MakeShared<FJsonValueObject>(ComponentRow));
            Test.TestEqual(TEXT("Only the actual nanite variant advertises Nanite data."),Mesh->HasValidNaniteData(),VariantName==TEXT("nanite"));
        }
        for (const auto& Component : Section->GetCollisionComponents())
        {
            FTriMeshCollisionData Data;
            if (!Test.TestTrue(TEXT("The collision transformer supplies provider input and an actual cooked body."),
                Component->GetPhysicsTriMeshData(&Data,true) && Data.Vertices.Num()>0 && Data.Indices.Num()>0 &&
                Component->GetBodySetup() && Component->GetBodySetup()->bCreatedPhysicsMeshes)) return false;
            auto ComponentRow=MakeShared<FJsonObject>(); ComponentRow->SetStringField(TEXT("kind"),TEXT("collision"));
            int32 CookedFaces=0;
            TArray<TSharedPtr<FJsonValue>> CookedFaceMap;
            TArray<TSharedPtr<FJsonValue>> RawCookedTriangles;
            TArray<FProjectWorldSurfaceTriangle> CookedLocal;
            for (const auto& Mesh : Component->GetBodySetup()->TriMeshGeometries)
            {
                if (!Test.TestTrue(TEXT("Cooked collision inventory contains only live native triangle meshes."),bool(Mesh))) return false;
                const int32 Count=Mesh->Elements().GetNumTriangles();
                CookedFaces+=Count;
                for (int32 Face=0;Face<Count;++Face)
                {
                    CookedFaceMap.Add(MakeShared<FJsonValueNumber>(Mesh->GetExternalFaceIndexFromInternal(Face)));
                    Chaos::FTriangle Triangle;
                    int32 A,B,C;
                    Mesh->GetTransformedTriangle(Face,Chaos::FRigidTransform3::Identity,Triangle,A,B,C);
                    FProjectWorldSurfaceTriangle Local;
                    TArray<TSharedPtr<FJsonValue>> Raw;
                    for (int32 Index=0;Index<3;++Index)
                    {
                        Local.Points[Index]=FVector(Triangle[Index]);
                        const auto& P=Local.Points[Index];
                        Raw.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
                    }
                    RawCookedTriangles.Add(MakeShared<FJsonValueArray>(Raw));
                    // Native cooking swaps the first two indices for this flag.
                    // Undo that declared convention only for canonical comparison.
                    if (Data.bFlipNormals) Swap(Local.Points[0],Local.Points[1]);
                    CookedLocal.Add(Local);
                }
            }
            ComponentRow->SetNumberField(TEXT("provider_input_faces"),Data.Indices.Num());
            ComponentRow->SetNumberField(TEXT("native_cooked_faces"),CookedFaces);
            ComponentRow->SetArrayField(TEXT("native_cooked_external_face_indices"),CookedFaceMap);
            ComponentRow->SetArrayField(TEXT("raw_cooked_triangles_local_cm"),RawCookedTriangles);
            ComponentRow->SetBoolField(TEXT("provider_flip_normals"),Data.bFlipNormals);
            ComponentRow->SetArrayField(TEXT("actor_transform"),Matrix(Section->GetActorTransform()));
            ComponentRow->SetArrayField(TEXT("component_transform"),Matrix(Component->GetComponentTransform()));
            TArray<TSharedPtr<FJsonValue>> Vertices,Faces;
            for (const auto& Local : Data.Vertices)
            {
                const FVector P=Component->GetComponentTransform().TransformPosition(FVector(Local));
                Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
            }
            for (const auto& IDs : Data.Indices)
            {
                Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(IDs.v0),double(IDs.v1),double(IDs.v2)})));
            }
            ComponentRow->SetArrayField(TEXT("provider_input_world_vertices_cm"),Vertices);
            ComponentRow->SetArrayField(TEXT("provider_input_triangles"),Faces);
            Vertices.Reset();Faces.Reset();
            for (const auto& Local : CookedLocal)
            {
                FProjectWorldSurfaceTriangle Face;
                FProjectWorldSurfaceTriangle WithoutFrame;
                const int32 First=Vertices.Num();
                for (int32 Index=0;Index<3;++Index)
                {
                    const FVector P=Component->GetComponentTransform().TransformPosition(Local.Points[Index]);
                    Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
                    Face.Points[Index]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,P);
                    WithoutFrame.Points[Index]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Local.Points[Index]);
                }
                Uncompensated.Add(WithoutFrame);
                Actual.Add(Face); Faces.Add(MakeShared<FJsonValueArray>(Numbers({double(First),double(First+1),double(First+2)})));
            }
            ComponentRow->SetArrayField(TEXT("world_vertices_cm"),Vertices); ComponentRow->SetArrayField(TEXT("triangles"),Faces); Components.Add(MakeShared<FJsonValueObject>(ComponentRow));
            int32 RayHits=0,CapsuleHits=0,WrongComponent=0,NeighborRayHits=0,NeighborCapsuleHits=0;
            double MaxRayError=0,MaxCapsuleFootOffset=0,MinNormalZ=1;
            if (!CollisionInventory.Calibrate(Test,Component.Get(),AllowAdjacent)) return false;
            ComponentRow->SetNumberField(TEXT("ownership_base_positive_controls"),1);
            ComponentRow->SetNumberField(TEXT("ownership_base_negative_controls"),3);
            TArray<TSharedPtr<FJsonValue>> CrossSectionHits;
            const auto OwnsHit=[&](const FHitResult& Hit,bool Ray)
            {
                const bool Owned=CollisionInventory.Accepts(Hit,Component.Get(),AllowAdjacent);
                if (Hit.GetComponent()==Component.Get()) return Owned;
                if (Owned) {if (Ray) ++NeighborRayHits;else ++NeighborCapsuleHits;}
                if (CrossSectionHits.Num()<8)
                {
                    auto HitRow=MakeShared<FJsonObject>(); HitRow->SetStringField(TEXT("probe"),Ray ? TEXT("ray") : TEXT("capsule"));
                    HitRow->SetStringField(TEXT("requested_component"),Component->GetPathName());
                    HitRow->SetStringField(TEXT("hit_component"),GetPathNameSafe(Hit.GetComponent()));
                    HitRow->SetBoolField(TEXT("same_declared_variant"),Owned); CrossSectionHits.Add(MakeShared<FJsonValueObject>(HitRow));
                }
                return Owned;
            };
            constexpr double Radius=34,HalfHeight=88;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ProjectWorldBasinSections),true);
            const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,HalfHeight);
            TArray<TSharedPtr<FJsonValue>> MissingRays;
            int32 ProbeFaceIndex=0;
            for (const auto& IDs : Data.Indices)
            {
                const FVector Center=Component->GetComponentTransform().TransformPosition(
                    (FVector(Data.Vertices[IDs.v0])+FVector(Data.Vertices[IDs.v1])+FVector(Data.Vertices[IDs.v2]))/3.);
                FHitResult Ray,Sweep;
                if (World->LineTraceSingleByChannel(Ray,Center+FVector(0,0,400),Center-FVector(0,0,400),ECC_Pawn,Query) && Ray.bBlockingHit)
                {
                    ++RayHits; WrongComponent+=OwnsHit(Ray,true) ? 0 : 1;
                    MaxRayError=FMath::Max(MaxRayError,FMath::Abs(Ray.ImpactPoint.Z-Center.Z));
                    MinNormalZ=FMath::Min(MinNormalZ,Ray.ImpactNormal.Z);
                }
                else
                {
                    auto Missing=MakeShared<FJsonObject>();
                    Missing->SetNumberField(TEXT("source_face_index"),ProbeFaceIndex);
                    Missing->SetArrayField(TEXT("world_center_cm"),Numbers({Center.X,Center.Y,Center.Z}));
                    MissingRays.Add(MakeShared<FJsonValueObject>(Missing));
                }
                if (World->SweepSingleByChannel(Sweep,Center+FVector(0,0,400),Center-FVector(0,0,400),FQuat::Identity,ECC_Pawn,Capsule,Query) && Sweep.bBlockingHit)
                {
                    ++CapsuleHits; WrongComponent+=OwnsHit(Sweep,false) ? 0 : 1;
                    MaxCapsuleFootOffset=FMath::Max(MaxCapsuleFootOffset,FMath::Abs(Sweep.Location.Z-HalfHeight-Center.Z));
                }
                ++ProbeFaceIndex;
            }
            ComponentRow->SetArrayField(TEXT("missing_ray_probes"),MissingRays);
            ComponentRow->SetNumberField(TEXT("physics_probe_faces"),Data.Indices.Num());
            ComponentRow->SetNumberField(TEXT("blocking_ray_hits"),RayHits); ComponentRow->SetNumberField(TEXT("blocking_capsule_hits"),CapsuleHits);
            ComponentRow->SetNumberField(TEXT("wrong_collision_components"),WrongComponent); ComponentRow->SetNumberField(TEXT("maximum_ray_error_cm"),MaxRayError);
            ComponentRow->SetNumberField(TEXT("neighbor_variant_ray_hits"),NeighborRayHits);
            ComponentRow->SetNumberField(TEXT("neighbor_variant_capsule_hits"),NeighborCapsuleHits);
            ComponentRow->SetArrayField(TEXT("cross_section_probe_hits"),CrossSectionHits);
            ComponentRow->SetNumberField(TEXT("minimum_ray_normal_z"),MinNormalZ); ComponentRow->SetNumberField(TEXT("maximum_capsule_foot_offset_cm"),MaxCapsuleFootOffset);
            ComponentRow->SetNumberField(TEXT("probe_capsule_radius_cm"),Radius); ComponentRow->SetNumberField(TEXT("probe_capsule_half_height_cm"),HalfHeight);
            Test.TestEqual(TEXT("Every actual collision face has a physical blocking ray."),RayHits,Data.Indices.Num());
            Test.TestEqual(TEXT("Every actual collision face supports the explicit generic capsule."),CapsuleHits,Data.Indices.Num());
            Test.TestEqual(TEXT("Physical queries hit only the declared variant's actual collision components."),WrongComponent,0);
            Test.TestTrue(TEXT("Physical ray heights meet the existing complete-field precision bound."),MaxRayError<=.01);
            if (StorageOnly) Test.TestTrue(TEXT("Storage-only collision normals remain finite and upward; no walkability claim."),FMath::IsFinite(MinNormalZ) && MinNormalZ>0);
            else Test.TestTrue(TEXT("Measured collision normals remain upward and below a45degree slope."),MinNormalZ>=FMath::Sqrt(.5));
            const auto& First=Data.Indices[0];
            const FVector Witness=Component->GetComponentTransform().TransformPosition(
                (FVector(Data.Vertices[First.v0])+FVector(Data.Vertices[First.v1])+FVector(Data.Vertices[First.v2]))/3.);
            TArray<ECollisionEnabled::Type> Enabled;
            for (auto* Collider : CollisionInventory.Components) Enabled.Add(Collider->GetCollisionEnabled());
            FHitResult DisabledRay,DisabledSweep,RestoredRay,RestoredSweep;
            for (auto* Collider : CollisionInventory.Components) Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            const bool DisabledRayHit=World->LineTraceSingleByChannel(DisabledRay,Witness+FVector(0,0,400),Witness-FVector(0,0,400),ECC_Pawn,Query);
            const bool DisabledSweepHit=World->SweepSingleByChannel(DisabledSweep,Witness+FVector(0,0,400),Witness-FVector(0,0,400),FQuat::Identity,ECC_Pawn,Capsule,Query);
            for (int32 I=0;I<CollisionInventory.Components.Num();++I) CollisionInventory.Components[I]->SetCollisionEnabled(Enabled[I]);
            const bool RestoredRayHit=World->LineTraceSingleByChannel(RestoredRay,Witness+FVector(0,0,400),Witness-FVector(0,0,400),ECC_Pawn,Query);
            const bool RestoredSweepHit=World->SweepSingleByChannel(RestoredSweep,Witness+FVector(0,0,400),Witness-FVector(0,0,400),FQuat::Identity,ECC_Pawn,Capsule,Query);
            ComponentRow->SetNumberField(TEXT("disabled_collision_ray_hits"),DisabledRayHit ? 1 : 0);
            ComponentRow->SetNumberField(TEXT("disabled_collision_capsule_hits"),DisabledSweepHit ? 1 : 0);
            ComponentRow->SetNumberField(TEXT("restored_collision_ray_hits"),RestoredRayHit ? 1 : 0);
            ComponentRow->SetNumberField(TEXT("restored_collision_capsule_hits"),RestoredSweepHit ? 1 : 0);
            ComponentRow->SetNumberField(TEXT("disabled_owned_collision_components"),CollisionInventory.Components.Num());
            Test.TestTrue(TEXT("Physical probes refuse the same intact geometry when its collision is disabled."),!DisabledRayHit && !DisabledSweepHit);
            Test.TestTrue(TEXT("Restoring collision restores both physical probes on the actual component."),
                RestoredRayHit && RestoredSweepHit && RestoredRay.bBlockingHit && RestoredSweep.bBlockingHit &&
                CollisionInventory.Accepts(RestoredRay,Component.Get(),AllowAdjacent) &&
                CollisionInventory.Accepts(RestoredSweep,Component.Get(),AllowAdjacent));
        }
        TArray<FProjectWorldSurfaceTriangle> SectionField;
        SectionField.Append(Actual.GetData()+FirstFace,Actual.Num()-FirstFace);
        PerSection.Add(MoveTemp(SectionField));
    }
    if (!RequiredPoints.IsEmpty())
    {
        Out.RequiredPhysics=MakeShared<FJsonObject>();
        if (VariantName==TEXT("collision")) CollisionInventory.CheckRequiredPoints(Test,Bundle,RequiredPoints,*Out.RequiredPhysics);
        else Out.RequiredPhysics->SetStringField(TEXT("status"),TEXT("not_applicable_visual_variant"));
    }
    if (!Isolated.Restore()) return false;
    Out.TerrainPhysicsIsolation=Isolated.Receipt();
    return true;
}
}
#endif
