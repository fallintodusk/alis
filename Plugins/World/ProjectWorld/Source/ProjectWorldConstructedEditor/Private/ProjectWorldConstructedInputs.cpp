#include "ProjectWorldConstructedInputs.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldGeneratedActors.h"
#include "ProjectWorldLayerProducerRegistry.h"
#include "ProjectWorldVerify.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldConstructedRealization
{
    namespace
    {
        void AppendDependencyToken(FString& Text,const FString& Token)
        { Text+=FString::Printf(TEXT("%d:"),Token.Len())+Token; }
        void AppendDependencyNumber(FString& Text,double Value)
        { AppendDependencyToken(Text,FString::Printf(TEXT("%.17g"),Value)); }
        FString DependencyDigest(const FString& Text,FDependencyInputs& Work)
        {
            FTCHARToUTF8 Encoded(*Text); Work.SemanticIdentityBytes+=Encoded.Length();
            return ProjectWorldProjection::HashText(Text);
        }
    }
    bool Settings(const FString& Json,FSettings& Out,FString& Error)
    {
        static const TCHAR* Fields[]={TEXT("maximum_join_step_m"),TEXT("nanite"),TEXT("collision"),TEXT("navigation")};
        TSharedPtr<FJsonObject> Object;
        if (!ProjectWorldLayerProducerRegistry::ParseExactSettings(Json,MakeArrayView(Fields),Object,Error)) return false;
        FSettings Candidate; bool Nanite=false; FString Collision,Navigation;
        const auto Step=Object->TryGetField(TEXT("maximum_join_step_m"));
        if (!Step || Step->Type!=EJson::Number || !Step->TryGetNumber(Candidate.MaximumJoinStepMeters) ||
            !FMath::IsFinite(Candidate.MaximumJoinStepMeters) || Candidate.MaximumJoinStepMeters<0 || Candidate.MaximumJoinStepMeters>.001 ||
            !Object->HasTypedField<EJson::Boolean>(TEXT("nanite")) || !Object->TryGetBoolField(TEXT("nanite"),Nanite) || !Nanite ||
            !Object->TryGetStringField(TEXT("collision"),Collision) || Collision!=TEXT("complex_as_simple") ||
            !Object->TryGetStringField(TEXT("navigation"),Navigation) || Navigation!=TEXT("physical_support"))
        { Error=TEXT("Structural settings require bounded native fidelity, Nanite, real collision and navigation support."); return false; }
        Out=Candidate; return true;
    }
    FString UnitIdentity(const FString& FeatureId,const FString& CellId)
    {
        return ProjectWorldGeneratedGeometry::StableGuid(FString::Printf(TEXT("%d:%s%d:%s"),
            FeatureId.Len(),*FeatureId,CellId.Len(),*CellId)).ToString(EGuidFormats::Digits);
    }
    bool ValidateSourcePolicy(const FProjectWorldCanonicalConstructedSurface& Surface,TArray<int32>& RegionBySource,FString& Error)
    {
        RegionBySource.Reset();
        if (!Surface.OperationalPolicy.IsSet() || Surface.FaceRegions.IsEmpty() || Surface.FaceRegions.Num()>64 ||
            Surface.AdmissionHash.Len()!=64 || Surface.SourceSnapshots.IsEmpty() || Surface.SourceProviderIds.IsEmpty())
        { Error=TEXT("Structural input requires admitted policy, complete face regions and source provenance."); return false; }
        const auto& Policy=Surface.OperationalPolicy.GetValue();
        if (Policy.MaximumTriangles<1 || Policy.MaximumTriangles>4096 || Surface.Triangles.IsEmpty() || Surface.Triangles.Num()>Policy.MaximumTriangles ||
            !FMath::IsFinite(Policy.MaximumOrdinaryGradient) || Policy.MaximumOrdinaryGradient<=0 ||
            !FMath::IsFinite(Policy.MaximumGangwayGrade) || Policy.MaximumGangwayGrade<=0 ||
            !FMath::IsFinite(Policy.MaximumCrossSlope) || Policy.MaximumCrossSlope<=0 ||
            !FMath::IsFinite(Policy.MaximumNumericErrorMeters) || Policy.MaximumNumericErrorMeters<=0)
        { Error=TEXT("Structural source field exceeds its declared policy capacity."); return false; }
        TArray<int32> Candidate; Candidate.Init(INDEX_NONE,Surface.Triangles.Num());
        for (int32 I=0;I<Surface.FaceRegions.Num();++I)
        {
            const auto& Region=Surface.FaceRegions[I];
            if (Region.SourceTriangles.IsEmpty() || Region.SourceIds.IsEmpty() ||
                Region.AxisStart.ContainsNaN() || Region.AxisEnd.ContainsNaN() ||
                (Region.Role!=TEXT("fixed_pier") && Region.Role!=TEXT("floating_deck") && Region.Role!=TEXT("gangway") &&
                Region.Role!=TEXT("inferred_apron") && Region.Role!=TEXT("inferred_gate_apron")))
            { Error=TEXT("Structural face region is empty or unsupported."); return false; }
            for (const auto& Id : Region.SourceIds) if (!Surface.SourceProviderIds.Contains(Id))
            { Error=TEXT("Structural face region has an unbound source."); return false; }
            for (int32 Source : Region.SourceTriangles)
            {
                if (!Candidate.IsValidIndex(Source) || Candidate[Source]!=INDEX_NONE)
                { Error=TEXT("Structural face regions overlap or reference missing source faces."); return false; }
                Candidate[Source]=I;
            }
            if (Region.Role==TEXT("gangway"))
            {
                const auto Axis=Region.AxisEnd-Region.AxisStart;
                if (Region.AxisSourceId.IsEmpty() || !Region.SourceIds.Contains(Region.AxisSourceId) || Axis.ContainsNaN() ||
                    !FMath::IsFinite(Axis.Length()) || Axis.Length()==0)
                { Error=TEXT("Stored gangway has no unique finite declared axis."); return false; }
            }
        }
        if (Candidate.Contains(INDEX_NONE)) { Error=TEXT("Structural face regions omit source faces."); return false; }
        RegionBySource=MoveTemp(Candidate); return true;
    }
    bool DependencyInputs(const FProjectWorldCanonicalBundle& Bundle,const FProjectWorldRealizationLayer& Layer,
        FDependencyInputs& Out,FString& Error)
    {
        Out={}; Error.Reset(); FDependencyInputs Candidate; FSettings Options;
        if (!Settings(Layer.NormalizedSettings,Options,Error)) return false;
        Candidate.Options=Options;
        if (Bundle.EngineGeoreferenceOriginMeters.ContainsNaN() || !FMath::IsFinite(Bundle.HeightOriginMeters))
        { Error=TEXT("Structural dependency frame is nonfinite."); return false; }
        TSet<FString> Cells;
        for (const auto& Cell : Bundle.Cells)
        {
            if (Cell.CellId.IsEmpty() || Cells.Contains(Cell.CellId) || Cell.Bounds.ContainsNaN() || Cell.Bounds.Z<=Cell.Bounds.X || Cell.Bounds.W<=Cell.Bounds.Y)
            { Error=TEXT("Structural canonical cell identity or bounds are invalid."); return false; }
            Cells.Add(Cell.CellId);
        }
        TMap<FString,FString> SourceDigests;
        for (const auto& Pair : Bundle.Features)
        {
            if (Pair.Value.FeatureClass!=TEXT("constructed_surface")) continue;
            const auto& Feature=Pair.Value; TMap<FString,FString> Represented; bool HasOwner=false;
            for (const auto& Representation : Feature.Representations)
            {
                const bool Authoritative=Representation.CellId==Feature.OwnerCellId;
                const FString Kind=Authoritative ? TEXT("authoritative_constructed_surface") : TEXT("constructed_surface_reference");
                if (Representation.Kind!=Kind || Represented.Contains(Representation.CellId) || !Cells.Contains(Representation.CellId))
                { Error=TEXT("Structural representations have an unknown, duplicated or contradictory cell declaration."); return false; }
                Represented.Add(Representation.CellId,Representation.Kind); HasOwner|=Authoritative;
            }
            if (!HasOwner) {Error=TEXT("Structural source has no authoritative cell representation."); return false;}
            for (const auto& Cell : Bundle.Cells)
            {
                int32 OwnedCount=0,ReferenceCount=0;
                for (const auto& Id : Cell.OwnedFeatureIds) OwnedCount+=Id==Pair.Key;
                for (const auto& Id : Cell.ReferencedFeatureIds) ReferenceCount+=Id==Pair.Key;
                const bool Declared=Represented.Contains(Cell.CellId),Authoritative=Cell.CellId==Feature.OwnerCellId;
                if (OwnedCount!=(Declared && Authoritative ? 1 : 0) || ReferenceCount!=(Declared && !Authoritative ? 1 : 0))
                { Error=TEXT("Structural representation and owner/reference manifests disagree."); return false; }
            }
            FString Identity; TArray<FProjectWorldSurfaceTriangle> Field; TArray<int32> Regions;
            if (Pair.Key!=Feature.FeatureId || !ProjectWorldConstructedSurface::Read(Bundle,Feature,Field,Identity,Error) ||
                !ValidateSourcePolicy(Feature.ConstructedSurface.GetValue(),Regions,Error)) return false;
            Candidate.ValidatedSourceFaces+=Field.Num(); Candidate.Fields.Add(Pair.Key,MoveTemp(Field));
            FString SourceText=TEXT("constructed_semantic_source_v1"); AppendDependencyToken(SourceText,Identity);
            const auto& Surface=Feature.ConstructedSurface.GetValue(); const auto& Policy=Surface.OperationalPolicy.GetValue();
            for (double Value : {double(Policy.MaximumTriangles),Policy.MaximumOrdinaryGradient,Policy.MaximumGangwayGrade,
                Policy.MaximumCrossSlope,Policy.MaximumNumericErrorMeters}) AppendDependencyNumber(SourceText,Value);
            for (const auto& Region : Surface.FaceRegions)
            {
                AppendDependencyToken(SourceText,Region.Role); AppendDependencyNumber(SourceText,Region.SourceIds.Num());
                for (const auto& Id : Region.SourceIds) AppendDependencyToken(SourceText,Id);
                AppendDependencyToken(SourceText,Region.AxisSourceId);
                for (double Value : {Region.AxisStart.X,Region.AxisStart.Y,Region.AxisEnd.X,Region.AxisEnd.Y}) AppendDependencyNumber(SourceText,Value);
                AppendDependencyNumber(SourceText,Region.SourceTriangles.Num());
                for (int32 Ordinal : Region.SourceTriangles) AppendDependencyNumber(SourceText,Ordinal);
            }
            SourceDigests.Add(Pair.Key,DependencyDigest(SourceText,Candidate));
        }
        for (const auto& Cell : Bundle.Cells)
        {
            FString CellText=TEXT("constructed_semantic_cell_v1");
            for (const auto& Text : {Cell.CellId,Bundle.GridId,Bundle.CanonicalCrs,Bundle.VerticalDatum,Bundle.CoordinateTransform}) AppendDependencyToken(CellText,Text);
            for (double Value : {Cell.Bounds.X,Cell.Bounds.Y,Cell.Bounds.Z,Cell.Bounds.W,Bundle.EngineGeoreferenceOriginMeters.X,
                Bundle.EngineGeoreferenceOriginMeters.Y,Bundle.HeightOriginMeters,Options.MaximumJoinStepMeters}) AppendDependencyNumber(CellText,Value);
            TArray<FString> Ids=Cell.OwnedFeatureIds; for (const auto& Id : Cell.ReferencedFeatureIds) Ids.AddUnique(Id); Ids.Sort();
            for (const auto& Id : Ids) if (const auto* Digest=SourceDigests.Find(Id))
            { AppendDependencyToken(CellText,UnitIdentity(Id,Cell.CellId)); AppendDependencyToken(CellText,*Digest); }
            Candidate.CellHashes.Add(Cell.CellId,DependencyDigest(CellText,Candidate));
        }
        Out=MoveTemp(Candidate); return true;
    }
}
