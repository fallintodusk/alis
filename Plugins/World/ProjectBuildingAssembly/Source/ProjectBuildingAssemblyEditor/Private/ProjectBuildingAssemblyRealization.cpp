#include "ProjectBuildingAssemblyRealization.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/SecureHash.h"

namespace ProjectBuildingAssembly
{
    namespace
    {
        const TCHAR* FeaturePrefix=TEXT("Project.BuildingAssembly.Feature=");
        const FName GeneratedOwner(TEXT("Project.BuildingAssembly.Generated=v1"));
        FString Digest(const FString& Text)
        {
            FTCHARToUTF8 Bytes(*Text);uint8 Hash[20];FSHA1::HashBuffer(Bytes.Get(),Bytes.Length(),Hash);
            return FString::FromHexBlob(Hash,20).ToLower();
        }
        FTransform InstanceTransform(const FInstance& I)
        {
            return FTransform(FRotator(0,I.Yaw,0),FVector(I.X,I.Y,I.Z)*100,
                FVector(I.Width,I.Depth,I.Height));
        }
        void Box(FMeshDescription& D,const FVector3f& Center,const FVector3f& Size)
        {
            FStaticMeshAttributes A(D);auto Positions=A.GetVertexPositions();auto Normals=A.GetVertexInstanceNormals();
            auto UV=A.GetVertexInstanceUVs();const FPolygonGroupID Group(0);
            const FVector3f Corners[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
            const int Faces[][3]={{0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,5,1},{0,4,5},{1,6,2},{1,5,6},{2,7,3},{2,6,7},{3,4,0},{3,7,4}};
            FVertexID Vertices[8];
            for (int I=0;I<8;++I) {Vertices[I]=D.CreateVertex();Positions[Vertices[I]]=Center+Corners[I]*Size/2;}
            for (const auto& Face : Faces)
            {
                TArray<FVertexInstanceID> V;
                const FVector3f N=FVector3f::CrossProduct(Positions[Vertices[Face[2]]]-Positions[Vertices[Face[0]]],
                    Positions[Vertices[Face[1]]]-Positions[Vertices[Face[0]]]).GetSafeNormal();
                for (int J=0;J<3;++J) {const auto Id=D.CreateVertexInstance(Vertices[Face[J]]);Normals[Id]=N;UV.Set(Id,0,FVector2f(J==1,J==2));V.Add(Id);}
                D.CreatePolygon(Group,V);
            }
        }
        FMeshDescription Shape(EPart Part,const FKit& Kit)
        {
            FMeshDescription D;FStaticMeshAttributes A(D);A.Register();A.GetVertexInstanceUVs().SetNumChannels(1);
            const auto Group=D.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Module");
            if (Part!=EPart::Window) Box(D,FVector3f::ZeroVector,FVector3f(100));
            else
            {
                const float Frame=static_cast<float>(Kit.FrameFraction*100),At=(100-Frame)/2;
                Box(D,FVector3f(-At,0,0),FVector3f(Frame,100,100));Box(D,FVector3f(At,0,0),FVector3f(Frame,100,100));
                Box(D,FVector3f(0,0,-At),FVector3f(100-2*Frame,100,Frame));Box(D,FVector3f(0,0,At),FVector3f(100-2*Frame,100,Frame));
            }
            for (const auto E : D.Edges().GetElementIDs()) A.GetEdgeHardnesses()[E]=true;
            return D;
        }
        FString TriangleKey(const FVector3f& A,const FVector3f& B,const FVector3f& C)
        {
            const auto Point=[](const FVector3f& P){return FString::Printf(TEXT("%lld,%lld,%lld"),
                FMath::RoundToInt64(P.X*10000.),FMath::RoundToInt64(P.Y*10000.),FMath::RoundToInt64(P.Z*10000.));};
            const FString P[]={Point(A),Point(B),Point(C)};
            TArray<FString> Cycles={P[0]+TEXT("|")+P[1]+TEXT("|")+P[2],P[1]+TEXT("|")+P[2]+TEXT("|")+P[0],P[2]+TEXT("|")+P[0]+TEXT("|")+P[1]};
            Cycles.Sort();return Cycles[0];
        }
        bool MeshMatches(UStaticMesh* Mesh,EPart Part,const FKit& Kit)
        {
            if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty() ||
                !Mesh->GetBodySetup() || Mesh->GetBodySetup()->bDoubleSidedGeometry || Mesh->GetBodySetup()->CollisionTraceFlag!=CTF_UseComplexAsSimple) return false;
            auto Expected=Shape(Part,Kit);FStaticMeshAttributes A(Expected);const auto Positions=A.GetVertexPositions();
            TArray<FString> Want,Have;
            for (const auto T : Expected.Triangles().GetElementIDs())
            {
                const auto V=Expected.GetTriangleVertices(T);Want.Add(TriangleKey(Positions[V[0]],Positions[V[1]],Positions[V[2]]));
            }
            const auto& L=Mesh->GetRenderData()->LODResources[0];const auto Indices=L.IndexBuffer.GetArrayView();
            if (Indices.Num()%3 || Indices.Num()/3!=Want.Num()) return false;
            for (int I=0;I<Indices.Num();I+=3)
            {
                Have.Add(TriangleKey(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I]),
                    L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+1]),L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+2])));
                const FVector A0(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I]));
                const FVector B0(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+1]));
                const FVector C0(L.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+2]));
                const FVector Face=FVector::CrossProduct(C0-A0,B0-A0).GetSafeNormal();
                for (int J=0;J<3;++J)
                {
                    const auto N=L.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Indices[I+J]);
                    if (FVector::DotProduct(Face,FVector(N.X,N.Y,N.Z))<=0) return false;
                }
            }
            Want.Sort();Have.Sort();return Want==Have;
        }
    }
    bool ModuleMatches(UStaticMesh* Mesh,EPart Part,const FKit& Kit) {return MeshMatches(Mesh,Part,Kit);}
    UStaticMesh* CreateModule(UObject& Outer,FName Name,EPart Part,const FKit& Kit,UMaterialInterface* Material,EObjectFlags Flags)
    {
        auto* Mesh=NewObject<UStaticMesh>(&Outer,Name,Flags);
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Material));auto Description=Shape(Part,Kit);
        UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bBuildSimpleCollision=false;Params.bAllowCpuAccess=true;
        if (!Mesh->BuildFromMeshDescriptions({&Description},Params)) return nullptr;
        if (!Mesh->GetBodySetup()) Mesh->CreateBodySetup();
        Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;Mesh->GetBodySetup()->bDoubleSidedGeometry=false;
        Mesh->GetBodySetup()->CreatePhysicsMeshes();Mesh->bHasNavigationData=false;
        return Mesh;
    }
    FString GeometryIdentity(const FString& FeatureId,const FPlan& Plan,const FKit& Kit,const FTransform& Transform)
    {
        const auto T=Transform.GetTranslation();const auto Q=Transform.GetRotation();const auto S=Transform.GetScale3D();
        FString Text=TEXT("project_modular_building_v1|")+FeatureId+
            FString::Printf(TEXT("|%.17g,%.17g,%.17g|%.17g,%.17g,%.17g,%.17g|%.17g,%.17g,%.17g"),T.X,T.Y,T.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z)+
            FString::Printf(TEXT("|frame=%.17g"),Kit.FrameFraction);
        for (const auto& I : Plan.Instances) Text+=FString::Printf(TEXT("|%d:%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g"),
            static_cast<int>(I.Part),I.X,I.Y,I.Z,I.Width,I.Depth,I.Height,I.Yaw);
        for (int Part=0;Part<4;++Part)
        {
            auto D=Shape(static_cast<EPart>(Part),Kit);FStaticMeshAttributes A(D);const auto P=A.GetVertexPositions();
            for (const auto Triangle : D.Triangles().GetElementIDs())
            {const auto V=D.GetTriangleVertices(Triangle);Text+=TEXT("|")+TriangleKey(P[V[0]],P[V[1]],P[V[2]]);}
        }
        return Digest(Text);
    }
    bool Matches(const AActor& Actor,const FString& Identity,const FPlan& Plan,const FKit& Kit,
        const FTransform& Transform,UMaterialInterface* Material,TConstArrayView<UStaticMesh*> Modules)
    {
        if (!Actor.Tags.Contains(GeneratedOwner) || !Actor.GetRootComponent() ||
            !Actor.Tags.Contains(FName(*(TEXT("Project.BuildingAssembly.Geometry=")+Identity))) ||
            !Actor.GetActorTransform().Equals(Transform,1.e-8)) return false;
        TInlineComponentArray<UInstancedStaticMeshComponent*> Components(&Actor);
        if (Components.Num()!=4) return false;
        for (int Part=0;Part<4;++Part)
        {
            auto* C=Components.FindByPredicate([&](const auto* V){return V->GetFName()==FName(*FString::Printf(TEXT("Part%d"),Part));});
            if (!C || (*C)->GetAttachParent()!=Actor.GetRootComponent() || !(*C)->GetRelativeTransform().Equals(FTransform::Identity,1.e-8) ||
                (!Modules.IsEmpty() && (Modules.Num()!=4 || (*C)->GetStaticMesh()!=Modules[Part])) ||
                !(*C)->GetComponentTransform().Equals(Transform,1.e-6) ||
                (*C)->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block ||
                (*C)->GetMaterial(0)!=Material || (*C)->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics ||
                !MeshMatches((*C)->GetStaticMesh(),static_cast<EPart>(Part),Kit)) return false;
            int Index=0;
            for (const auto& I : Plan.Instances) if (static_cast<int>(I.Part)==Part)
            {FTransform Found;if (!(*C)->GetInstanceTransform(Index++,Found,false) || !Found.Equals(InstanceTransform(I),1.e-6)) return false;}
            if ((*C)->GetInstanceCount()!=Index) return false;
        }
        return true;
    }
    bool Apply(UWorld& World,const FString& FeatureId,const FPlan& Plan,const FKit& Kit,
        const FTransform& Transform,UMaterialInterface* Material,AActor*& OutActor,bool& Reused,FString& Error,const FNativeOptions& Options)
    {
        OutActor=nullptr;Reused=false;
        if (Plan.Instances.empty() || Plan.Instances.size()>65536 || Transform.ContainsNaN() || !Material)
        {Error=TEXT("Assembly requires a finite nonempty prepared plan and material.");return false;}
        if (!FMath::IsFinite(Kit.FrameFraction) || Kit.FrameFraction<.05 || Kit.FrameFraction>.4)
        {Error=TEXT("Assembly frame geometry is unsupported.");return false;}
        if (!Options.Modules.IsEmpty())
        {
            if (Options.Modules.Num()!=4) {Error=TEXT("Shared kit requires exactly four modules.");return false;}
            for (int Part=0;Part<4;++Part) if (!MeshMatches(Options.Modules[Part],static_cast<EPart>(Part),Kit))
            {Error=TEXT("Shared module differs from the admitted original kit.");return false;}
        }
        if (Options.bExternalActor && (Options.Modules.Num()!=4 || !Options.ActorGuid.IsValid() || Options.ActorName.IsNone()))
        {Error=TEXT("Persistent assembly requires shared modules and stable actor identity.");return false;}
        for (const auto& I : Plan.Instances)
            if (static_cast<int>(I.Part)<0 || static_cast<int>(I.Part)>3 ||
                !FMath::IsFinite(I.X) || !FMath::IsFinite(I.Y) || !FMath::IsFinite(I.Z) ||
                !FMath::IsFinite(I.Width) || !FMath::IsFinite(I.Depth) || !FMath::IsFinite(I.Height) ||
                I.Width<=0 || I.Depth<=0 || I.Height<=0 || (I.Yaw!=0 && I.Yaw!=90))
            {Error=TEXT("Assembly plan contains an invalid instance.");return false;}
        const FName FeatureTag(*(FString(FeaturePrefix)+FeatureId));AActor* Previous=nullptr;
        for (TActorIterator<AActor> It(&World);It;++It) if (It->Tags.Contains(FeatureTag))
        {if (Previous) {Error=TEXT("Duplicate assembly ownership.");return false;}Previous=*It;}
        if (Previous && !Previous->Tags.Contains(GeneratedOwner))
        {Error=TEXT("Feature-associated actor is not owned generated assembly.");return false;}
        const FString Identity=GeometryIdentity(FeatureId,Plan,Kit,Transform);
        if (Previous && Matches(*Previous,Identity,Plan,Kit,Transform,Material,Options.Modules)) {OutActor=Previous;Reused=true;return true;}
        FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transactional;
        if (!Previous) {Spawn.OverrideActorGuid=Options.ActorGuid;Spawn.Name=Options.ActorName;}
        auto* Actor=World.SpawnActor<AActor>(Spawn);
        if (!Actor) {Error=TEXT("Cannot stage modular actor.");return false;}
        if (!Previous && Options.bExternalActor) Actor->SetPackageExternal(true);
        auto* Root=NewObject<USceneComponent>(Actor,TEXT("AssemblyRoot"),RF_Transactional);Actor->SetRootComponent(Root);
        Actor->AddInstanceComponent(Root);Root->RegisterComponent();Actor->SetActorTransform(Transform);
        for (int Part=0;Part<4;++Part)
        {
            auto* Mesh=Options.Modules.IsEmpty() ? CreateModule(*Actor,*FString::Printf(TEXT("Module%d"),Part),static_cast<EPart>(Part),Kit,Material,RF_Transactional) : Options.Modules[Part];
            if (!Mesh) {World.DestroyActor(Actor);Error=TEXT("Cannot build original module mesh.");return false;}
            auto* C=NewObject<UInstancedStaticMeshComponent>(Actor,*FString::Printf(TEXT("Part%d"),Part),RF_Transactional);
            Actor->AddInstanceComponent(C);C->SetupAttachment(Root);C->SetStaticMesh(Mesh);C->SetMaterial(0,Material);
            C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            C->SetCollisionResponseToAllChannels(ECR_Block);
            for (const auto& I : Plan.Instances) if (static_cast<int>(I.Part)==Part) C->AddInstance(InstanceTransform(I));
            C->RegisterComponent();
        }
        Actor->Tags={GeneratedOwner,FeatureTag,FName(*(TEXT("Project.BuildingAssembly.Geometry=")+Identity))};
        if (!Matches(*Actor,Identity,Plan,Kit,Transform,Material,Options.Modules)) {World.DestroyActor(Actor);Error=TEXT("Staged assembly does not realize its plan.");return false;}
        if (Previous && Options.bExternalActor)
        {
            TInlineComponentArray<UInstancedStaticMeshComponent*> OldParts(Previous);
            if (!Previous->IsPackageExternal() || Previous->GetActorGuid()!=Options.ActorGuid || OldParts.Num()!=4 || !Previous->GetRootComponent())
            {World.DestroyActor(Actor);Error=TEXT("Persistent assembly ownership cannot be updated in place.");return false;}
            for (int Part=0;Part<4;++Part) if (!OldParts.ContainsByPredicate([&](const auto* C){return C->GetFName()==FName(*FString::Printf(TEXT("Part%d"),Part));}))
            {World.DestroyActor(Actor);Error=TEXT("Persistent assembly module family is missing.");return false;}
            Previous->Modify();Previous->SetActorTransform(Transform);Previous->Tags=Actor->Tags;
            for (int Part=0;Part<4;++Part)
            {
                auto* C=*OldParts.FindByPredicate([&](const auto* V){return V->GetFName()==FName(*FString::Printf(TEXT("Part%d"),Part));});
                C->Modify();C->SetRelativeTransform(FTransform::Identity);C->SetStaticMesh(Options.Modules[Part]);C->SetMaterial(0,Material);C->ClearInstances();
                for (const auto& I : Plan.Instances) if (static_cast<int>(I.Part)==Part) C->AddInstance(InstanceTransform(I));
            }
            World.DestroyActor(Actor);Previous->MarkPackageDirty();
            if (!Matches(*Previous,Identity,Plan,Kit,Transform,Material,Options.Modules)) {Error=TEXT("Transactional assembly update differs from its plan.");return false;}
            OutActor=Previous;return true;
        }
        if (Previous && !World.DestroyActor(Previous)) {World.DestroyActor(Actor);Error=TEXT("Cannot replace previous assembly.");return false;}
        OutActor=Actor;return true;
    }
}
