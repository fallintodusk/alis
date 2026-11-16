#include "ProjectWorldMeshTerrainRegionalSupport.h"
#include "ProjectWorldMeshTerrainRegionalScene.h"
#include "ProjectWorldConstructedSurface.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldRegionalSupportTest
{
    namespace
    {
        bool Vector(const TSharedPtr<FJsonValue>& Value,FVector& Out)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
            if (!Value || !Value->TryGetArray(Values) || Values->Num()!=3) return false;
            for (int32 I=0;I<3;++I) if (!(*Values)[I] || (*Values)[I]->Type!=EJson::Number ||
                !(*Values)[I]->TryGetNumber(Out[I]) || !FMath::IsFinite(Out[I])) return false;
            return true;
        }
        bool Faces(const FJsonObject& Row,const FProjectWorldCanonicalBundle& Bundle,bool Canonical,
            TArray<FProjectWorldSurfaceTriangle>& Out,FString& Error)
        {
            const TArray<TSharedPtr<FJsonValue>> *Vertices=nullptr,*Triangles=nullptr;
            if (!Row.TryGetArrayField(Canonical ? TEXT("vertices_canonical") : TEXT("world_vertices_cm"),Vertices) ||
                !Row.TryGetArrayField(TEXT("triangles"),Triangles) || Vertices->IsEmpty() || Triangles->IsEmpty() || Triangles->Num()>ProjectWorldTerrainSurface::MaximumFieldTriangles)
            {Error=TEXT("Saved physical support requires complete bounded vertex/index fields.");return false;}
            TArray<FVector> Points;
            for (const auto& Value : *Vertices)
            {
                FVector P;if (!Vector(Value,P)) {Error=TEXT("Saved support has a malformed vertex.");return false;}
                Points.Add(Canonical ? FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,P) : P);
            }
            for (const auto& Value : *Triangles)
            {
                FVector Ids;if (!Vector(Value,Ids)) {Error=TEXT("Saved support has malformed indices.");return false;}
                FProjectWorldSurfaceTriangle Face;
                for (int32 I=0;I<3;++I)
                {
                    if (Ids[I]<0 || Ids[I]>=Points.Num() || Ids[I]!=FMath::FloorToDouble(Ids[I])) {Error=TEXT("Support index escapes its saved field.");return false;}
                    Face.Points[I]=Points[int32(Ids[I])];
                }
                Out.Add(Face);
            }
            return true;
        }
    }
    bool FSupport::Read(const ProjectWorldRegionalSceneTest::FScene& Scene,FString& Error)
    {
        Ground.Reset();Structures.Reset();Water.Reset();
        for (const auto& Variant : Scene.Expected->GetArrayField(TEXT("variants")))
            if (Variant->AsObject()->GetStringField(TEXT("variant"))==TEXT("fallback"))
                for (const auto& Component : Variant->AsObject()->GetArrayField(TEXT("components")))
                    if (Component->AsObject()->GetStringField(TEXT("kind"))==TEXT("gpu_fallback") &&
                        !Faces(*Component->AsObject(),Scene.Bundle,false,Ground,Error)) return false;
        for (const auto& Actor : Scene.Expected->GetObjectField(TEXT("regional_water_before"))->GetArrayField(TEXT("actors")))
            if (!Faces(*Actor->AsObject(),Scene.Bundle,true,Water,Error)) return false;
        const TSharedPtr<FJsonObject>* Bodies=nullptr;
        if (Scene.Expected->TryGetObjectField(TEXT("regional_structures_before"),Bodies))
            for (const auto& Actor : (*Bodies)->GetArrayField(TEXT("actors")))
                if (!Faces(*Actor->AsObject(),Scene.Bundle,true,Structures,Error)) return false;
        if (Ground.IsEmpty() || Water.IsEmpty()) {Error=TEXT("Regional gameplay cannot invent absent Terrain or ordinary Water support.");return false;}
        int64 Expected=0;
        for (const auto& Cell:Scene.Bundle.Cells)
        {
            const auto& T=Cell.Terrain;
            if (T.SamplesX<2 || T.SamplesY<2) {Error=TEXT("Regional Ground has no authenticated cell topology.");return false;}
            Expected+=2LL*(T.SamplesX-1)*(T.SamplesY-1);
            if (Expected>ProjectWorldTerrainSurface::MaximumFieldTriangles) {Error=TEXT("Regional Ground exceeds the finite observation budget.");return false;}
            for (const auto& Q:T.RefinedQuads)
            {
                if (Q.Triangles.Num()<2) {Error=TEXT("Regional Ground has an incomplete replacement quad.");return false;}
                Expected+=Q.Triangles.Num()-2;
                if (Expected>ProjectWorldTerrainSurface::MaximumFieldTriangles) {Error=TEXT("Regional Ground exceeds the finite observation budget.");return false;}
            }
        }
        if (Expected<1 || Ground.Num()!=Expected) {Error=TEXT("Saved Ground face census differs from authenticated final cell topology.");return false;}
        GroundCapacity=static_cast<int32>(Expected);
        Scene.Test.AddInfo(FString::Printf(TEXT("RegionalGroundSupport faces=%d capacity=%d"),Ground.Num(),GroundCapacity));
        return ProjectWorldTerrainSurface::ValidateField(Ground,GroundCapacity,Error) &&
            (Structures.IsEmpty() || ProjectWorldConstructedSurface::ValidateField(Structures,Error)) &&
            ProjectWorldConstructedSurface::ValidateField(Water,Error);
    }
    bool FSupport::Sample(const FVector& Position,FSample& Out,FString& Error) const
    {
        Out=FSample();double GroundHeight=0,BodyHeight=0;
        const auto Base=ProjectWorldTerrainSurface::TrySampleField(Ground,FVector2D(Position),GroundCapacity,GroundHeight,Error);
        if (Base!=ProjectWorldConstructedSurface::ESampleResult::Sampled)
        {if (Error.IsEmpty()) Error=TEXT("The route leaves actual complete-cell Terrain support.");return false;}
        Out.Height=GroundHeight;
        if (!Structures.IsEmpty())
        {
            const auto Body=ProjectWorldConstructedSurface::TrySample(Structures,FVector2D(Position),BodyHeight,Error);
            if (Body==ProjectWorldConstructedSurface::ESampleResult::Invalid) return false;
            if (Body==ProjectWorldConstructedSurface::ESampleResult::Sampled && BodyHeight>=GroundHeight)
            {Out.Height=BodyHeight;Out.Structure=true;}
        }
        const auto Wet=ProjectWorldConstructedSurface::TrySample(Water,FVector2D(Position),Out.WaterHeight,Error);
        if (Wet==ProjectWorldConstructedSurface::ESampleResult::Invalid) return false;
        Out.Wet=Wet==ProjectWorldConstructedSurface::ESampleResult::Sampled;
        if (Out.Wet && Out.Height<=Out.WaterHeight)
        {Error=TEXT("The selected route's actual top support is submerged by ordinary Water.");return false;}
        return true;
    }
    bool FSupport::FloorEnvelope(const FVector& Position,double Radius,double FloorDistance,ESupportLayer Owner,double& Bound,FString& Error) const
    {
        if (Position.ContainsNaN() || !FMath::IsFinite(Radius) || Radius<=0 || !FMath::IsFinite(FloorDistance) || FloorDistance<0)
        {Error=TEXT("The floor envelope requires the actual finite capsule and native floor distance.");return false;}
        double Gradient=0;int32 Faces=0;
        const auto& Field=Owner==ESupportLayer::Structure ? Structures : Ground;
        for (const auto& Face : Field)
        {
            FBox2D Box(ForceInit);for (const auto& P : Face.Points) Box+=FVector2D(P);
            if (!Box.Intersect(FBox2D(FVector2D(Position)-FVector2D(Radius),FVector2D(Position)+FVector2D(Radius)))) continue;
            const FVector Point(Position.X,Position.Y,0);
            const auto Flatten=[](const FVector& P){return FVector(P.X,P.Y,0);};
            if (FVector::DistSquared(Point,FMath::ClosestPointOnTriangleToPoint(Point,Flatten(Face.Points[0]),Flatten(Face.Points[1]),Flatten(Face.Points[2])))>Radius*Radius) continue;
            const FVector Normal=FVector::CrossProduct(Face.Points[1]-Face.Points[0],Face.Points[2]-Face.Points[0]);
            if (Normal.ContainsNaN() || Normal.Z==0) {Error=TEXT("An actual local support face has no finite grade.");return false;}
            Gradient=FMath::Max(Gradient,FVector2D(Normal).Length()/FMath::Abs(Normal.Z));++Faces;
        }
        if (!Faces) {Error=TEXT("The capsule envelope has no authenticated local support faces.");return false;}
        Bound=FloorDistance+Radius*(FMath::Sqrt(1+Gradient*Gradient)-1)+.01;
        return FMath::IsFinite(Bound);
    }
    bool ReadRoutes(const FJsonObject& Input,TArray<FRoute>& Routes,FString& Error)
    {
        Routes.Reset();const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if (!Input.TryGetArrayField(TEXT("routes"),Values) || Values->IsEmpty() || Values->Num()>8)
        {Error=TEXT("Regional gameplay requires one to eight explicit supported routes.");return false;}
        for (const auto& Value : *Values)
        {
            const TSharedPtr<FJsonObject>* Row=nullptr;const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;FRoute Route;
            if (!Value || !Value->TryGetObject(Row) || !(*Row)->HasTypedField<EJson::String>(TEXT("id")) || !(*Row)->TryGetStringField(TEXT("id"),Route.Id) || Route.Id.IsEmpty() ||
                !(*Row)->HasTypedField<EJson::Number>(TEXT("width_m")) || !(*Row)->TryGetNumberField(TEXT("width_m"),Route.WidthMeters) || !FMath::IsFinite(Route.WidthMeters) || Route.WidthMeters<=0 ||
                !(*Row)->TryGetArrayField(TEXT("points_world_cm"),Points) || Points->Num()<2 || Points->Num()>512)
            {Error=TEXT("A regional route has malformed identity, width or complete points.");return false;}
            double Length=0;
            for (const auto& Point : *Points)
            {
                FVector P;if (!Vector(Point,P)) {Error=TEXT("A regional route has a nonfinite point.");return false;}
                if (!Route.Points.IsEmpty()) Length+=FVector2D(P-Route.Points.Last()).Length();Route.Points.Add(P);
            }
            if (Length<100 || Length>200000) {Error=TEXT("A regional route must be between one metre and two kilometres.");return false;}
            Routes.Add(MoveTemp(Route));
        }
        return true;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRegionalSupportTest,"Project.World.Realization.MeshTerrain.CanonicalRegionSupport",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectWorldRegionalSupportTest::RunTest(const FString&)
{
    using namespace ProjectWorldRegionalSupportTest;
    FSupport Support;FString Error;FSample Sample;
    Support.Ground={{{FVector(0,0,0),FVector(100,0,0),FVector(0,100,0)}},{{FVector(100,0,0),FVector(100,100,0),FVector(0,100,0)}}};
    Support.Structures=Support.Ground;for (auto& Face : Support.Structures) for (auto& P : Face.Points) P.Z=100;
    Support.Water=Support.Ground;for (auto& Face : Support.Water) for (auto& P : Face.Points) P.Z=25+.5*P.X;
    TestTrue(TEXT("Separate elevated support is sampled above actual varying Water."),Support.Sample(FVector(50,25,0),Sample,Error) && Sample.Structure && Sample.Wet && Sample.Height==100 && Sample.WaterHeight==50);
    const auto Structures=Support.Structures;Support.Structures.Reset();
    TestFalse(TEXT("Missing structural support cannot substitute submerged underlying Terrain."),Support.Sample(FVector(50,25,0),Sample,Error));
    Support.Structures=Structures;Support.Ground.Reset();
    TestFalse(TEXT("A structure cannot conceal missing complete-cell Ground authority."),Support.Sample(FVector(50,25,0),Sample,Error));
    FSupport Junction;
    Junction.Ground={{{FVector(0,0,0),FVector(0,-3000,0),FVector(3000,0,0)}},
        {{FVector(3000,3000,990),FVector(0,0,0),FVector(3000,0,0)}}};
    double Bound=0;
    TestTrue(TEXT("Captured Kazan junction neighbours produce the local actual-capsule envelope."),Junction.FloorEnvelope(FVector(1000,0,0),23,2.4,ESupportLayer::Ground,Bound,Error));
    TestTrue(TEXT("The captured 3.3700 cm native contact fits neighbouring support slopes although its single-face 3.2983 cm estimate failed."),3.3700361027304098<=Bound && 3.3700361027304098>3.298263532064921);
    TestFalse(TEXT("The local capsule envelope still rejects a 10 cm foot separation."),10.<=Bound);
    FSupport Buried;
    Buried.Ground={{{FVector(-100,-100,-10000),FVector(100,-100,-4000),FVector(-100,100,-10000)}},
        {{FVector(100,-100,-4000),FVector(100,100,-4000),FVector(-100,100,-10000)}}};
    Buried.Structures=Buried.Ground;for (auto& Face : Buried.Structures) for (auto& P : Face.Points) P.Z=100;
    TestTrue(TEXT("The flat structural deck has a finite actual-capsule envelope above steep buried Ground."),Buried.FloorEnvelope(FVector(0,0,190),23,2.4,ESupportLayer::Structure,Bound,Error));
    TestFalse(TEXT("A 10 cm gap above a flat deck cannot borrow the slope of buried Ground."),10.<=Bound);
    TestEqual(TEXT("A flat structural owner retains only the native floor distance and existing numeric margin."),Bound,2.41);
    return !HasAnyErrors();
}
#endif
