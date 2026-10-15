// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialTerrainSemantics.h"
#include "ProjectMaterialExpressionUtils.h"
#include "ProjectMaterialSurfaceRecipe.h"
#include "MeshPartitionMaterialExpressionUtils.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"

UMaterialExpression* ProjectMaterialTerrainSemantics::ComposeColor(UMaterial* Material,
    const FProjectMaterialSurfaceRecipe& Recipe, UMaterialExpression* GroundColor,
    const TMap<FName,int32>& Channels, FString& Error)
{
    using namespace ProjectMaterialExpressionUtils;
    if (!GroundColor || Channels.Num()!=2 || !Channels.Contains(TEXT("ground")) ||
        !Channels.Contains(TEXT("hydro_transition")) || Channels.FindRef(TEXT("ground"))!=0 ||
        Channels.FindRef(TEXT("hydro_transition"))!=1)
    {Error=TEXT("Metric terrain requires the authenticated ground/hydro channel layout."); return nullptr;}
    auto* Resource=Node<UE::MeshPartition::UMaterialExpressionMeshPartitionResource>(Material);
    auto* Ground=Node<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(Material);
    auto* Hydro=Node<UE::MeshPartition::UMaterialExpressionMeshPartitionChannelSampleIndex>(Material);
    Ground->ChannelTextureInput.Connect(0,Resource); Ground->ConstChannelIndex=Channels.FindRef(TEXT("ground"));
    Hydro->ChannelTextureInput.Connect(0,Resource); Hydro->ConstChannelIndex=Channels.FindRef(TEXT("hydro_transition"));
    auto* Tint=Vector(Material,Recipe,TEXT("HydroTint"));
    auto* GroundBlend=Node<UMaterialExpressionLinearInterpolate>(Material);
    GroundBlend->A.Connect(0,Tint); GroundBlend->B.Connect(0,GroundColor); GroundBlend->Alpha.Connect(0,Ground);
    auto* Amount=Binary<UMaterialExpressionMultiply>(Material,Hydro,Scalar(Material,Recipe,TEXT("HydroDarkening")));
    auto* Final=Node<UMaterialExpressionLinearInterpolate>(Material);
    Final->A.Connect(0,GroundBlend); Final->B.Connect(0,Tint); Final->Alpha.Connect(0,Amount);
    return Final;
}
