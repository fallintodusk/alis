// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldVerifyTestFlow
{
    // Test-only typed overrides deliberately establish no replacement authority admission.
    inline bool ApplyTypedTerrainFixture(FProjectWorldCanonicalBundle& Bundle,
        const FJsonObject& Fixture,FString& Error)
    {
        for (const auto& Value:Fixture.GetArrayField(TEXT("cells")))
        {
            const auto Object=Value->AsObject();const FString Key=Object->GetStringField(TEXT("cell_id"));
            auto* Cell=Bundle.Cells.FindByPredicate([&](const auto& C){return C.CellId==Key;});
            if (Cell==nullptr) { Error=TEXT("Typed fixture selected a cell absent from its loaded base.");return false; }
            Cell->Terrain.RefinedQuads.Reset();
            for (const auto& Q:Object->GetArrayField(TEXT("quads")))
            {
                const auto O=Q->AsObject();auto& Quad=Cell->Terrain.RefinedQuads.AddDefaulted_GetRef();
                const auto XY=O->GetArrayField(TEXT("coordinate"));Quad.Coordinate=FIntPoint(XY[0]->AsNumber(),XY[1]->AsNumber());
                for (const auto& V:O->GetArrayField(TEXT("vertices")))
                {
                    const auto P=V->AsArray();Quad.Vertices.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()));
                    Quad.Weights.Add(FVector2f(P[3]->AsNumber(),P[4]->AsNumber()));
                }
                for (const auto& F:O->GetArrayField(TEXT("triangles")))
                { const auto I=F->AsArray();Quad.Triangles.Add(FIntVector(I[0]->AsNumber(),I[1]->AsNumber(),I[2]->AsNumber())); }
            }
        }
        return true;
    }
}
#endif
