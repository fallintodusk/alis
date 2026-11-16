// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldGroundBaseParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldShoreTransitionParsing.h"
#include "Dom/JsonObject.h"

namespace ProjectWorldGroundBaseParsing
{
    namespace
    {
        using FObject=TSharedPtr<FJsonObject>;
        using FArray=TArray<TSharedPtr<FJsonValue>>;
        bool String(const FObject& O,const TCHAR* Key,FString& Out)
        {const auto V=O ? O->TryGetField(Key) : nullptr;return V && V->Type==EJson::String && V->TryGetString(Out) && !Out.IsEmpty();}
        bool Number(const TSharedPtr<FJsonValue>& V,double& Out)
        {return V && V->Type==EJson::Number && V->TryGetNumber(Out) && FMath::IsFinite(Out);}
        bool Number(const FObject& O,const TCHAR* Key,double& Out)
        {return O && Number(O->TryGetField(Key),Out);}
        bool Integer(const FObject& O,const TCHAR* Key,int32& Out)
        {double V=0;if (!Number(O,Key,V) || V<MIN_int32 || V>MAX_int32 || V!=FMath::FloorToDouble(V)) return false;Out=static_cast<int32>(V);return true;}
        bool Object(const FObject& O,const TCHAR* Key,FObject& Out)
        {const FObject* V=nullptr;if (!O || !O->TryGetObjectField(Key,V) || !V || !*V) return false;Out=*V;return true;}
        const FArray* Array(const FObject& O,const TCHAR* Key)
        {const FArray* V=nullptr;return O && O->TryGetArrayField(Key,V) ? V : nullptr;}
        bool Vector(const FObject& O,const TCHAR* Key,FVector2D& Out)
        {const auto V=Array(O,Key);return V && V->Num()==2 && Number((*V)[0],Out.X) && Number((*V)[1],Out.Y);}
        bool Hash(const FString& V)
        {if (V.Len()!=64) return false;for (TCHAR C:V) if (!((C>='0'&&C<='9')||(C>='a'&&C<='f'))) return false;return true;}
        bool Hash(const FObject& O,const TCHAR* Key,FString& Out) {return String(O,Key,Out) && Hash(Out);}
        bool Equals(const FObject& O,const TCHAR* Key,const FString& Expected)
        {FString V;return String(O,Key,V) && V==Expected;}
        bool Epsg(const FString& V)
        {if (!V.StartsWith(TEXT("EPSG:")) || V.Len()==5) return false;for (int32 I=5;I<V.Len();++I) if (V[I]<'0'||V[I]>'9') return false;return true;}
        bool RelativeJson(const FString& V)
        {TArray<FString> Parts;V.ParseIntoArray(Parts,TEXT("/"));return !V.StartsWith(TEXT("/")) && !V.Contains(TEXT(":")) && !V.Contains(TEXT("\\")) && !Parts.Contains(TEXT("..")) && V.EndsWith(TEXT(".json"));}
        bool Version(const FObject& O,const FString& Schema)
        {int32 V=0;return Equals(O,TEXT("$schema"),Schema) && Integer(O,TEXT("schema_version"),V) && V==1;}
        bool Reject(FProjectWorldCanonicalValidation& V,const TCHAR* Message)
        {V.ErrorCode=TEXT("ground-base-input");V.Message=Message;return false;}
        bool Targets(const FObject& Profile,FRetainedInput& Out)
        {
            if (const auto Values=Array(Profile,TEXT("target_cells")))
            {
                for (const auto& Value:*Values)
                {
                    const FObject* O=nullptr;FIntPoint P;
                    if (!Value->TryGetObject(O) || !O || (*O)->Values.Num()!=2 || !Integer(*O,TEXT("x"),P.X) ||
                        !Integer(*O,TEXT("y"),P.Y) || Out.Targets.Contains(P)) return false;
                    Out.Targets.Add(P);
                }
            }
            else
            {
                FObject Range;FIntPoint Low,High;
                if (!Object(Profile,TEXT("target_cell_range"),Range) || Range->Values.Num()!=4 ||
                    !Integer(Range,TEXT("min_x"),Low.X) || !Integer(Range,TEXT("min_y"),Low.Y) ||
                    !Integer(Range,TEXT("max_x"),High.X) || !Integer(Range,TEXT("max_y"),High.Y) ||
                    Low.X>High.X || Low.Y>High.Y) return false;
                const int64 Width=static_cast<int64>(High.X)-Low.X+1,Height=static_cast<int64>(High.Y)-Low.Y+1;
                // A complete target needs at least one distinct Source grid node per cell.
                if (Width>Out.Samples.Num() || Height>Out.Samples.Num() || Width*Height>Out.Samples.Num()) return false;
                for (int64 Y=Low.Y;Y<=High.Y;++Y) for (int64 X=Low.X;X<=High.X;++X) Out.Targets.Add(FIntPoint(static_cast<int32>(X),static_cast<int32>(Y)));
            }
            return !Out.Targets.IsEmpty();
        }
        bool Quantize(double Value,double Step,double& Out)
        {
            const double Places=FMath::Max(0.0,FMath::CeilToDouble(-FMath::LogX(10.0,Step)))+1;
            const double Scale=FMath::Pow(10.0,Places);
            if (!FMath::IsFinite(Scale) || !FMath::IsFinite(Value/Step)) return false;
            const double Rounded=FMath::RoundHalfToEven(Value/Step)*Step*Scale;
            if (!FMath::IsFinite(Rounded)) return false;
            Out=FMath::RoundHalfToEven(Rounded)/Scale;return FMath::IsFinite(Out);
        }
        bool Matrix(const FObject& Parent,const TCHAR* Key,int32 Rows,int32 Cols,int32 X0,int32 Y0,
            const FRetainedInput& Input)
        {
            const auto Values=Array(Parent,Key);if (!Values || Values->Num()!=Rows) return false;
            for (int32 Y=0;Y<Rows;++Y)
            {
                const FArray* Row=nullptr;if (!(*Values)[Y]->TryGetArray(Row) || !Row || Row->Num()!=Cols) return false;
                for (int32 X=0;X<Cols;++X)
                {
                    double V=0,Expected=0;const int32 I=X0+X,J=Y0+Y;
                    if (I<0 || J<0 || I>=Input.Size.X || J>=Input.Size.Y || !Number((*Row)[X],V) ||
                        !Quantize(Input.Samples[J*Input.Size.X+I],Input.HeightStep,Expected) || FMath::Abs(V-Expected)>1e-8) return false;
                }
            }
            return true;
        }
        bool Lineage(const FObject& O,const FRetainedInput& I,const FString& Base,const FString& Request)
        {
            FString Parent;
            return O && O->Values.Num()==7 && Equals(O,TEXT("kind"),TEXT("qualified_ground_base")) &&
                Equals(O,TEXT("base_result_sha256"),Base) && Hash(O,TEXT("parent_lineage_sha256"),Parent) &&
                Equals(O,TEXT("refinement_inputs_sha256"),Request) && Equals(O,TEXT("source_admission_sha256"),I.AdmissionHash) &&
                Equals(O,TEXT("ground_grid_sha256"),I.GridHash) && Equals(O,TEXT("construction_identity"),I.ConstructionHash);
        }
        bool Dependencies(const FObject& Terrain,const FObject& Manifest,const FRetainedInput& I,const FString& Base,const FString& Request,const FString& Shore)
        {
            const auto Components=Array(Terrain,TEXT("raster_component_dependencies"));
            const auto Declared=Array(Manifest,TEXT("terrain_component_dependencies"));FObject Artifact;
            if (!Components || Components->Num()!=1 || !Declared || Declared->Num()!=1 || !Object(Manifest,TEXT("terrain_artifact"),Artifact)) return false;
            const FObject *A=nullptr,*B=nullptr;FString Component,Sample;
            if (!(*Components)[0]->TryGetObject(A) || !A || (*A)->Values.Num()!=2 ||
                !(*Declared)[0]->TryGetObject(B) || !B || (*B)->Values.Num()!=2 ||
                !Hash(*A,TEXT("component_id"),Component) || !Hash(*A,TEXT("sample_semantic_sha256"),Sample) ||
                !Equals(*B,TEXT("component_id"),Component) || !Equals(*B,TEXT("sample_semantic_sha256"),Sample)) return false;
            const auto Values=Array(Artifact,TEXT("dependencies"));if (!Values || Values->Num()!=(Shore.IsEmpty() ? 4 : 5)) return false;
            TSet<FString> Expected={TEXT("base-compile:")+Base,TEXT("ground-base:")+Request,
                TEXT("source-admission:")+I.AdmissionHash,TEXT("raster-component:")+Component+TEXT(":")+Sample};
            if (!Shore.IsEmpty()) Expected.Add(Shore);
            for (const auto& Value:*Values) {FString S;if (Value->Type!=EJson::String || !Value->TryGetString(S) || Expected.Remove(S)!=1) return false;}
            return Expected.IsEmpty();
        }
    }
    bool ReadInput(const FObject& Operation,const FObject& Input,const FString& InputHash,FRetainedInput& Out,FString& Error)
    {
        Error=TEXT("operation, retained descriptor or qualified grid contract");
        if (Out.Present) return false;
        FRetainedInput I;FObject Base,Admission,C,Grid,Frame,Evidence,Meta,Profile,Layout;
        const auto Ids=Array(Operation,TEXT("feature_ids"));FString Path;double Bytes=0,NoData=0;
        if (!Operation || Operation->Values.Num()!=5 || !Ids || !Ids->IsEmpty() ||
            !Equals(Operation,TEXT("kind"),TEXT("qualified_ground_base")) || !Object(Operation,TEXT("base_features"),Base) || !Base->Values.IsEmpty() ||
            !Hash(Operation,TEXT("construction_identity"),I.ConstructionHash) || !Hash(InputHash) ||
            !Input || Input->Values.Num()!=5 || !Version(Input,TEXT("https://alis.world/schemas/world-compiler/ground-base-input-v1.json")) ||
            !String(Input,TEXT("compiler_profile_path"),Path) || !RelativeJson(Path) || !Object(Input,TEXT("source_admission"),Admission) ||
            Admission->Values.Num()!=3 || !String(Admission,TEXT("path"),Path) || !RelativeJson(Path) ||
            !Hash(Admission,TEXT("sha256"),I.AdmissionHash) || !Number(Admission,TEXT("byte_size"),Bytes) || Bytes<1 || Bytes>16000000 || Bytes!=FMath::FloorToDouble(Bytes) ||
            !Object(Input,TEXT("construction"),C) || (C->Values.Num()!=4 && C->Values.Num()!=5) ||
            (C->Values.Num()==5 && !C->HasField(TEXT("water_conditioning_limits"))) ||
            !Object(C,TEXT("grid"),Grid) || Grid->Values.Num()!=10 ||
            !Version(Grid,TEXT("https://alis.world/schemas/world-source/ground-grid-v1.json")) ||
            !Equals(Grid,TEXT("basis"),TEXT("raster_ground_model")) || !Equals(Grid,TEXT("sample_rule"),TEXT("grid_nodes")) ||
            !Object(Grid,TEXT("frame"),Frame) || Frame->Values.Num()!=3 || !String(Frame,TEXT("horizontal"),I.CanonicalCrs) || !Epsg(I.CanonicalCrs) ||
            !Equals(Frame,TEXT("vertical"),TEXT("EPSG:3855")) || !Equals(Frame,TEXT("unit"),TEXT("m")) ||
            !Vector(Grid,TEXT("origin"),I.Origin) || !Vector(Grid,TEXT("spacing"),I.Spacing) || I.Spacing.X<=0 || I.Spacing.Y>=0 ||
            !Number(Grid,TEXT("nodata"),NoData)) return false;
        Error=TEXT("qualified grid dimensions or complete finite samples");FVector2D Size;
        if (!Vector(Grid,TEXT("size"),Size) || Size.X<2 || Size.Y<2 || Size.X>MAX_int32 || Size.Y>MAX_int32 ||
            Size.X!=FMath::FloorToDouble(Size.X) || Size.Y!=FMath::FloorToDouble(Size.Y) || Size.X*Size.Y>MAX_int32) return false;
        I.Size=FIntPoint(static_cast<int32>(Size.X),static_cast<int32>(Size.Y));const auto Rows=Array(Grid,TEXT("samples"));if (!Rows || Rows->Num()!=I.Size.Y) return false;
        for (const auto& RowValue:*Rows)
        {
            const FArray* Row=nullptr;if (!RowValue->TryGetArray(Row) || !Row || Row->Num()!=I.Size.X) return false;
            for (const auto& Value:*Row) {double Z=0;if (!Number(Value,Z) || Z==NoData) return false;I.Samples.Add(Z);}
        }
        Error=TEXT("source evidence, frame or accuracy metadata");
        if (!Object(C,TEXT("evidence"),Evidence) || Evidence->Values.Num()!=6 || !Equals(Evidence,TEXT("basis"),TEXT("raster_ground_model")) ||
            !Equals(Evidence,TEXT("canonical_crs"),I.CanonicalCrs) || !Equals(Evidence,TEXT("vertical_datum"),TEXT("EPSG:3855")) ||
            !Hash(Evidence,TEXT("grid_content_sha256"),I.GridHash) || !Hash(Evidence,TEXT("source_sha256"),I.SourceHash) || !Hash(Evidence,TEXT("processing_sha256"),I.ProcessingHash) ||
            !Object(C,TEXT("source_metadata"),Meta) || Meta->Values.Num()!=3 || !String(Meta,TEXT("source_crs"),I.SourceCrs) || !Epsg(I.SourceCrs) ||
            !String(Meta,TEXT("confidence"),I.Confidence)) return false;
        const auto Accuracy=Meta->TryGetField(TEXT("source_accuracy_m"));if (!Accuracy) return false;
        I.KnownAccuracy=Accuracy->Type==EJson::Number;
        if ((I.KnownAccuracy && (!Number(Accuracy,I.Accuracy) || I.Accuracy<0)) || (!I.KnownAccuracy && Accuracy->Type!=EJson::Null)) return false;
        Error=TEXT("compiler profile, canonical frame or complete target domain");int32 ProfileVersion=0;FString ProfileSchema;
        if (!Object(C,TEXT("compiler_profile"),Profile) || !String(Profile,TEXT("$schema"),ProfileSchema) ||
            !Integer(Profile,TEXT("schema_version"),ProfileVersion) || ProfileVersion!=1 ||
            !String(Profile,TEXT("profile_id"),I.Profile) || !Vector(Profile,TEXT("engine_georeference_origin"),I.Georeference) ||
            !Object(Profile,TEXT("grid"),Layout) || !Equals(Layout,TEXT("canonical_crs"),I.CanonicalCrs) || !Equals(Layout,TEXT("vertical_datum"),TEXT("EPSG:3855")) ||
            !String(Layout,TEXT("coordinate_transform"),I.Transform) || !Vector(Layout,TEXT("origin"),I.Lattice) ||
            !Vector(Layout,TEXT("sample_spacing"),I.CellSpacing) || I.CellSpacing.X!=I.Spacing.X || I.CellSpacing.Y!=-I.Spacing.Y ||
            !Number(Layout,TEXT("height_quantization"),I.HeightStep) || I.HeightStep<=0 ||
            !Number(Layout,TEXT("coordinate_quantization"),I.CoordinateStep) || I.CoordinateStep<=0 ||
            !Number(Layout,TEXT("vertical_origin_m"),I.HeightOrigin) || !Integer(Layout,TEXT("halo_samples"),I.Halo) || I.Halo<0 || !Vector(Layout,TEXT("cell_quads"),Size) ||
            Size.X<1 || Size.Y<1 || Size.X>MAX_int32 || Size.Y>MAX_int32 || Size.X!=FMath::FloorToDouble(Size.X) || Size.Y!=FMath::FloorToDouble(Size.Y)) return false;
        I.Quads=FIntPoint(static_cast<int32>(Size.X),static_cast<int32>(Size.Y));if (!Targets(Profile,I) || I.Quads.X>=I.Size.X || I.Quads.Y>=I.Size.Y || I.Halo>I.Size.X || I.Halo>I.Size.Y) return false;
        if (C->HasField(TEXT("water_conditioning_limits")))
        {
            Error=TEXT("declared candidate-only Water conditioning limits");
            FObject Limits;double Cut=0,Distance=0;TSet<FString> Seen;
            if (!Object(C,TEXT("water_conditioning_limits"),Limits) || Limits->Values.Num()!=3 ||
                !Number(Limits,TEXT("maximum_cut_m"),Cut) || Cut<0 || !Number(Limits,TEXT("maximum_support_distance_m"),Distance) || Distance<0) return false;
            const auto Accepted=Array(Limits,TEXT("accepted_accuracy"));if (!Accepted || Accepted->IsEmpty()) return false;
            for (const auto& Value:*Accepted) {FString S;if (Value->Type!=EJson::String || !Value->TryGetString(S) || S.IsEmpty() || Seen.Contains(S)) return false;Seen.Add(S);}
        }
        I.InputHash=InputHash;I.Present=true;Out=MoveTemp(I);Error.Reset();return true;
    }
    bool ReadTerrain(const FObject& Terrain,const FObject& Manifest,const FObject& CoverageCell,const FProjectWorldCanonicalBundle& B,
        const FProjectWorldCanonicalCell& Cell,const FString& BaseHash,FRetainedInput& I,FProjectWorldCanonicalValidation& Validation,const FObject& Refinement)
    {
        if (!I.Present) return true;
        FString ShoreDependency;
        if (!Cell.Terrain.RefinedQuads.IsEmpty() && !ProjectWorldShoreTransitionParsing::Dependency(Refinement,I.InputHash,ShoreDependency))
            return Reject(Validation,TEXT("Refined Ground requires its authenticated explicit shore construction."));
        FObject Alignment,Fit,Window,LineageObject,CoverageLineage,Provenance;
        double Residual=0,Quantum=0,Accuracy=0;
        const FIntPoint Coordinate(Cell.CellX,Cell.CellY);
        if (B.ProfileId!=I.Profile || B.CanonicalCrs!=I.CanonicalCrs || B.VerticalDatum!=TEXT("EPSG:3855") || B.CoordinateTransform!=I.Transform ||
            B.LatticeOriginMeters!=I.Lattice || B.SampleSpacingMeters!=I.CellSpacing || B.EngineGeoreferenceOriginMeters!=I.Georeference || B.CellQuads!=I.Quads ||
            B.HeightQuantizationMeters!=I.HeightStep || B.CoordinateQuantizationMeters!=I.CoordinateStep || B.HeightOriginMeters!=I.HeightOrigin ||
            !I.Targets.Contains(Coordinate) || I.Used.Contains(Coordinate) ||
            !Equals(Terrain,TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/terrain-cell-v2.json")) ||
            Cell.Terrain.SamplesX!=I.Quads.X+1 || Cell.Terrain.SamplesY!=I.Quads.Y+1 ||
            Cell.Terrain.VerticalProvenanceId!=TEXT("qualified_ground:")+I.ProcessingHash || Cell.Terrain.VerticalConfidence!=I.Confidence ||
            Cell.Terrain.bHasVerticalSourceAccuracy!=I.KnownAccuracy || (I.KnownAccuracy && Cell.Terrain.VerticalSourceAccuracyMeters!=I.Accuracy) ||
            Cell.Terrain.SamplingQuantizationResidualMeters!=I.HeightStep*.5 ||
            !Object(Terrain,TEXT("vertical_provenance"),Provenance) || Provenance->Values.Num()!=5 ||
            !Equals(Provenance,TEXT("source_ref"),TEXT("qualified_ground:")+I.ProcessingHash) || !Equals(Provenance,TEXT("vertical_datum"),TEXT("EPSG:3855")) ||
            !Equals(Provenance,TEXT("confidence"),I.Confidence) || !Number(Provenance,TEXT("sampling_quantization_residual_m"),Residual) || Residual!=I.HeightStep*.5 ||
            !Number(Terrain,TEXT("height_quantization"),Quantum) || Quantum!=I.HeightStep ||
            !Object(Terrain,TEXT("raster_alignment"),Alignment) || Alignment->Values.Num()!=4 || !Equals(Alignment,TEXT("authority"),TEXT("qualified_ground_grid_nodes")) ||
            !Equals(Alignment,TEXT("source_crs"),I.SourceCrs) || !Equals(Alignment,TEXT("canonical_crs"),I.CanonicalCrs) || !Equals(Alignment,TEXT("resampling_method"),TEXT("bilinear")) ||
            !Object(Manifest,TEXT("terrain_lineage"),LineageObject) || !Lineage(LineageObject,I,BaseHash,B.InputsHash) ||
            !Object(CoverageCell,TEXT("terrain_lineage"),CoverageLineage) || !Lineage(CoverageLineage,I,BaseHash,B.InputsHash) ||
            !Dependencies(Terrain,Manifest,I,BaseHash,B.InputsHash,ShoreDependency))
            return Reject(Validation,TEXT("Ground Terrain frame, shape, provenance or retained lineage disagrees."));
        const auto AccuracyValue=Provenance->TryGetField(TEXT("source_accuracy_m"));
        if (!AccuracyValue || (I.KnownAccuracy ? (!Number(AccuracyValue,Accuracy) || Accuracy!=I.Accuracy) : AccuracyValue->Type!=EJson::Null))
            return Reject(Validation,TEXT("Ground source accuracy differs from its retained known or unknown value."));
        FString ParentA,ParentB;String(LineageObject,TEXT("parent_lineage_sha256"),ParentA);String(CoverageLineage,TEXT("parent_lineage_sha256"),ParentB);
        const double Width=I.Quads.X*I.CellSpacing.X,Height=I.Quads.Y*I.CellSpacing.Y;
        const double West=I.Lattice.X+Cell.CellX*Width,South=I.Lattice.Y+Cell.CellY*Height,North=South+Height;
        const double SourceX=(West-I.Origin.X)/I.Spacing.X,SourceY=(North-I.Origin.Y)/I.Spacing.Y;
        const double X=FMath::RoundHalfToEven(SourceX),Y=FMath::RoundHalfToEven(SourceY);
        if (ParentA!=ParentB || X<0 || Y<0 || X>MAX_int32 || Y>MAX_int32 ||
            FMath::Abs(I.Origin.X+X*I.Spacing.X-West)>1e-8 || FMath::Abs(I.Origin.Y+Y*I.Spacing.Y-North)>1e-8 ||
            Cell.Bounds!=FVector4d(West,South,West+Width,North) ||
            !Object(Terrain,TEXT("water_fit"),Fit) || Fit->Values.Num()!=2 || !Object(Fit,TEXT("halo_window"),Window) ||
            !Matrix(Fit,TEXT("core_samples"),I.Quads.Y+1,I.Quads.X+1,static_cast<int32>(X),static_cast<int32>(Y),I) ||
            !Matrix(Window,TEXT("samples"),I.Quads.Y+1+2*I.Halo,I.Quads.X+1+2*I.Halo,static_cast<int32>(X)-I.Halo,static_cast<int32>(Y)-I.Halo,I))
            return Reject(Validation,TEXT("Ground pre-Water core or halo differs from its qualified grid nodes."));
        const auto Bounds=Array(Window,TEXT("index_bounds"));
        const int64 Left=static_cast<int64>(Cell.CellX)*I.Quads.X-I.Halo,Top=static_cast<int64>(Cell.CellY)*I.Quads.Y-I.Halo;
        const int64 Expected[4]={Left,Top,Left+I.Quads.X+2*I.Halo,Top+I.Quads.Y+2*I.Halo};
        if (!Bounds || Bounds->Num()!=4) return Reject(Validation,TEXT("Ground halo index bounds are missing."));
        for (int32 K=0;K<4;++K) {double V=0;if (!Number((*Bounds)[K],V) || V!=Expected[K]) return Reject(Validation,TEXT("Ground halo index bounds disagree."));}
        I.Used.Add(Coordinate);return true;
    }
    bool ValidateUsage(const FRetainedInput& I,FProjectWorldCanonicalValidation& V)
    {return !I.Present || I.Used.Num()==I.Targets.Num() || Reject(V,TEXT("Qualified Ground input does not bind every original target cell."));}
}
