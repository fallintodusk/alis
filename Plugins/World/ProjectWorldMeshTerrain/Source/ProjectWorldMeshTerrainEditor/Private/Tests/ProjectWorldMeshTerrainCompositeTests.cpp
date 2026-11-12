// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldMeshTerrainProducer.h"
#include "ProjectWorldMeshTerrainSurfaceFidelity.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "MeshPartitionMeshData.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "StaticMeshCompiler.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ProjectWorldMeshTerrainPartition.h"
#include "ProjectWorldTerrainRuntimeRole.h"
#include "MeshPartitionEditorComponent.h"
#include "MeshPartitionDefinition.h"
#include "MeshPartitionCompiledSection.h"
#include "MeshPartitionStaticMeshComponent.h"
#include "MeshPartitionCollisionComponent.h"
#include "MeshPartitionTransformer.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "Misc/ScopeExit.h"
#include "ProjectWorldMeshTerrainBasinScene.h"
#include "ProjectWorldMeshTerrainStorageFixture.h"
#include "ProjectWorldMeshTerrainSectionCollection.h"
#include "ProjectWorldMeshTerrainCollisionFixture.h"
#include "ProjectWorldMeshTerrainCoastalFixture.h"
#include "ProjectWorldMeshTerrainCanonicalFixture.h"
#include "ProjectWorldMeshTerrainRegionWater.h"
#include "ProjectWorldMeshTerrainRegionDomain.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Level.h"
#include "ExternalPackageHelper.h"
#include "FileHelpers.h"
#include "WorldPartition/WorldPartition.h"
#include "ProjectWorldRealizationService.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "Tests/AutomationEditorCommon.h"
#include "AssetRegistry/AssetRegistryModule.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCompositeTerrainTests
{
    TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (double Value : Values) Result.Add(MakeShared<FJsonValueNumber>(Value));
        return Result;
    }
    bool Unload(UPackage* Package)
    {
        UPackageTools::FUnloadPackageParams Parameters({Package}); Parameters.bResetTransBuffer=false;
        return UPackageTools::UnloadPackages(Parameters);
    }
    struct FScratch
    {
        FString Root,Mount,Directory,PackageName;
        TArray<FString> Packages;
        bool PluginManaged=false;
        FAutomationTestBase* Test=nullptr;
        ~FScratch()
        {
            if (!PackageName.IsEmpty()) Packages.AddUnique(PackageName);
            TArray<UPackage*> Loaded;
            for (const auto& Name : Packages) if (auto* Package=FindPackage(nullptr,*Name)) Loaded.Add(Package);
            if (!Loaded.IsEmpty())
            {
                UPackageTools::FUnloadPackageParams Params(Loaded); Params.bResetTransBuffer=false;
                const bool Released=UPackageTools::UnloadPackages(Params);
                if (Test) Test->TestTrue(TEXT("Scratch mesh packages unload after the consumer proof."),Released);
                if (!Released) return;
            }
            if (!Mount.IsEmpty() && !PluginManaged) FPackageName::UnRegisterMountPoint(Mount,Directory);
        }
    };
    bool Read(FAutomationTestBase& Test,FScratch& Scratch,FProjectWorldCanonicalBundle& Bundle,FString& FixtureHash,
        const TCHAR* Exporter=TEXT("composite_mesh_fixture.py"),TSharedPtr<FJsonObject>* OutDocument=nullptr,bool Region=false)
    {
        const auto Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const FString Tmp=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"));
        Scratch.Root=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_OUTPUT"));
        if (Scratch.Root.IsEmpty()) Scratch.Root=Tmp/TEXT("world/composite_terrain")/Id;
        Scratch.Root=FPaths::ConvertRelativePathToFull(Scratch.Root);
        if (!Test.TestTrue(TEXT("Native composite evidence uses fresh project scratch."),
            FPaths::IsUnderDirectory(Scratch.Root,Tmp) && !IFileManager::Get().DirectoryExists(*Scratch.Root) &&
            IFileManager::Get().MakeDirectory(*Scratch.Root,true))) return false;
        FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE"));
        if (Path.IsEmpty())
        {
            const FString Destination=Scratch.Root/TEXT("input");
            const FString Script=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tools/World/CanonicalCompilation/tests")/Exporter);
            int32 Code=-1; FString Out,Error;
            if (!Test.TestTrue(TEXT("The permanent captured constructor creates a self-contained typed fixture."),
                FPlatformProcess::ExecProcess(TEXT("python.exe"),*FString::Printf(TEXT("-B \"%s\" \"%s\""),*Script,*Destination),&Code,&Out,&Error) && Code==0))
            { Test.AddError(Out+Error); return false; }
            Path=Destination/TEXT("composite-terrain-fixture.json");
        }
        else
        {
            const FString Expected=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_SHA256"));
            if (!Test.TestTrue(TEXT("External captured input is scratch-local and authenticated."),
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Path),Tmp) && Expected.Len()==64 &&
                FProjectWorldCanonicalLoader::ComputeFileSha256(Path,FixtureHash) && FixtureHash==Expected)) return false;
        }
        FString Text; TSharedPtr<FJsonObject> Document;
        if (!Test.TestTrue(TEXT("The complete consumed typed payload is hashed and parsed."),
            FProjectWorldCanonicalLoader::ComputeFileSha256(Path,FixtureHash) && FFileHelper::LoadFileToString(Text,*Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Document))) return false;
        if (Document->HasField(TEXT("canonical_result")))
        {
            if (OutDocument) *OutDocument=Document;
            return ProjectWorldTerrainCanonicalFixture::Read(Test,*Document,Bundle,Region);
        }
        if (!Test.TestTrue(TEXT("Captured source meaning and frame are explicit."),
            Document->GetStringField(TEXT("canonical_crs"))==TEXT("EPSG:32618") &&
            Document->GetStringField(TEXT("vertical_datum"))==TEXT("EPSG:3855") &&
            Document->GetStringField(TEXT("source_frame_bundle_sha256")).Len()==64 &&
            !Document->GetStringField(TEXT("reduction")).IsEmpty())) return false;
        const auto& Origin=Document->GetArrayField(TEXT("engine_georeference_origin_m"));
        const auto& Size=Document->GetArrayField(TEXT("sample_size"));
        const auto& Spacing=Document->GetArrayField(TEXT("sample_spacing_m"));
        const auto& Bounds=Document->GetArrayField(TEXT("bounds"));
        Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.VerticalDatum=TEXT("EPSG:3855"); Bundle.CoordinateTransform=TEXT("captured_typed_fixture");
        Bundle.EngineGeoreferenceOriginMeters=FVector2D(Origin[0]->AsNumber(),Origin[1]->AsNumber());
        Bundle.HeightOriginMeters=Document->GetNumberField(TEXT("height_origin_m"));
        Bundle.SampleSpacingMeters=FVector2D(Spacing[0]->AsNumber(),Spacing[1]->AsNumber());
        Bundle.CellQuads=FIntPoint(Size[0]->AsNumber()-1,Size[1]->AsNumber()-1);
        Bundle.CoordinateQuantizationMeters=.01; Bundle.HeightQuantizationMeters=.1;
        auto& Cell=Bundle.Cells.AddDefaulted_GetRef(); Cell.CellId=Document->GetStringField(TEXT("cell_id"));
        Cell.Bounds=FVector4d(Bounds[0]->AsNumber(),Bounds[1]->AsNumber(),Bounds[2]->AsNumber(),Bounds[3]->AsNumber());
        auto& T=Cell.Terrain; T.Bounds=Cell.Bounds; T.SampleSpacing=Bundle.SampleSpacingMeters;
        T.SamplesX=Size[0]->AsNumber(); T.SamplesY=Size[1]->AsNumber();
        T.SurfaceRoles={TEXT("ground"),TEXT("hydro_transition")};
        for (const auto& Value : Document->GetArrayField(TEXT("base_heights"))) T.HeightsMeters.Add(Value->AsNumber());
        for (const auto& Value : Document->GetArrayField(TEXT("base_weights"))) for (int32 Role=0; Role<2; ++Role)
            T.SurfaceWeights.FindOrAdd(T.SurfaceRoles[Role]).Add(Value->AsArray()[Role]->AsNumber());
        for (const auto& Value : Document->GetArrayField(TEXT("quads")))
        {
            const auto Object=Value->AsObject(); auto& Q=T.RefinedQuads.AddDefaulted_GetRef();
            const auto& Coordinate=Object->GetArrayField(TEXT("coordinate"));
            Q.Coordinate=FIntPoint(Coordinate[0]->AsNumber(),Coordinate[1]->AsNumber());
            for (const auto& Vertex : Object->GetArrayField(TEXT("vertices")))
            {
                const auto& P=Vertex->AsArray(); Q.Vertices.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()));
                Q.Weights.Add(FVector2f(P[3]->AsNumber(),P[4]->AsNumber()));
            }
            for (const auto& Face : Object->GetArrayField(TEXT("triangles")))
            {
                const auto& I=Face->AsArray(); Q.Triangles.Add(FIntVector(I[0]->AsNumber(),I[1]->AsNumber(),I[2]->AsNumber()));
            }
        }
        if (OutDocument) *OutDocument=Document;
        return Test.TestTrue(TEXT("The reduced grid consists exclusively of complete selected quads."),
            T.SamplesX>=2 && T.SamplesX<=32 && T.SamplesY>=2 && T.SamplesY<=32 &&
            T.HeightsMeters.Num()==T.SamplesX*T.SamplesY && T.RefinedQuads.Num()==(T.SamplesX-1)*(T.SamplesY-1));
    }
    bool SourceWeights(const FProjectWorldCanonicalBundle& Bundle,const UE::Geometry::FDynamicMesh3& Source)
    {
        const auto& Terrain=Bundle.Cells[0].Terrain;
        if (!Source.Attributes() || Source.Attributes()->NumWeightLayers()!=2) return false;
        TMap<FVector,FVector2f> Specified;
        for (const auto& Q : Terrain.RefinedQuads) for (int32 I=0; I<Q.Vertices.Num(); ++I)
            Specified.Add(Q.Vertices[I],Q.Weights[I]);
        if (Specified.Num()!=Source.VertexCount()) return false;
        for (int32 Id : Source.VertexIndicesItr())
        {
            const FVector Canonical=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,Source.GetVertex(Id));
            const FVector2f* Expected=nullptr;
            for (const auto& Pair : Specified) if (Pair.Key.Equals(Canonical,1e-8)) { Expected=&Pair.Value; break; }
            if (!Expected) return false;
            for (int32 Role=0; Role<2; ++Role)
            {
                const auto* Layer=Source.Attributes()->GetWeightLayer(Role); float Actual=0; Layer->GetValue(Id,&Actual);
                if (Layer->GetName()!=Terrain.SurfaceRoles[Role] || !FMath::IsNearlyEqual(Actual,(*Expected)[Role],1e-6f)) return false;
            }
        }
        return true;
    }
    bool NativeWeights(const UE::Geometry::FDynamicMesh3& Source,const UE::MeshPartition::FMeshData& Native)
    {
        if (Native.VertexCount()!=Source.VertexCount() || Native.GetWeightLayerNames().Num()!=2) return false;
        for (int32 Role=0; Role<2; ++Role)
        {
            const auto* Layer=Source.Attributes()->GetWeightLayer(Role);
            if (!Native.HasWeightLayer(Layer->GetName())) return false;
            for (int32 Id : Source.VertexIndicesItr())
            {
                float Expected=0; Layer->GetValue(Id,&Expected);
                if (Native.GetWeightLayerValue(Layer->GetName(),Id)!=Expected) return false;
            }
        }
        return true;
    }
    UStaticMesh* Compile(FScratch& Scratch,const FMeshDescription& Description,FAutomationTestBase& Test)
    {
        Scratch.Mount=TEXT("/ProjectWorldCompositeTerrain")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("/");
        Scratch.Directory=Scratch.Root/TEXT("packages/"); IFileManager::Get().MakeDirectory(*Scratch.Directory,true);
        FPackageName::RegisterMountPoint(Scratch.Mount,Scratch.Directory); Scratch.PackageName=Scratch.Mount+TEXT("SM_Terrain");
        auto* Package=CreatePackage(*Scratch.PackageName);
        auto* Mesh=NewObject<UStaticMesh>(Package,TEXT("Surface"),RF_Public|RF_Standalone);
        Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface)));
        Mesh->bAllowCPUAccess=true; Mesh->bGenerateMeshDistanceField=false; Mesh->bHasNavigationData=false;
        Mesh->SetNumSourceModels(1);
        auto Nanite=Mesh->GetNaniteSettings(); Nanite.bEnabled=true; Nanite.FallbackTarget=ENaniteFallbackTarget::PercentTriangles;
        Nanite.FallbackPercentTriangles=Nanite.KeepPercentTriangles=1; Nanite.FallbackRelativeError=Nanite.TrimRelativeError=0;
        Mesh->SetNaniteSettings(Nanite);
        UStaticMesh::FBuildMeshDescriptionsParams Parameters; Parameters.bUseHashAsGuid=true;
        if (!Test.TestTrue(TEXT("Native MeshPartition data compiles through the real StaticMesh builder."),Mesh->BuildFromMeshDescriptions({&Description},Parameters))) return nullptr;
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
        Mesh->CreateBodySetup(); auto* Body=Mesh->GetBodySetup(); Body->CollisionTraceFlag=CTF_UseComplexAsSimple;
        Body->bDoubleSidedGeometry=true; Body->CreatePhysicsMeshes();
        const FString Filename=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        if (!Test.TestTrue(TEXT("The isolated native mesh saves only under project scratch."),UPackage::SavePackage(Package,Mesh,*Filename,Args))) return nullptr;
        const TWeakObjectPtr<UStaticMesh> Before(Mesh); Mesh=nullptr;
        if (!Test.TestTrue(TEXT("The candidate package unloads before reading saved geometry."),Unload(Package)) ||
            !Test.TestFalse(TEXT("Saved proof cannot reuse the original in-memory mesh."),Before.IsValid())) return nullptr;
        Mesh=LoadObject<UStaticMesh>(nullptr,*(Scratch.PackageName+TEXT(".Surface")));
        if (!Test.TestNotNull(TEXT("The exact saved native mesh reloads."),Mesh)) return nullptr;
        if (!Test.TestTrue(TEXT("Reload retains actual built Nanite and cooked complex collision."),Mesh->HasValidNaniteData() &&
            Mesh->GetBodySetup() && Mesh->GetBodySetup()->bCreatedPhysicsMeshes && Mesh->GetBodySetup()->CollisionTraceFlag==CTF_UseComplexAsSimple)) return nullptr;
        return Mesh;
    }

    bool ReadBuffer(FRHIBuffer* Buffer,uint32 Bytes,TArray<uint8>& Out)
    {
        if (!Buffer || Bytes==0 || Bytes>64*1024*1024 || Buffer->GetSize()<Bytes) return false;
        FRHIGPUBufferReadback Readback(TEXT("ProjectWorldSectionGeometry"));
        ENQUEUE_RENDER_COMMAND(ProjectWorldSectionCopy)([&Readback,Buffer,Bytes](FRHICommandListImmediate& Commands)
            {Readback.EnqueueCopy(Commands,Buffer,Bytes);});
        FlushRenderingCommands();
        const double Deadline=FPlatformTime::Seconds()+10; bool Ready=false;
        while (!Ready && FPlatformTime::Seconds()<Deadline)
        {
            ENQUEUE_RENDER_COMMAND(ProjectWorldSectionRead)([&Readback,&Out,&Ready,Bytes](FRHICommandListImmediate&)
            {
                if (!Readback.IsReady()) return;
                if (const void* Data=Readback.Lock(Bytes))
                {Out.SetNumUninitialized(Bytes);FMemory::Memcpy(Out.GetData(),Data,Bytes);Readback.Unlock();Ready=true;}
            });
            FlushRenderingCommands();
            if (!Ready) FPlatformProcess::Sleep(.001f);
        }
        return Ready;
    }

    TArray<TSharedPtr<FJsonValue>> Matrix(const FTransform& Transform)
    {
        TArray<TSharedPtr<FJsonValue>> Rows; const FMatrix M=Transform.ToMatrixWithScale();
        for (int32 I=0;I<4;++I) Rows.Add(MakeShared<FJsonValueArray>(Numbers({M.M[I][0],M.M[I][1],M.M[I][2],M.M[I][3]})));
        return Rows;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldCompositeTerrainConsumerTest,
    "Project.World.Realization.MeshTerrain.CompositeConsumer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldCompositeTerrainConsumerTest::RunTest(const FString&)
{
    using namespace ProjectWorldCompositeTerrainTests;
    FScratch Scratch; Scratch.Test=this; FProjectWorldCanonicalBundle Bundle; FString FixtureHash,Error;
    if (!Read(*this,Scratch,Bundle,FixtureHash)) return false;
    const auto& Cell=Bundle.Cells[0]; UE::Geometry::FDynamicMesh3 Source;
    if (!TestTrue(TEXT("The existing Terrain adapter realizes the captured complete composite."),
        ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Cell,Source,Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Dynamic source geometry and semantic weights match the final canonical input."),
        ProjectWorldMeshTerrainProducer::MatchesCellMesh(Bundle,Cell,Source,Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Each source semantic weight independently matches its specifying input vertex and role."),SourceWeights(Bundle,Source))) return false;
    auto WrongWeight=Source; const float Invalid=-.25f;
    WrongWeight.Attributes()->GetWeightLayer(0)->SetValue(*WrongWeight.VertexIndicesItr().begin(),&Invalid);
    TestFalse(TEXT("A wrong realized semantic weight is rejected independently of the builder."),SourceWeights(Bundle,WrongWeight));
    TArray<FProjectWorldSurfaceTriangle> Expected;
    if (!ProjectWorldTerrainSurface::Triangles(Cell,FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W)),Expected,Error))
    { AddError(Error); return false; }
    FProjectWorldMeshTerrainSurfaceFidelity Fidelity;
    auto Missing=Expected; int32 Largest=0; double LargestArea=0;
    for (int32 I=0; I<Missing.Num(); ++I)
    {
        const auto& F=Missing[I]; const double Area=ProjectWorldTerrainSurface::Area({FVector2D(F.Points[0]),FVector2D(F.Points[1]),FVector2D(F.Points[2])});
        if (Area>LargestArea) { LargestArea=Area; Largest=I; }
    }
    Missing.RemoveAt(Largest);
    TestFalse(TEXT("Removing a positive-area source face cannot certify complete coverage."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Missing,1e-4,Fidelity,Error));
    auto Stale=Cell; Stale.Terrain.RefinedQuads.Reset(); TArray<FProjectWorldSurfaceTriangle> Old;
    if (!ProjectWorldTerrainSurface::Triangles(Stale,FBox2D(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W)),Old,Error))
    { AddError(Error); return false; }
    TestFalse(TEXT("Original grid geometry cannot authenticate a corrected interior."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Old,1e-4,Fidelity,Error));
    FProjectWorldCanonicalBundle Balanced; Balanced.CellQuads=FIntPoint(1,1);
    auto& Control=Balanced.Cells.AddDefaulted_GetRef(); Control.Bounds=FVector4d(0,0,30,30);
    Control.Terrain.Bounds=Control.Bounds; Control.Terrain.SamplesX=Control.Terrain.SamplesY=2;
    Control.Terrain.SampleSpacing=FVector2D(30,30); Control.Terrain.HeightsMeters={3,3,3,3};
    auto& Q=Control.Terrain.RefinedQuads.AddDefaulted_GetRef(); Q.Coordinate=FIntPoint(0,0);
    const FVector A(0,0,3),B(30,0,3),C(0,30,3),D(30,30,3),M(10,10,3);
    Q.Vertices={A,B,C,D}; Q.Weights.Init(FVector2f(1,0),4); Q.Triangles={FIntVector(0,1,2),FIntVector(1,3,2)};
    const TArray<FProjectWorldSurfaceTriangle> CompleteControl={{{A,B,C}},{{B,D,C}}};
    TestTrue(TEXT("The planar overlap control has complete valid source coverage."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Balanced,CompleteControl,1e-4,Fidelity,Error));
    const TArray<FProjectWorldSurfaceTriangle> BalancedHole={{{A,B,M}},{{A,B,M}},{{B,C,M}},{{B,D,C}}};
    TestFalse(TEXT("A genuine overlap cannot balance a missing region even at identical heights and area."),
        ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Balanced,BalancedHole,1e-4,Fidelity,Error));
    TestTrue(TEXT("The balanced control refuses actual overlap rather than a malformed fixture."),Error.Contains(TEXT("Overlapping compiled faces")));
    // Isolate storage precision without changing the canonical or engine world frame.
    const bool LocalStorage=FParse::Param(FCommandLine::Get(),TEXT("ProjectCompositeLocalStorage"));
    const FVector StorageOrigin=LocalStorage ? FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
        FVector(Cell.Bounds.X,Cell.Bounds.Y,Bundle.HeightOriginMeters)) : FVector::ZeroVector;
    const FTransform MeshToWorld(StorageOrigin);
    UE::MeshPartition::FMeshData Native; Native.AppendDynamicMesh(Source,MeshToWorld.Inverse()); Native.RecomputeNormals();
    if (!TestTrue(TEXT("Both semantic channels survive the actual MeshPartition data conversion."),NativeWeights(Source,Native))) return false;
    FMeshDescription Description; FStaticMeshAttributes(Description).Register(); Native.ConvertToMeshDescription(Description);
    UStaticMesh* Mesh=Compile(Scratch,Description,*this); if (!Mesh) return false;
    TArray<FProjectWorldSurfaceTriangle> Compiled; TArray<TSharedPtr<FJsonValue>> Vertices,Triangles;
    const auto& LOD=Mesh->GetRenderData()->LODResources[0];
    for (uint32 I=0; I<LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(); ++I)
    {
        const FVector P=MeshToWorld.TransformPosition(FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I)));
        Vertices.Add(MakeShared<FJsonValueArray>(Numbers({P.X,P.Y,P.Z})));
    }
    for (int32 I=0; I<LOD.IndexBuffer.GetNumIndices(); I+=3)
    {
        FProjectWorldSurfaceTriangle Face;
        for (int32 J=0; J<3; ++J) Face.Points[J]=FProjectWorldCanonicalLoader::UnrealToCanonical(Bundle,
            MeshToWorld.TransformPosition(FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+J)))));
        Compiled.Add(Face); Triangles.Add(MakeShared<FJsonValueArray>(Numbers({double(LOD.IndexBuffer.GetIndex(I)),double(LOD.IndexBuffer.GetIndex(I+1)),double(LOD.IndexBuffer.GetIndex(I+2))})));
    }
    const bool Covered=ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Compiled,1e-4,Fidelity,Error);
    auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("scope"),TEXT("Captured typed Terrain -> MeshPartition data -> saved StaticMesh CPU fallback; no GPU depth, section partition, full-cell frontier, gameplay or authority admission."));
    Receipt->SetStringField(TEXT("fixture_sha256"),FixtureHash);
    Receipt->SetArrayField(TEXT("mesh_to_world"),Matrix(MeshToWorld));
    Receipt->SetBoolField(TEXT("isolated_local_storage_experiment"),LocalStorage);
    Receipt->SetArrayField(TEXT("engine_georeference_origin_m"),Numbers({Bundle.EngineGeoreferenceOriginMeters.X,Bundle.EngineGeoreferenceOriginMeters.Y}));
    Receipt->SetNumberField(TEXT("height_origin_m"),Bundle.HeightOriginMeters);
    Receipt->SetNumberField(TEXT("expected_triangles"),Fidelity.ExpectedTriangles);
    Receipt->SetNumberField(TEXT("mismatched_triangles"),Fidelity.MismatchedTriangles);
    Receipt->SetNumberField(TEXT("maximum_whole_field_error_m"),Fidelity.MaximumErrorMeters);
    Receipt->SetStringField(TEXT("whole_field_diagnostic"),Error);
    Receipt->SetArrayField(TEXT("vertices_cm"),Vertices); Receipt->SetArrayField(TEXT("triangles"),Triangles);
    FString Text; const FString Path=Scratch.Root/TEXT("compiled.json");
    if (!TestTrue(TEXT("Complete compiled geometry and metrics are retained for independent verification."),
        FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text)) && FFileHelper::SaveStringToFile(Text,*Path))) return false;
    AddInfo(FString::Printf(TEXT("CompositeTerrainConsumer fixture=%s source_vertices=%d source_faces=%d compiled_faces=%d mismatches=%d error_m=%.17g output=%s"),
        *FixtureHash,Source.VertexCount(),Source.TriangleCount(),Compiled.Num(),Fidelity.MismatchedTriangles,Fidelity.MaximumErrorMeters,*Path));
    return TestTrue(TEXT("Every saved compiled face agrees with the complete specifying affine terrain field."),Covered);
}

