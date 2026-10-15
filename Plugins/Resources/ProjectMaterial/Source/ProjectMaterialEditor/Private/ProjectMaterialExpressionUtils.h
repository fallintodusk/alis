// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "ProjectMaterialSurfaceRecipe.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"

namespace ProjectMaterialExpressionUtils
{
template<class T> T* Node(UMaterial* M)
{
	return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass()));
}
inline UMaterialExpression* Constant(UMaterial* M,float V)
{
	auto* N=Node<UMaterialExpressionConstant>(M); N->R=V; return N;
}
inline UMaterialExpression* Scalar(UMaterial* M,const FProjectMaterialSurfaceRecipe& R,const TCHAR* Name)
{
	auto* N=Node<UMaterialExpressionScalarParameter>(M);
	N->ParameterName=Name; N->DefaultValue=R.Scalars.FindChecked(Name); return N;
}
inline UMaterialExpression* Vector(UMaterial* M,const FProjectMaterialSurfaceRecipe& R,const TCHAR* Name)
{
	auto* N=Node<UMaterialExpressionVectorParameter>(M);
	N->ParameterName=Name; N->DefaultValue=R.Vectors.FindChecked(Name); return N;
}
template<class T> UMaterialExpression* Binary(UMaterial* M,UMaterialExpression* A,UMaterialExpression* B)
{
	auto* N=Node<T>(M); N->A.Connect(0,A); N->B.Connect(0,B); return N;
}
UMaterialExpression* WorldXY(UMaterial*);
UMaterialExpression* WorldOrientedNormal(UMaterial*,UMaterialExpression*,FString&);
}
