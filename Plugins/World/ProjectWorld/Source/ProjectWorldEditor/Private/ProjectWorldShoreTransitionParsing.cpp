// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldShoreTransitionParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldCanonicalFiniteJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"
#include "Polygon2.h"

namespace ProjectWorldShoreTransitionParsing
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
        bool Object(const FObject& O,const TCHAR* Key,FObject& Out)
        {const FObject* V=nullptr;if (!O || !O->TryGetObjectField(Key,V) || !V || !*V) return false;Out=*V;return true;}
        const FArray* Array(const FObject& O,const TCHAR* Key)
        {const FArray* V=nullptr;return O && O->TryGetArrayField(Key,V) ? V : nullptr;}
        bool Hash(const FString& V)
        {if (V.Len()!=64) return false;for (TCHAR C:V) if (!((C>='0'&&C<='9')||(C>='a'&&C<='f'))) return false;return true;}
        bool Hash(const FObject& O,const TCHAR* Key,FString& Out) {return String(O,Key,Out) && Hash(Out);}
        bool Equals(const FObject& O,const TCHAR* Key,const FString& Expected)
        {FString V;return String(O,Key,V) && V==Expected;}
        bool Coordinate(const TSharedPtr<FJsonValue>& V,FIntPoint& Out)
        {
            const FArray* Values=nullptr;if (!V || !V->TryGetArray(Values) || !Values || Values->Num()!=2) return false;
            for (int32 I=0;I<2;++I) {double N=0;if (!Number((*Values)[I],N) || N<0 || N>MAX_int32 || N!=FMath::FloorToDouble(N)) return false;Out[I]=static_cast<int32>(N);}
            return true;
        }
        bool Policy(const FObject& O,TMap<FString,double>& Out)
        {
            if (!O || O->Values.Num()!=8) return false;
            for (const TCHAR* Key:{TEXT("bank_grade_x"),TEXT("bank_wet_offset_m"),TEXT("south_collar_zero_y"),TEXT("north_collar_zero_y"),
                TEXT("collar_grade"),TEXT("maximum_bank_gradient"),TEXT("maximum_triangles"),TEXT("maximum_restore_m")})
            {
                double N=0;const FString K=Key;
                if (!Number(O,Key,N) || (!K.EndsWith(TEXT("zero_y")) && N<=0)) return false;
                Out.Add(K,N);
            }
            const double Count=Out[TEXT("maximum_triangles")];
            return Count>=2 && Count<=4096 && Count==FMath::FloorToDouble(Count) && Out[TEXT("south_collar_zero_y") ]<Out[TEXT("north_collar_zero_y")];
        }
    }
    bool ReadInput(const FObject& Operation,const FObject& Input,const FString& InputHash,FInputs& Out)
    {
        FInput I;FObject Base,Selected,P,Provenance;double Version=0;FString Description;
        const auto Ids=Array(Operation,TEXT("feature_ids"));
        if (!Out.IsEmpty() || !Hash(InputHash) || !Operation || Operation->Values.Num()!=4 ||
            !Equals(Operation,TEXT("kind"),TEXT("inferred_shore_transition")) || !Ids || Ids->Num()!=1 ||
            !Object(Operation,TEXT("base_features"),Base) || Base->Values.Num()!=1 ||
            !Input || Input->Values.Num()!=11 || !ProjectWorldCanonicalJson::FiniteDocument(Input) ||
            !Equals(Input,TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/shore-transition-input-v1.json")) ||
            !Number(Input,TEXT("schema_version"),Version) || Version!=1 || !String(Input,TEXT("grid_id"),I.Grid) ||
            !Hash(Input,TEXT("ground_input_sha256"),I.GroundHash) || !String(Input,TEXT("water_feature_id"),I.WaterId) ||
            !Hash(Input,TEXT("base_water_sha256"),I.BaseWaterHash) || !Hash(Input,TEXT("effective_water_sha256"),I.WaterHash) ||
            !Object(Input,TEXT("effective_water"),I.WaterSnapshot) || !Equals(I.WaterSnapshot,TEXT("feature_id"),I.WaterId) ||
            (*Ids)[0]->Type!=EJson::String || (*Ids)[0]->AsString()!=I.WaterId || !Equals(Base,*I.WaterId,I.BaseWaterHash) ||
            !Object(Input,TEXT("parameters"),P) || !Policy(P,I.Policy) || !Object(Input,TEXT("selected"),Selected) ||
            Selected->Values.IsEmpty() || Selected->Values.Num()>4 || !Object(Input,TEXT("provenance"),Provenance) ||
            Provenance->Values.Num()!=2 || !Equals(Provenance,TEXT("basis"),TEXT("authored_game_world_transition_not_surveyed")) ||
            !String(Provenance,TEXT("description"),Description)) return false;
        int32 Total=0;
        for (const auto& Pair:Selected->Values)
        {
            const FArray* Coordinates=nullptr;TSet<FIntPoint> Points;
            if (Pair.Key.IsEmpty() || !Pair.Value->TryGetArray(Coordinates) || !Coordinates || Coordinates->IsEmpty()) return false;
            for (const auto& V:*Coordinates) {FIntPoint C;if (!Coordinate(V,C) || Points.Contains(C) || ++Total>64) return false;Points.Add(C);}
            I.Selected.Add(FString(*Pair.Key),MoveTemp(Points));
        }
        Out.Add(InputHash,MoveTemp(I));return true;
    }
    bool ReadTerrain(const FObject& Document,const FString& Grid,const FString& Cell,FInputs& Inputs)
    {
        FObject C,P,M;FString InputHash,Domain,Implementation,Execution;TMap<FString,double> Parameters;
        if (!Document || Document->Values.Num()!=11 || !Equals(Document,TEXT("construction"),TEXT("inferred_east_bank_v1")) ||
            !Object(Document,TEXT("consumed"),C) || C->Values.Num()!=8 || !Hash(C,TEXT("shore_input_sha256"),InputHash) ||
            !Hash(C,TEXT("domain_sha256"),Domain) || !Hash(C,TEXT("implementation_sha256"),Implementation) ||
            !Hash(C,TEXT("execution_identity"),Execution) || !Object(Document,TEXT("policy"),P) || !Policy(P,Parameters) ||
            !Object(Document,TEXT("metrics"),M) || M->Values.Num()!=5) return false;
        auto* I=Inputs.Find(InputHash);if (!I || I->Grid!=Grid || !I->Selected.Contains(Cell) || I->Used.Contains(Cell) ||
            !Equals(C,TEXT("ground_input_sha256"),I->GroundHash) || !Equals(C,TEXT("water_feature_id"),I->WaterId) ||
            !Equals(C,TEXT("base_water_sha256"),I->BaseWaterHash) || !Equals(C,TEXT("water_sha256"),I->WaterHash)) return false;
        for (const auto& Pair:I->Policy) if (!Parameters.Contains(Pair.Key) || Parameters[Pair.Key]!=Pair.Value) return false;
        double Count=0,Faces=0,Grade=0,Restore=0,Wet=0;
        if (!Number(M,TEXT("affected_quads"),Count) || Count<1 || Count>64 || Count!=FMath::FloorToDouble(Count) ||
            !Number(M,TEXT("triangles"),Faces) || Faces<1 || Faces!=FMath::FloorToDouble(Faces) ||
            !Number(M,TEXT("maximum_bank_gradient"),Grade) || Grade<0 || Grade>I->Policy[TEXT("maximum_bank_gradient")] ||
            !Number(M,TEXT("maximum_restore_m"),Restore) || Restore<0 || Restore>I->Policy[TEXT("maximum_restore_m")] ||
            !Number(M,TEXT("wet_height_upper_m"),Wet)) return false;
        int32 Total=0;for (const auto& Pair:I->Selected) Total+=Pair.Value.Num();if (Count!=Total) return false;
        const auto Quads=Array(Document,TEXT("quads"));TSet<FIntPoint> Actual;
        if (!Quads || Quads->Num()!=I->Selected[Cell].Num()) return false;
        for (const auto& V:*Quads)
        {
            const FObject* Q=nullptr;FIntPoint Point;
            if (!V->TryGetObject(Q) || !Q || !Coordinate((*Q)->TryGetField(TEXT("coordinate")),Point) || Actual.Contains(Point)) return false;
            Actual.Add(Point);if (!I->Selected[Cell].Contains(Point)) return false;
        }
        I->Used.Add(Cell);I->MaximumWetHeight=FMath::Max(I->MaximumWetHeight,Wet);return true;
    }
    bool ValidateEncoded(const FObject& Document,const FProjectWorldCanonicalTerrain& Terrain)
    {
        if (!Document || !Equals(Document,TEXT("construction"),TEXT("inferred_east_bank_v1"))) return true;
        FObject P;double Limit=0,Capacity=0;
        if (!Object(Document,TEXT("policy"),P) || !Number(P,TEXT("maximum_bank_gradient"),Limit) || !Number(P,TEXT("maximum_triangles"),Capacity)) return false;
        for (const auto& Q:Terrain.RefinedQuads)
        {
            if (Q.Triangles.Num()>Capacity) return false;
            for (const auto& T:Q.Triangles)
            {
                const FVector U=Q.Vertices[T.Y]-Q.Vertices[T.X],V=Q.Vertices[T.Z]-Q.Vertices[T.X];
                const double D=U.X*V.Y-U.Y*V.X;
                if (D<=0 || FVector2D((U.Z*V.Y-V.Z*U.Y)/D,(U.X*V.Z-V.X*U.Z)/D).Size()>Limit) return false;
            }
        }
        return true;
    }
    bool ValidateSources(const FObject& Features,const FInputs& Inputs)
    {
        const auto Values=Array(Features,TEXT("features"));if (!Values) return false;
        for (const auto& V:*Values)
        {
            const FObject* Feature=nullptr;FString Id;
            if (!V->TryGetObject(Feature) || !Feature || !String(*Feature,TEXT("feature_id"),Id)) return false;
            for (const auto& Pair:Inputs) if (Pair.Value.WaterId==Id &&
                !ProjectWorldCanonicalJson::Equal(V,MakeShared<FJsonValueObject>(Pair.Value.WaterSnapshot))) return false;
        }
        return true;
    }
    bool Dependency(const FObject& Document,const FString& GroundInputHash,FString& Out)
    {
        FObject C;if (!Equals(Document,TEXT("construction"),TEXT("inferred_east_bank_v1")) ||
            !Object(Document,TEXT("consumed"),C) || C->Values.Num()!=8 || !Equals(C,TEXT("ground_input_sha256"),GroundInputHash)) return false;
        TArray<FString> Keys;for (const auto& Pair:C->Values) Keys.Add(FString(*Pair.Key));Keys.Sort();FString Text;
        auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);Writer->WriteObjectStart();
        for (const auto& Key:Keys)
        {
            FString S;if (!String(C,*Key,S)) return false;
            for (TCHAR Ch:S) if (!((Ch>='a'&&Ch<='z')||(Ch>='A'&&Ch<='Z')||(Ch>='0'&&Ch<='9')||Ch==':'||Ch=='/'||Ch=='_'||Ch=='-'||Ch=='.')) return false;
            Writer->WriteValue(Key,S);
        }
        Writer->WriteObjectEnd();Writer->Close();FTCHARToUTF8 Utf8(*Text);TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length());FString HashValue;
        if (!FProjectSha256::HashBuffer(Bytes,HashValue)) return false;Out=TEXT("refinement:")+HashValue;return true;
    }
    bool ValidateWetField(const FInput& Input,const FProjectWorldCanonicalFeature& Water,
        const FProjectWorldCanonicalBundle& Bundle,double& MaximumWetHeight)
    {
        TArray<const FProjectWorldCanonicalTerrainQuad*> Quads;FBox2D Bounds(ForceInit);double ExpectedArea=0;
        for (const auto& Selection:Input.Selected)
        {
            const auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Selection.Key;});
            if (!Cell) return false;
            for (const auto& Coordinate:Selection.Value)
            {
                const auto* Q=Cell->Terrain.RefinedQuads.FindByPredicate([&](const auto& V){return V.Coordinate==Coordinate;});
                if (!Q) return false;Quads.Add(Q);
                for (const auto& V:Q->Vertices) Bounds+=FVector2D(V);
                ExpectedArea+=Cell->Terrain.SampleSpacing.X*Cell->Terrain.SampleSpacing.Y;
            }
        }
        if (!Bounds.bIsValid || FMath::Abs(ExpectedArea-(Bounds.Max.X-Bounds.Min.X)*(Bounds.Max.Y-Bounds.Min.Y))>1e-8) return false;
        TArray<TPair<FVector2D,FVector2D>> Segments;TArray<double> Levels{Bounds.Min.Y,Bounds.Max.Y};
        for (const auto& Polygon:Water.GeometryPolygons)
        {
            auto AddRing=[&](const TArray<FVector2D>& Ring)
            {
                for (int32 N=0;N<Ring.Num();++N)
                {
                    const auto A=Ring[N],B=Ring[(N+1)%Ring.Num()];
                    if (FMath::Min(A.X,B.X)>=Bounds.Max.X || FMath::Max(A.X,B.X)<=Bounds.Min.X ||
                        FMath::Min(A.Y,B.Y)>=Bounds.Max.Y || FMath::Max(A.Y,B.Y)<=Bounds.Min.Y) continue;
                    if (A==B) continue;Segments.Emplace(A,B);
                    for (double Y:{A.Y,B.Y}) if (Y>Bounds.Min.Y && Y<Bounds.Max.Y) Levels.AddUnique(Y);
                }
            };
            AddRing(Polygon.Outer);for (const auto& Hole:Polygon.Holes) AddRing(Hole);
        }
        Levels.Sort();if (Levels.Num()>4096) return false;
        auto Inside=[&](const FVector2D& P)
        {
            for (const auto& Polygon:Water.GeometryPolygons)
            {
                if (!UE::Geometry::FPolygon2d(Polygon.Outer).Contains(P)) continue;
                bool Hole=false;for (const auto& H:Polygon.Holes) Hole|=UE::Geometry::FPolygon2d(H).Contains(P);
                if (!Hole) return true;
            }
            return false;
        };
        auto Clip=[](const TArray<FVector>& Polygon,TFunctionRef<double(const FVector&)> Distance)
        {
            TArray<FVector> Out;if (Polygon.IsEmpty()) return Out;
            FVector A=Polygon.Last();double Before=Distance(A);
            for (const auto& B:Polygon)
            {
                const double After=Distance(B);
                if ((Before<0 && After>0) || (Before>0 && After<0)) Out.Add(A+(Before/(Before-After))*(B-A));
                if (After>=0) Out.Add(B);A=B;Before=After;
            }
            return Out;
        };
        bool Observed=false;MaximumWetHeight=-MAX_dbl;
        for (int32 N=0;N+1<Levels.Num();++N)
        {
            const double Low=Levels[N],High=Levels[N+1],Middle=(Low+High)*.5;
            const TPair<FVector2D,FVector2D>* Coast=nullptr;
            for (const auto& S:Segments) if (FMath::Min(S.Key.Y,S.Value.Y)<Middle && Middle<FMath::Max(S.Key.Y,S.Value.Y))
            {if (Coast) return false;Coast=&S;}
            if (!Coast || !Inside(FVector2D(Bounds.Min.X,Middle)) || Inside(FVector2D(Bounds.Max.X,Middle))) return false;
            const auto A=Coast->Key,B=Coast->Value;const double Slope=(B.X-A.X)/(B.Y-A.Y);
            for (const auto* Q:Quads) for (const auto& T:Q->Triangles)
            {
                TArray<FVector> Polygon{Q->Vertices[T.X],Q->Vertices[T.Y],Q->Vertices[T.Z]};
                Polygon=Clip(Polygon,[&](const FVector& P){return P.Y-Low;});
                Polygon=Clip(Polygon,[&](const FVector& P){return High-P.Y;});
                Polygon=Clip(Polygon,[&](const FVector& P){return (P.Y-A.Y)*Slope-(P.X-A.X);});
                for (const auto& P:Polygon)
                {
                    if (P.ContainsNaN() || P.Z>Water.WaterSurface.LevelMeters) return false;
                    MaximumWetHeight=FMath::Max(MaximumWetHeight,P.Z);Observed=true;
                }
            }
        }
        return Observed;
    }
    bool ValidateUsage(const FInputs& Inputs,const FString& GroundHash,const FProjectWorldCanonicalBundle& Bundle)
    {
        for (const auto& Pair:Inputs)
        {
            const auto& I=Pair.Value;const auto* Water=Bundle.Features.Find(I.WaterId);double ActualWet=0;
            if (I.Grid!=Bundle.GridId || I.GroundHash!=GroundHash || I.Used.Num()!=I.Selected.Num() || !Water ||
                Water->FeatureClass!=TEXT("water") || !Water->WaterSurface.bValid || !Water->WaterSurface.Knots.IsEmpty() ||
                I.MaximumWetHeight>Water->WaterSurface.LevelMeters || !ValidateWetField(I,*Water,Bundle,ActualWet)) return false;
        }
        return true;
    }
}