static bool RunSectionConsumer(FAutomationTestBase& Test,const bool StorageOnly,const bool SplitStorage=false,const TCHAR* Exporter=nullptr,const bool Coastal=false,const bool RequireCanonical=false,const bool Region=false)
{
    using namespace ProjectWorldCompositeTerrainTests;
    using namespace UE::MeshPartition;
    const bool SectionStorage=SplitStorage || FParse::Param(FCommandLine::Get(),TEXT("ProjectCompositeSectionStorage"));
    if (!Test.TestTrue(TEXT("Section storage is an isolated storage-only comparison."),!SectionStorage || StorageOnly)) return false;
    FScratch Scratch; Scratch.Test=&Test; FProjectWorldCanonicalBundle Bundle; FString FixtureHash,Error,MapSha256;
    TSharedPtr<FJsonObject> Document,WaterExpected;
    if (!Read(Test,Scratch,Bundle,FixtureHash,Exporter ? Exporter : StorageOnly ? TEXT("ground_continuation_mesh_fixture.py") : TEXT("identified_basin_mesh_fixture.py"),&Document,Region)) return false;
    const bool Canonical=Document->HasField(TEXT("canonical_result"));
    ProjectWorldRegionWater::FState RegionWater;
    ProjectWorldRegionDomain::FStructures RegionStructures;
    if (!Test.TestTrue(TEXT("Regional observation cannot select injected geometry."),!Region || (RequireCanonical && Canonical))) return false;
    if (!Test.TestTrue(TEXT("The selected canonical acceptance path cannot fall back to a manufactured typed fixture."),!RequireCanonical || Canonical)) return false;
    TArray<FVector> RequiredPhysicsPoints;
    const bool PhysicsInput=ProjectWorldTerrainCollisionFixture::ReadRequiredPoints(*Document,RequiredPhysicsPoints,Error);
    if (!Test.TestTrue(FString::Printf(TEXT("Declared canonical physics queries are valid: %s"),*Error),PhysicsInput)) return false;
    const bool Gameplay=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_GAMEPLAY"))==TEXT("1");
    const bool NativeBuild=Gameplay || Region;
    if (Canonical && !Test.TestTrue(TEXT("The canonical Terrain consumer excludes basin gameplay, Water and storage experiments."),
        !Gameplay && !StorageOnly && !SplitStorage && !Coastal &&
        FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_WATER")).IsEmpty())) return false;
    if (Coastal && !Test.TestTrue(TEXT("Coastal evaluation requires authenticated context and the native gameplay envelope."),
        Gameplay && Document->HasTypedField<EJson::Object>(TEXT("coastal_context")))) return false;
    if (Coastal && !ProjectWorldCoastalSceneTest::ValidateInput(Test,*Document,Bundle)) return false;
    if (StorageOnly && !Test.TestTrue(TEXT("Storage-only evidence excludes gameplay and joint Water."),
        !Gameplay && FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_WATER")).IsEmpty())) return false;
    const bool JointWater=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_WATER"))==TEXT("1");
    if (Coastal && !Test.TestTrue(TEXT("The coastal contract always includes ordinary original Water."),JointWater)) return false;
    if (!Test.TestTrue(TEXT("The joint Water fixture requires the native saved gameplay scene."),!JointWater || Gameplay)) return false;
    FString ExitSentinel;
    ProjectWorldConstructedRuntimeTest::FPawnSelection Pawn;
    if (Gameplay && !Test.TestTrue(TEXT("Gameplay requires the explicit configured Hero and direct-map frontend bypass."),
        GEditor && !GEditor->PlayWorld && ProjectWorldConstructedRuntimeTest::ConfiguredPawnRequested() &&
        FParse::Param(FCommandLine::Get(),TEXT("ProjectSkipFrontEnd")) && !ISourceControlModule::Get().GetProvider().IsEnabled() &&
        FPlatformMisc::GetEnvironmentVariable(TEXT("ALIS_NO_PERSISTENT_EDITOR"))==TEXT("1") &&
        FParse::Value(FCommandLine::Get(),TEXT("testexit="),ExitSentinel) && ExitSentinel==TEXT("Automation Test Queue Empty"))) return false;
    auto* Shared=LoadObject<UMeshPartitionDefinition>(nullptr,ProjectWorldMeshTerrainProducer::SharedDefinitionObjectPath);
    if (!Test.TestNotNull(TEXT("The actual accepted production definition exists."),Shared) ||
        !Test.TestNotNull(TEXT("Its accepted terrain material exists."),Shared->GetMaterial()) ||
        !Test.TestTrue(TEXT("The source definition is validated read-only."),
            ProjectWorldMeshTerrainProducer::ValidateSharedDefinition(Shared->GetMaterial()->GetPathName(),Error)))
    {Test.AddError(Error);return false;}
    FString SharedFile=FPackageName::LongPackageNameToFilename(Shared->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    FString SharedBefore; if (!Test.TestTrue(TEXT("Shared definition source bytes are authenticated."),
        FProjectWorldCanonicalLoader::ComputeFileSha256(SharedFile,SharedBefore))) return false;
    if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_REPLAY")).IsEmpty())
    {
        if (!Test.TestTrue(TEXT("Saved-scene replay requires the cold configured gameplay envelope."),Gameplay)) return false;
        return ProjectWorldBasinSceneTest::Replay(Test,Scratch.Root,MoveTemp(Bundle),*Document,FixtureHash,SharedBefore,Coastal);
    }
    Scratch.Mount=TEXT("/ProjectWorldBasinSections")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("/");
    Scratch.Directory=Scratch.Root/(NativeBuild ? TEXT("runtime_plugin/Content/") : TEXT("packages/")); IFileManager::Get().MakeDirectory(*Scratch.Directory,true);
    Scratch.PackageName=Scratch.Mount+TEXT("SectionMap");Scratch.PluginManaged=NativeBuild;
    if (NativeBuild)
    {
        ProjectWorldBasinSceneTest::FInput Plugin;Plugin.Root=Scratch.Root;Plugin.Mount=Scratch.Mount;Plugin.Directory=Scratch.Directory;
        if (!ProjectWorldBasinSceneTest::PrepareNativePlugin(Test,Plugin)) return false;
    }
    else FPackageName::RegisterMountPoint(Scratch.Mount,Scratch.Directory);
    if (Gameplay || Canonical)
    {
        const TArray<FString> Roots={ULevel::GetExternalActorsPath(Scratch.PackageName),FExternalPackageHelper::GetExternalObjectsPath(Scratch.PackageName)};
        for (const auto& Root : Roots)
        {
            if (!Test.TestTrue(TEXT("External actor/object package roots resolve inside this scratch scene."),
                FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Root+TEXT("/ConfinementProbe"))),Scratch.Directory))) return false;
        }
    }
    auto* Package=CreatePackage(*Scratch.PackageName);
    Package->SetPackageFlags(PKG_NewlyCreated);
    UPackage* DefinitionPackage=Coastal ? CreatePackage(*(Scratch.Mount+TEXT("PrivateDefinition"))) : Package;
    auto* Definition=(Gameplay && !Coastal) || Canonical ? Shared : DuplicateObject<UMeshPartitionDefinition>(Shared,DefinitionPackage,TEXT("Definition"));
    if (Coastal) {Definition->SetFlags(RF_Public|RF_Standalone);Scratch.Packages.Add(DefinitionPackage->GetName());}
    if (!Gameplay && !Canonical) Definition->SetFlags(RF_Public|RF_Standalone);
    if (SectionStorage && !ProjectWorldTerrainStorageFixture::PrepareDefinition(Test,*Definition,*Shared)) return false;
    if (!Test.TestTrue(TEXT("The cloned definition keeps the accepted material."),Definition->GetMaterial()==Shared->GetMaterial())) return false;
    const auto Init=UWorld::InitializationValues().CreateWorldPartition(NativeBuild).EnableWorldPartitionStreaming(NativeBuild);
    UWorld* World=UWorld::CreateWorld(EWorldType::Editor,false,TEXT("SectionMap"),Package,true,ERHIFeatureLevel::Num,&Init);
    if (!Test.TestNotNull(TEXT("A separate scratch editor world owns every generated section."),World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
    bool NativeEditorWorld=false;
    TArray<FWorldPartitionReference> LoadedReferences;
    auto DestroyOwnedWorld=[&]()
    {
        if (World)
        {
            Scratch.Packages.AddUnique(World->GetPackage()->GetName());
            for (auto* External : World->GetPackage()->GetExternalPackages())
            {Scratch.Packages.AddUnique(External->GetName());if (Gameplay && !NativeEditorWorld) ProjectWorldBasinSceneTest::DescribePackage(Test,External->GetName());}
            if (NativeEditorWorld)
            {
                if (!Test.TestTrue(TEXT("The collector retains the exact native-loaded Editor world."),GEditor->GetEditorWorldContext().World()==World)) return;
                LoadedReferences.Reset();
                FAutomationEditorCommonUtils::CreateNewMap();
            }
            else {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);World->RemoveFromRoot();}
            World=nullptr;
        }
    };
    ON_SCOPE_EXIT {DestroyOwnedWorld();};
    World->SetFlags(RF_Public|RF_Standalone);
    if (Canonical) World->PersistentLevel->SetUseExternalActors(true);
    if (Region && !ProjectWorldRegionWater::Apply(Test,*World,*Document,Bundle,Scratch.Mount,Scratch.Packages,RegionWater)) return false;
    if (Coastal && !ProjectWorldCoastalSceneTest::Prepare(Test,*World,Bundle,*Definition,*Shared)) return false;
    const auto& Bounds=Bundle.Cells[0].Bounds;
    const auto StartValues=Coastal ? Document->GetObjectField(TEXT("coastal_context"))->GetArrayField(TEXT("start")) : Numbers({Bounds.X+3,Bounds.Y+3,21});
    const FVector Start=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(StartValues[0]->AsNumber(),StartValues[1]->AsNumber(),StartValues[2]->AsNumber()));
    if (Gameplay && !ProjectWorldConstructedRuntimeTest::ConfigurePawn(World,Start,Test,Pawn)) return false;
    AProjectWorldMeshTerrainPartition* Partition=nullptr;
    if (Gameplay || Canonical)
    {
        if (JointWater)
        {
            ProjectWorldBasinSceneTest::FInput Water;Water.Bundle=Bundle;Water.Mount=Scratch.Mount;Water.Directory=Scratch.Directory;
            if (!(Coastal ? ProjectWorldCoastalSceneTest::ApplyWater(Test,*World,*Document,Water) :
                ProjectWorldBasinSceneTest::ApplyJointWater(Test,World,*Document,Water))) return false;
            Bundle=MoveTemp(Water.Bundle);Scratch.Packages.Append(Water.Packages);WaterExpected=Water.Expected;
        }
        if (!Canonical) Bundle.Cells[0].Terrain.ArtifactHash=FixtureHash;
        FProjectWorldRealizationResult Result;
        if (!Test.TestTrue(TEXT("The bounded runtime candidate uses actual Terrain Apply and native provider channels."),
            ProjectWorldMeshTerrainProducer::ApplyLayer(World,Bundle,TEXT(""),Shared->GetMaterial(),FixtureHash,Result,Error))) {Test.AddError(Error);return false;}
        int32 Count=0;
        for (TActorIterator<AProjectWorldMeshTerrainPartition> It(World);It;++It) {Partition=*It;++Count;}
        if (!Test.TestEqual(TEXT("Actual Terrain Apply creates one owned partition root."),Count,1)) return false;
    }
    else Partition=World->SpawnActor<AProjectWorldMeshTerrainPartition>();
    if (!Test.TestNotNull(TEXT("The actual project partition actor creates its editor adapter."),Partition)) return false;
    const FVector StorageOrigin=StorageOnly && !SectionStorage ? FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,
        FVector(Bounds.X,Bounds.Y,Bundle.HeightOriginMeters)) : FVector::ZeroVector;
    if (StorageOnly && !SectionStorage) Partition->SetActorTransform(FTransform(StorageOrigin));
    Partition->SetMeshPartitionDefinition(Definition);
    if (NativeBuild)
    {
        Partition->MarkPackageDirty();
        const FString MapFile=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetMapPackageExtension());
        if (!Test.TestTrue(TEXT("The accepted definition and actual provider descriptors save before native building."),FEditorFileUtils::SaveMap(World,MapFile))) return false;
        TArray<TWeakObjectPtr<UObject>> Originals={World,Partition};
        if (JointWater) ProjectWorldBasinSceneTest::TrackWaterInstances(World,Originals);
        DestroyOwnedWorld();Definition=nullptr;Partition=nullptr;Package=nullptr;
        TArray<UPackage*> Loaded;
        for (const auto& Name : Scratch.Packages) if (auto* Existing=FindPackage(nullptr,*Name)) Loaded.Add(Existing);
        UPackageTools::FUnloadPackageParams Params(Loaded);Params.bResetTransBuffer=false;
        if (!Test.TestTrue(TEXT("Every original source package unloads before the commandlet build."),UPackageTools::UnloadPackages(Params))) return false;
        for (const auto& Original : Originals)
            if (!Test.TestFalse(TEXT("The commandlet cannot build against live parent source instances."),Original.IsValid())) return false;
        ProjectWorldBasinSceneTest::FInput Build;
        Build.Root=Scratch.Root;Build.Mount=Scratch.Mount;Build.Directory=Scratch.Directory;Build.PackageName=Scratch.PackageName;
        if (!ProjectWorldBasinSceneTest::BuildWithNativeCommandlet(Test,Build)) return false;
        if (!Test.TestTrue(TEXT("The exact native-built map dependency is authenticated before inspection."),
            FProjectWorldCanonicalLoader::ComputeFileSha256(MapFile,MapSha256))) return false;
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanPathsSynchronous({Scratch.Mount},true);
        const bool LoadedMap=FEditorFileUtils::LoadMap(MapFile,false,false);
        auto* Current=GEditor->GetEditorWorldContext().World();
        if (Current && Current->GetOutermost()->GetName()==Scratch.PackageName) {World=Current;NativeEditorWorld=true;}
        if (!Test.TestTrue(TEXT("The parent reopens the exact successful commandlet output."),LoadedMap && World && NativeEditorWorld && World->GetWorldPartition())) return false;
        World->GetWorldPartition()->LoadAllActors(LoadedReferences);World->UpdateWorldComponents(false,false);World->SendAllEndOfFrameUpdates();
        Package=World->GetPackage();Definition=Coastal ? LoadObject<UMeshPartitionDefinition>(nullptr,*(Scratch.Mount+TEXT("PrivateDefinition.Definition"))) : Shared;
        for (TActorIterator<AProjectWorldMeshTerrainPartition> It(World);It;++It) Partition=*It;
        if (!Test.TestTrue(TEXT("The native child retains the accepted read-only definition and owning partition."),
            Definition && Partition && Partition->GetMeshPartitionDefinition()==Definition)) return false;
    }
    auto* Editor=Cast<UMeshPartitionEditorComponent>(Partition->GetMeshPartitionComponent());
    if (!Test.TestNotNull(TEXT("The real native editor component is used."),Editor)) return false;
    UE::Geometry::FDynamicMesh3 Source;
    if (!Test.TestTrue(TEXT("The owner realizes the complete captured joint terrain."),
        ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Bundle.Cells[0],Source,Error))) {Test.AddError(Error);return false;}
    FMeshData Native; Native.AppendDynamicMesh(Source,FTransform(StorageOrigin).Inverse()); Native.RecomputeNormals();
    if (!Test.TestTrue(TEXT("Original role weights survive native input conversion."),NativeWeights(Source,Native))) return false;
    int32 SourceFaces=Source.TriangleCount();
    if (Region) for (int32 Index=1;Index<Bundle.Cells.Num();++Index)
    {
        UE::Geometry::FDynamicMesh3 Additional;
        if (!ProjectWorldMeshTerrainProducer::BuildCellMesh(Bundle,Bundle.Cells[Index],Additional,Error)) {Test.AddError(Error);return false;}
        FMeshData Part;Part.AppendDynamicMesh(Additional,FTransform(StorageOrigin).Inverse());Part.RecomputeNormals();
        if (!Test.TestTrue(TEXT("Every original selected cell preserves both native role weights."),NativeWeights(Additional,Part))) return false;
        Native.AppendDynamicMesh(Additional,FTransform(StorageOrigin).Inverse());SourceFaces+=Additional.TriangleCount();
    }
    auto SharedData=MakeShared<FMeshData>(); SharedData->Copy(Native);
    auto Receipt=MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("fixture_sha256"),FixtureHash);
    if (WaterExpected) Receipt->SetObjectField(TEXT("water"),WaterExpected);
    Receipt->SetStringField(TEXT("source_definition_sha256"),SharedBefore);
    Receipt->SetStringField(TEXT("expected_definition_object_path"),Definition->GetPathName());
    Receipt->SetStringField(TEXT("accepted_material"),Shared->GetMaterial()->GetPathName());
    Receipt->SetBoolField(TEXT("gameplay_requested"),Gameplay);
    Receipt->SetBoolField(TEXT("isolated_storage_frame_only"),StorageOnly);
    Receipt->SetBoolField(TEXT("compiled_section_storage_experiment"),SectionStorage);
    Receipt->SetBoolField(TEXT("native_grid_split_storage_experiment"),SplitStorage);
    Receipt->SetArrayField(TEXT("native_parent_transform"),Matrix(Partition->GetActorTransform()));
    Receipt->SetStringField(TEXT("scope"),Region ?
        TEXT("Authenticated selected complete-cell Terrain through the native provider/channel builder and regional ordinary Water; saved/reloaded fields, collision witnesses and diagnostic material capture. Selected-cell Water preparation is not full-world equivalence; no Hero traversal, streaming, geographic acceptance or authority admission.") : SplitStorage ?
        TEXT("Native 30m ROI grid split and section-local storage, combined field and actual shared decoded trace. Diagnostic grid only; no production WP grid, section reload, gameplay or authority admission.") : SectionStorage ?
        TEXT("Isolated compiled-section storage using private pipelines and parent-local input; actual components and generic physical probes. No production enablement, section partition/reload, walkability, Hero or authority admission.") : StorageOnly ?
        TEXT("Isolated one-cell native parent storage frame; actual configured visual/collision components and generic physical probes. No bed/coast/grade/walkability, section partition, section reload, Hero, WP or production admission.") : Gameplay ?
        TEXT("Actual Terrain Apply/provider channels and separate native WorldPartitionBuilder, saved configured sections/GPU fallback/collision; Hero continuation reported separately, no rendered Nanite or authority admission.") :
        TEXT("Actual configured sections, GPU fallback buffers, cooked collision and generic34/88cm physical probes; no configured Hero walking, channels/grouping, section reload, rendered Nanite, WorldPartitionBuilder or authority admission."));
    TArray<TSharedPtr<FJsonValue>> Variants;
    TMap<FName,ProjectWorldTerrainStorageFixture::FSplitSections> SavedSplits;
    for (const auto& Variant : Definition->GetCompiledSectionBuildVariants())
    {
        if (!Test.TestTrue(TEXT("Only the coastal private definition requests the native WP grid splitter."),Variant.bSplitSectionsToMatchWorldPartitionRuntimeGrid==Coastal)) return false;
        FCompiledSectionBuildInfo Info; Info.BuildKey=FGuid::NewGuid(); Info.BuildVariantName=Variant.Name;
        Info.MegaMeshPath=FSoftObjectPath(Partition);
        if (!Test.TestTrue(TEXT("Section build info binds the exact cloned definition."),Info.SetMegaMeshDefinition(Definition))) return false;
        FPrepareCompiledSectionsParams Params{Native};
        TArray<ACompiledSection*> Sections;
        ProjectWorldTerrainStorageFixture::FSplitSections Split;
        if (NativeBuild)
        {
            for (TActorIterator<ACompiledSection> It(World);It;++It)
                if (It->GetBuildInfo().BuildVariantName==Variant.Name) Sections.Add(*It);
        }
        else if (SplitStorage)
        {
            const FVector Origin=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Bounds.X,Bounds.Y,Bundle.HeightOriginMeters));
            if (!ProjectWorldTerrainStorageFixture::PrepareSplit(Test,*Editor,Info,Variant,Native,Partition->GetActorTransform(),Origin,Split)) return false;
            Sections=Split.Sections;
        }
        else Sections=Editor->PrepareCompiledSections(Info,Variant,Params);
        if (Coastal)
        {if (!Test.TestTrue(TEXT("The native builder produces multiple bounded coastal sections."),Sections.Num()>1 && Sections.Num()<=961)) return false;}
        else if (Region)
        {if (!Test.TestTrue(TEXT("The selected complete regional domain has a bounded positive native section census."),Sections.Num()>0 && Sections.Num()<=2*Bundle.Cells.Num())) return false;}
        else if (!Test.TestEqual(TEXT("The exact bounded fixture supplies every declared native section."),Sections.Num(),SplitStorage ? Bundle.Cells[0].Terrain.RefinedQuads.Num() : 1)) return false;
        if (Canonical) {Split.Sections=Sections;SavedSplits.Add(Variant.Name,Split);}
        TArray<FTransformerUnit> Units;
        for (auto* Section : Sections)
        {
            if (!Test.TestTrue(TEXT("Section cannot silently use the definition CDO."),Section->GetBuildInfo().GetMegaMeshDefinition()==Definition)) return false;
            if (NativeBuild)
            {
                if (!Test.TestTrue(TEXT("Runtime sections carry real modifier and variant identities from the native builder."),
                    Section->GetBuildInfo().ModifierSetHash.IsValid() && Section->GetBuildInfo().BuildVariantHash.IsValid())) return false;
            }
            else Units.Add(MakeTransformerUnit(Section,SplitStorage ? Split.Parts[Units.Num()] : SharedData));
        }
        if (!NativeBuild)
        {
            auto Context=Editor->LaunchTransformers(MoveTemp(Units),Definition,Variant);
            if (!Test.TestTrue(TEXT("The actual configured transformer pipeline launches."),Context.IsValid())) return false;
            WaitOnGameThread(*Context);
            if (!Test.TestFalse(TEXT("No variant pipeline was cancelled."),Context->bWasCancelled)) return false;
        }
        FStaticMeshCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
        auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("variant"),Variant.Name.ToString());
        if (Coastal || Region)
        {
            TArray<TSharedPtr<FJsonValue>> Inventory;
            for (auto* Section : Sections)
            {
                const auto& Binding=Section->GetBuildInfo();auto Entry=MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("build_key"),Binding.BuildKey.ToString(EGuidFormats::Digits));
                Entry->SetStringField(TEXT("modifier_hash"),Binding.ModifierSetHash.ToString(EGuidFormats::Digits));
                Entry->SetStringField(TEXT("variant_hash"),Binding.BuildVariantHash.ToString(EGuidFormats::Digits));
                Entry->SetStringField(TEXT("actor_package"),Section->GetPackage()->GetName());
                Entry->SetArrayField(TEXT("grid_cell"),Numbers({double(Binding.GridCellCoord.X),double(Binding.GridCellCoord.Y),double(Binding.GridCellCoord.Z)}));
                Entry->SetArrayField(TEXT("actor_transform"),Matrix(Section->GetActorTransform()));
                Inventory.Add(MakeShared<FJsonValueObject>(Entry));
            }
            Row->SetArrayField(TEXT("native_sections"),Inventory);
        }
        Row->SetStringField(TEXT("native_build_key"),Sections[0]->GetBuildInfo().BuildKey.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("native_actor_package"),Sections[0]->GetPackage()->GetName());
        Row->SetNumberField(TEXT("source_faces"),SourceFaces);
        ProjectWorldTerrainSectionCollection::FCollected Collected;
        if (!ProjectWorldTerrainSectionCollection::Collect(Test,*World,Bundle,Sections,Variant.Name,StorageOnly || Coastal || Canonical,SplitStorage || Coastal || (Region && Bundle.Cells.Num()>1),Collected,RequiredPhysicsPoints)) return false;
        if (Collected.RequiredPhysics.IsValid()) Row->SetObjectField(TEXT("required_physics"),Collected.RequiredPhysics);
        if (Collected.TerrainPhysicsIsolation) Row->SetObjectField(TEXT("terrain_physics_isolation"),Collected.TerrainPhysicsIsolation);
        auto& Actual=Collected.Actual; auto& Uncompensated=Collected.Uncompensated;
        auto& Components=Collected.Components; auto& PerSection=Collected.PerSection;
        if (SplitStorage || Coastal)
        {
            if (Coastal)
            {Split.Sections=Sections;if (!ProjectWorldCoastalSceneTest::BindNativeParts(Test,Bundle,Native,Partition->GetActorTransform(),Split)) return false;}
            ProjectWorldTerrainStorageFixture::CheckShared(Test,Bundle,Split,PerSection,*Row);
            if (SplitStorage) SavedSplits.Add(Variant.Name,Split);
        }
        FProjectWorldMeshTerrainSurfaceFidelity Fidelity;
        if (StorageOnly)
        {
            Test.TestFalse(TEXT("Omitting the actual storage frame cannot authenticate the same local payload."),
                ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Uncompensated,1e-4,Fidelity,Error));
        }
        const bool Covered=Region ? ProjectWorldMeshTerrainProducer::CompareCanonicalSurface(Bundle,Actual,1e-4,Fidelity,Error) :
            ProjectWorldMeshTerrainProducer::CompareRefinedSurface(Bundle,Actual,1e-4,Fidelity,Error);
        Row->SetNumberField(TEXT("actual_faces"),Actual.Num()); Row->SetNumberField(TEXT("mismatched_triangles"),Fidelity.MismatchedTriangles);
        Row->SetNumberField(TEXT("expected_triangles"),Fidelity.ExpectedTriangles);
        Row->SetNumberField(TEXT("maximum_whole_field_error_m"),Fidelity.MaximumErrorMeters); Row->SetStringField(TEXT("diagnostic"),Error);
        Row->SetArrayField(TEXT("components"),Components); Variants.Add(MakeShared<FJsonValueObject>(Row));
        Test.TestTrue(FString::Printf(TEXT("Configured variant %s has positive complete canonical field fidelity: %s; faces=%d mismatches=%d max_m=%.17g"),
            *Variant.Name.ToString(),*Error,Actual.Num(),Fidelity.MismatchedTriangles,Fidelity.MaximumErrorMeters),Actual.Num()>0 && Covered);
        if (!Gameplay && Variant.Name==TEXT("nanite"))
        {
            const FString Option=FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_BASIN_ENCODING_PRECISION"));
            if (!Option.IsEmpty())
            {
                int32 Precision=0;auto Metrics=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("encoding_candidate"),Metrics);
                Receipt->SetStringField(TEXT("encoding_scope"),TEXT("Fast transient precision/attribute/GPU-field check from the permanent typed fixture and actual configured transformer; no saved candidate, native WP builder, gameplay or rendered acceptance."));
                if (Test.TestTrue(TEXT("Fast encoding comparison uses an explicit bounded precision and one actual visual section."),
                    LexTryParseString(Precision,*Option) && Precision>=4 && Precision<=16 && Sections.Num()==1 && Sections[0]->GetMeshComponents().Num()==1))
                {
                    auto Candidate=ProjectWorldBasinSceneTest::CompileEncodingCandidate(Test,Sections[0]->GetMeshComponents()[0]->GetStaticMesh(),Precision,*Metrics);
                }
            }
        }
    }
    Receipt->SetArrayField(TEXT("variants"),Variants);
    if (Region && !Test.HasAnyErrors())
    {
        TArray<TSharedPtr<FJsonValue>> Cells;
        for (const auto& Cell : Bundle.Cells) Cells.Add(MakeShared<FJsonValueString>(Cell.CellId));
        Receipt->SetArrayField(TEXT("selected_complete_cells"),Cells);
        if (!ProjectWorldRegionDomain::ApplyStructures(Test,*World,*Document,Bundle,Scratch.Mount,Scratch.Packages,RegionStructures) ||
            !ProjectWorldRegionDomain::ObserveStructures(Test,*World,Bundle,RegionStructures,*Receipt,false)) return false;
        if (!RegionStructures.Feature.IsEmpty())
        {
            const FString MapFile=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetMapPackageExtension());
            if (!Test.TestTrue(TEXT("The combined admitted structure saves only after original Terrain proof."),FEditorFileUtils::SaveMap(World,MapFile) &&
                FProjectWorldCanonicalLoader::ComputeFileSha256(MapFile,MapSha256))) return false;
        }
    }
    if (Coastal && !Test.HasAnyErrors())
    {
        if (!ProjectWorldCoastalSceneTest::InstallUpper(Test,*World,*Document,Bundle,Scratch.Mount,Scratch.Packages,*Receipt)) return false;
        const FString MapFile=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetMapPackageExtension());
        if (!Test.TestTrue(TEXT("The joined candidate saves only after lower-field native proof."),FEditorFileUtils::SaveMap(World,MapFile) &&
            FProjectWorldCanonicalLoader::ComputeFileSha256(MapFile,MapSha256))) return false;
    }
    Test.TestEqual(TEXT("Every configured collision/nanite/fallback variant was measured."),Variants.Num(),3);
    FString After; Test.TestTrue(TEXT("Accepted shared definition bytes remain unchanged."),
        FProjectWorldCanonicalLoader::ComputeFileSha256(SharedFile,After) && After==SharedBefore);
    FString Text; FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Text));
    Test.TestTrue(TEXT("Complete actual section outputs and transforms are retained."),FFileHelper::SaveStringToFile(Text,*(Scratch.Root/TEXT("sections.json"))));
    if (Test.HasAnyErrors()) return false;
    const FString Filename=FPackageName::LongPackageNameToFilename(Scratch.PackageName,FPackageName::GetMapPackageExtension());
    if (NativeBuild) {FString Actual;Test.TestTrue(TEXT("Inspection preserves the exact native-built map dependency."),
        FProjectWorldCanonicalLoader::ComputeFileSha256(Filename,Actual) && Actual==MapSha256);}
    FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone;
    if (!NativeBuild) Test.TestTrue(TEXT("The manual section candidate saves only to its scratch scene."),
        Canonical ? FEditorFileUtils::SaveMap(World,Filename) : UPackage::SavePackage(Package,World,*Filename,Save));
    Scratch.Packages.AddUnique(Scratch.PackageName);
    for (auto* External : World->GetPackage()->GetExternalPackages()) Scratch.Packages.AddUnique(External->GetName());
    for (const auto& Name : Scratch.Packages)
    {
        const FString File=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Name,
            Name==Scratch.PackageName ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension()));
        const bool Inside=FPaths::IsUnderDirectory(File,Scratch.Directory),Exists=IFileManager::Get().FileExists(*File);
        if (!Test.TestTrue(FString::Printf(TEXT("Saved package is confined and present: %s inside=%d exists=%d file=%s"),*Name,Inside,Exists,*File),Inside && Exists))
        {ProjectWorldBasinSceneTest::DescribePackage(Test,Name);return false;}
    }
    if ((SplitStorage || Canonical) && !Test.HasAnyErrors())
    {
        if (Region && !ProjectWorldRegionWater::Observe(Test,*World,Bundle,RegionWater,*Receipt,false)) return false;
        auto Reload=MakeShared<FJsonObject>();
        const bool Replayed=ProjectWorldTerrainStorageFixture::ReplaySaved(Test,*World,Filename,Bundle,SavedSplits,
            [&](){DestroyOwnedWorld();Partition=nullptr;Editor=nullptr;Definition=nullptr;Package=nullptr;},*Reload,RequiredPhysicsPoints,!Canonical,
            Canonical ? TFunction<bool(UWorld&,FJsonObject&)>([&](UWorld& Saved,FJsonObject& Evidence)
                {return (!Region || ProjectWorldRegionWater::Observe(Test,Saved,Bundle,RegionWater,Evidence,true)) &&
                    (!Region || ProjectWorldRegionDomain::ObserveStructures(Test,Saved,Bundle,RegionStructures,Evidence,true)) &&
                    ProjectWorldTerrainCanonicalFixture::Capture(Test,Saved,Bundle,*Document,Scratch.Root,Evidence,Region);}) : TFunction<bool(UWorld&,FJsonObject&)>(),
            Region ? MakeArrayView(Scratch.Packages) : TConstArrayView<FString>(),Region);
        Receipt->SetObjectField(TEXT("saved_section_replay"),Reload);
        FString Updated; FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Updated));
        Test.TestTrue(TEXT("Fresh saved-section replay retains its separate actual output evidence."),
            FFileHelper::SaveStringToFile(Updated,*(Scratch.Root/TEXT("sections.json"))));
        return Replayed;
    }
    if (Gameplay && !Test.HasAnyErrors())
    {
        TArray<TWeakObjectPtr<UObject>> Originals={World};
        if (JointWater) ProjectWorldBasinSceneTest::TrackWaterInstances(World,Originals);
        for (TActorIterator<UE::MeshPartition::ACompiledSection> It(World);It;++It)
        {
            Originals.Add(*It);
            for (const auto& Component : It->GetMeshComponents()) Originals.Add(Component->GetStaticMesh());
            for (const auto& Component : It->GetCollisionComponents()) Originals.Add(Component.Get());
        }
        DestroyOwnedWorld(); Partition=nullptr;Editor=nullptr;Definition=nullptr;
        TArray<UPackage*> Loaded;
        for (const auto& Name : Scratch.Packages) if (auto* Existing=FindPackage(nullptr,*Name)) Loaded.Add(Existing);
        UPackageTools::FUnloadPackageParams Params(Loaded);Params.bResetTransBuffer=false;
        if (!Test.TestTrue(TEXT("All original section scene packages unload before reopening."),UPackageTools::UnloadPackages(Params))) return false;
        Package=nullptr;
        for (const auto& Original : Originals)
            if (!Test.TestFalse(TEXT("Reload cannot reuse original world, section, mesh or collision instances."),Original.IsValid())) return false;
        ProjectWorldBasinSceneTest::FInput Scene;
        Scene.Root=Scratch.Root;Scene.Mount=MoveTemp(Scratch.Mount);Scene.Directory=Scratch.Directory;
        Scene.PackageName=MoveTemp(Scratch.PackageName);Scene.Bundle=Bundle;Scene.Pawn=Pawn;Scene.Expected=Receipt;Scene.Start=Start;
        Scene.Packages=MoveTemp(Scratch.Packages);
        Scene.PluginManaged=Scratch.PluginManaged;
        Scene.MapSha256=MapSha256;
        return Coastal ? ProjectWorldCoastalSceneTest::Queue(Test,MoveTemp(Scene),*Document) : ProjectWorldBasinSceneTest::Queue(Test,MoveTemp(Scene));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBasinSectionsTest,
    "Project.World.Realization.MeshTerrain.IdentifiedBasinSections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBasinSectionsTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,false);
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldCanonicalAuthoredDryTest,
    "Project.World.Realization.MeshTerrain.CanonicalAuthoredDry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
void FProjectWorldCanonicalAuthoredDryTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{
    if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE")).IsEmpty())
    {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}
}
bool FProjectWorldCanonicalAuthoredDryTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,false,false,nullptr,false,true);
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldCanonicalAuthoredDryTest,
    "Project.World.Realization.MeshTerrain.CanonicalAuthoredDry","[Slow][Integration][World]")

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldCanonicalRegionTest,
    "Project.World.Realization.MeshTerrain.CanonicalRegion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
void FProjectWorldCanonicalRegionTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{
    if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE")).IsEmpty())
    {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}
}
bool FProjectWorldCanonicalRegionTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,false,false,nullptr,false,true,true);
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldCanonicalRegionTest,
    "Project.World.Realization.MeshTerrain.CanonicalRegion","[Slow][Integration][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldContinuationStorageFrameTest,
    "Project.World.Realization.MeshTerrain.ContinuationStorageFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldContinuationStorageFrameTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,true);
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldContinuationStorageFrameTest,
    "Project.World.Realization.MeshTerrain.ContinuationStorageFrame","[Slow][Integration][World]")
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldSectionStorageSplitTest,
    "Project.World.Realization.MeshTerrain.SectionStorageSplit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldSectionStorageSplitTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,true,true);
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldSectionStorageSplitTest,
    "Project.World.Realization.MeshTerrain.SectionStorageSplit","[Slow][Integration][World]")
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldCoastalCookSupportTest,
    "Project.World.Realization.MeshTerrain.CoastalCookSupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldCoastalCookSupportTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,true,true,TEXT("coastal_cook_mesh_fixture.py"));
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldCoastalCookSupportTest,
    "Project.World.Realization.MeshTerrain.CoastalCookSupport","[Slow][Integration][World]")
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FProjectWorldCoastalJoinedTest,
    "Project.World.Realization.MeshTerrain.CoastalJoined",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
void FProjectWorldCoastalJoinedTest::GetTests(TArray<FString>& Names,TArray<FString>& Parameters) const
{
    if (!FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_WORLD_COMPOSITE_TERRAIN_FIXTURE")).IsEmpty())
    {Names.Add(TEXT(""));Parameters.Add(TEXT(""));}
}
bool FProjectWorldCoastalJoinedTest::RunTest(const FString&)
{
    return RunSectionConsumer(*this,false,false,nullptr,true);
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldCoastalJoinedTest,
    "Project.World.Realization.MeshTerrain.CoastalJoined","[Slow][Integration][World]")
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectWorldBasinSectionsTest,
    "Project.World.Realization.MeshTerrain.IdentifiedBasinSections","[Slow][Integration][World]")
#endif
