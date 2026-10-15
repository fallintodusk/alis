// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectMaterialExpressionUtils.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"

UMaterialExpression* ProjectMaterialExpressionUtils::WorldXY(UMaterial* M)
{
	auto* World=Node<UMaterialExpressionWorldPosition>(M);
	auto* XY=Node<UMaterialExpressionComponentMask>(M);
	XY->R=XY->G=true; XY->B=XY->A=false; XY->Input.Connect(0,World); return XY;
}

UMaterialExpression* ProjectMaterialExpressionUtils::WorldOrientedNormal(
	UMaterial* M,UMaterialExpression* Detail,FString& Error)
{
	auto* F=Node<UMaterialExpressionMaterialFunctionCall>(M);
	auto* Native=LoadObject<UMaterialFunction>(nullptr,TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.BlendAngleCorrectedNormals"));
	if (!Native || !F->SetMaterialFunction(Native))
	{ Error=TEXT("Native world-normal function is unavailable."); return nullptr; }
	int32 Bound=0;
	for (auto& I : F->FunctionInputs)
	{
		if (I.Input.InputName==TEXT("BaseNormal")) { I.Input.Connect(0,Node<UMaterialExpressionVertexNormalWS>(M)); ++Bound; }
		if (I.Input.InputName==TEXT("AdditionalNormal")) { I.Input.Connect(0,Detail); ++Bound; }
	}
	if (Bound!=2) { Error=TEXT("Native world-normal signature is unsupported."); return nullptr; }
	auto* Unit=Node<UMaterialExpressionNormalize>(M); Unit->VectorInput.Connect(0,F); return Unit;
}
