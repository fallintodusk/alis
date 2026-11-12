#include "ProjectWorldBuildingAssembly.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldAuthoredOverlay.h"
#include "ProjectWorldGeneratedActors.h"

namespace ProjectWorldBuildingAssembly
{
    bool Prepare(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldCanonicalCell& Cell,
        const FProjectWorldAuthoredOverlaySet& Overlays,const FString& FeatureId,
        const ProjectBuildingAssembly::FKit& Kit,FPrepared& Out,FString& Error,bool bBuildRemainingMassing)
    {
        Out={};FPrepared Candidate;const auto* Feature=Bundle.Features.Find(FeatureId);
        const auto Refuse=[&](const TCHAR* Reason){Error=Reason;return false;};
        if (!Feature || Feature->FeatureClass!=TEXT("building") || Feature->OwnerCellId!=Cell.CellId ||
            !Cell.OwnedFeatureIds.Contains(FeatureId) || Feature->BuildingVolumes.Num()!=1 ||
            Feature->CivilGrade.IsSet() || Feature->ConstructedSurface.IsSet())
            return Refuse(TEXT("Prototype requires one ordinary owned building volume without a civil-grade exception."));
        for (const auto& Overlay : Overlays.Overlays)
            if (Overlay.Anchor.Kind==EProjectWorldAnchorKind::Feature && Overlay.Anchor.FeatureId==FeatureId)
                return Refuse(TEXT("Selected building has protected authored content."));
        const auto& Volume=Feature->BuildingVolumes[0];
        if (Volume.MinHeightMeters!=0 || Volume.GeometryPolygons.Num()!=1 ||
            !Volume.GeometryPolygons[0].Holes.IsEmpty())
            return Refuse(TEXT("Prototype does not support elevated, multipart or holed building volumes."));
        TArray<FVector2D> Ring=Volume.GeometryPolygons[0].Outer;
        if (Ring.Num()==5 && Ring[0].Equals(Ring.Last(),1.e-8)) Ring.Pop();
        if (Ring.Num()!=4) return Refuse(TEXT("Prototype requires a four-corner rectangular volume."));
        double Signed=0;
        for (int I=0;I<4;++I)
        {
            const auto& P=Ring[I];const auto& Q=Ring[(I+1)%4];Signed+=(P.X-Ring[0].X)*(Q.Y-Ring[0].Y)-(P.Y-Ring[0].Y)*(Q.X-Ring[0].X);
            if (P.ContainsNaN() || P.X<=Cell.Bounds.X || P.X>=Cell.Bounds.Z || P.Y<=Cell.Bounds.Y || P.Y>=Cell.Bounds.W)
                return Refuse(TEXT("Building volume lacks complete single-cell support."));
        }
        if (Signed>0) Swap(Ring[1],Ring[3]);
        const double Width=FVector2D::Distance(Ring[0],Ring[1]);
        if (Width<1) return Refuse(TEXT("Building rectangle is degenerate."));
        const FVector2D U=(Ring[1]-Ring[0])/Width,V(U.Y,-U.X);
        const double Depth=FVector2D::DotProduct(Ring[3]-Ring[0],V);
        const double Inset=FMath::Max(.03,2*Bundle.CoordinateQuantizationMeters);
        if (Depth<=2*Inset || FVector2D::Distance(Ring[3],Ring[0]+V*Depth)>Inset/2 ||
            FVector2D::Distance(Ring[2],Ring[0]+U*Width+V*Depth)>Inset/2)
            return Refuse(TEXT("Building departs from the supported inscribed rectangle."));
        const FVector2D Origin=Ring[0]+(U+V)*Inset;
        const double W=Width-2*Inset,D=Depth-2*Inset;
        Candidate.Footprint={Origin,Origin+U*W,Origin+U*W+V*D,Origin+V*D};
        FBox2D Bounds(ForceInit);
        for (const auto& P : Candidate.Footprint)
        {
            for (int I=0;I<4;++I)
                if (FVector2D::CrossProduct(Ring[(I+1)%4]-Ring[I],P-Ring[I])>1.e-8)
                    return Refuse(TEXT("Fitted building is not inside its complete original volume."));
            Bounds+=P;
        }
        for (const auto& Pair : Bundle.Features)
        {
            const auto& Water=Pair.Value;
            if (Water.FeatureClass!=TEXT("water")) continue;
            FBox2D WaterBounds(ForceInit);
            for (const auto& Polygon : Water.GeometryPolygons) for (const auto& P : Polygon.Outer) WaterBounds+=P;
            for (const auto& P : Water.GeometryPoints) WaterBounds+=P;
            for (const auto& Part : Water.GeometryParts) for (const auto& P : Part) WaterBounds+=P;
            if (Water.GeometryPolygons.IsEmpty() && WaterBounds.bIsValid)
            {
                if (!FMath::IsFinite(Water.WidthMeters) || Water.WidthMeters<=0)
                    return Refuse(TEXT("Water ribbon has unknown physical width."));
                WaterBounds=WaterBounds.ExpandBy(Water.WidthMeters/2);
            }
            if (WaterBounds.bIsValid && WaterBounds.Intersect(Bounds)) return Refuse(TEXT("Prototype refuses unresolved Water overlap."));
        }
        TArray<FProjectWorldSurfaceTriangle> Surface;
        if (!ProjectWorldTerrainSurface::Triangles(Cell,Bounds,Surface,Error)) return false;
        const int Faces[2][3]={{0,2,1},{0,3,2}};
        double Minimum=MAX_dbl,Maximum=-MAX_dbl,Area=0;
        for (const auto& Face : Faces)
        {
            FProjectWorldSurfaceTriangle Foot;
            for (int I=0;I<3;++I) Foot.Points[I]=FVector(Candidate.Footprint[Face[I]],0);
            for (const auto& Ground : Surface)
            {
                TArray<FVector2D> Overlap;
                if (!ProjectWorldTerrainSurface::IntersectXY(Ground,Foot,Overlap,Error)) return false;
                const double PieceArea=ProjectWorldTerrainSurface::Area(Overlap);
                if (PieceArea<=0) continue;
                Area+=PieceArea;
                for (const auto& P : Overlap)
                {
                    double Z=0;
                    if (!ProjectWorldTerrainSurface::Height(Ground,P,Z,Error) || !FMath::IsFinite(Z))
                        return Refuse(TEXT("Unknown physical Terrain support."));
                    Minimum=FMath::Min(Minimum,Z);Maximum=FMath::Max(Maximum,Z);
                }
            }
        }
        if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) ||
            FMath::Abs(Area-W*D)>FMath::Max(1.e-7,W*D*1.e-8))
            return Refuse(TEXT("Terrain triangles do not completely cover the foundation."));
        Candidate.FeatureId=FeatureId;Candidate.TerrainArtifact=Cell.Terrain.ArtifactHash;
        Candidate.SupportMinimum=Minimum;Candidate.SupportMaximum=Maximum;Candidate.CoveredArea=Area;
        auto& Input=Candidate.Input;Input.FeatureId=TCHAR_TO_UTF8(*FeatureId);
        Input.Width=W;Input.Depth=D;Input.Height=Volume.HeightMeters;Input.SupportSpan=Maximum-Minimum;Input.Kit=Kit;
        std::string Why;
        if (!ProjectBuildingAssembly::Build(Input,Candidate.Plan,Why)) {Error=UTF8_TO_TCHAR(Why.c_str());return false;}
        const FVector UnrealOrigin=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Origin,Maximum));
        const FVector Axis=FProjectWorldCanonicalLoader::CanonicalToUnreal(Bundle,FVector(Origin+U,Maximum))-UnrealOrigin;
        Candidate.Transform=FTransform(FRotator(0,FMath::RadiansToDegrees(FMath::Atan2(Axis.Y,Axis.X)),0),UnrealOrigin);
        if (!bBuildRemainingMassing) {Out=MoveTemp(Candidate);return true;}
        TSet<FString> Selected={FeatureId};
        if (!ProjectWorldBuildingMeshBuilder::BuildCell(Bundle,Cell,Overlays,FProjectWorldBuildingSettings(),
            Candidate.RemainingMassing,Error,&Selected)) return false;
        if (Candidate.RemainingMassing.AssemblyExcludedFeatures!=TArray<FString>{FeatureId})
            return Refuse(TEXT("Selected massing exclusion differs from the assembly input."));
        Out=MoveTemp(Candidate);return true;
    }
}
