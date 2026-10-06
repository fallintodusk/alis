// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldWaterRealization.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"

// The water layer owns its cell actors, cell meshes, and the generated material under its artifact root.
namespace ProjectWorldWaterVerifyProjection
{
	FString Input(const FExpressionInput& Input)
	{
		return Input.Expression != nullptr
			? FString::Printf(TEXT("%s:%d"), *Input.Expression->GetName(), Input.OutputIndex)
			: FString(TEXT("none"));
	}

	FString Expression(const UMaterialExpression& Expression)
	{
		FString Text = Expression.GetName() + TEXT("|") + Expression.GetClass()->GetName();
		if (const UMaterialExpressionConstant3Vector* Vector = Cast<UMaterialExpressionConstant3Vector>(&Expression))
		{
			Text += FString::Printf(TEXT("|%.9g,%.9g,%.9g,%.9g"),
				Vector->Constant.R, Vector->Constant.G, Vector->Constant.B, Vector->Constant.A);
		}
		else if (const UMaterialExpressionConstant* Scalar = Cast<UMaterialExpressionConstant>(&Expression))
		{
			Text += FString::Printf(TEXT("|%.9g"), Scalar->R);
		}
		return Text;
	}

	void AddMaterial(FProjectWorldProjection& Projection, const UMaterial& Material, const FString& ArtifactRoot)
	{
		FProjectWorldProjectionRecord& Record = Projection.Add(
			TEXT("material"), ProjectWorldProjection::RootRelative(Material.GetOutermost()->GetName(), ArtifactRoot));
		TArray<FString> Expressions;
		for (const TObjectPtr<UMaterialExpression>& Candidate : Material.GetExpressions())
		{
			if (Candidate != nullptr)
			{
				Expressions.Add(Expression(*Candidate));
			}
		}
		Expressions.Sort([](const FString& Left, const FString& Right) { return Left.Compare(Right, ESearchCase::CaseSensitive) < 0; });
		Record.Fields.Add(TEXT("blend_mode"), ProjectWorldProjection::EnumName(StaticEnum<EBlendMode>(), Material.BlendMode));
		Record.Fields.Add(TEXT("shading_models"), FString::Printf(TEXT("%u"), Material.GetShadingModels().GetShadingModelField()));
		Record.Fields.Add(TEXT("expressions"), FString::Join(Expressions, TEXT(";")));
		const UMaterialEditorOnlyData* Inputs = Material.GetEditorOnlyData();
		if (Inputs == nullptr)
		{
			Record.Fields.Add(TEXT("inputs"), TEXT("none"));
			return;
		}
		Record.Fields.Add(TEXT("input.base_color"), Input(Inputs->BaseColor));
		Record.Fields.Add(TEXT("input.metallic"), Input(Inputs->Metallic));
		Record.Fields.Add(TEXT("input.specular"), Input(Inputs->Specular));
		Record.Fields.Add(TEXT("input.roughness"), Input(Inputs->Roughness));
		Record.Fields.Add(TEXT("input.normal"), Input(Inputs->Normal));
		Record.Fields.Add(TEXT("input.emissive_color"), Input(Inputs->EmissiveColor));
		Record.Fields.Add(TEXT("input.opacity"), Input(Inputs->Opacity));
		Record.Fields.Add(TEXT("input.opacity_mask"), Input(Inputs->OpacityMask));
		Record.Fields.Add(TEXT("input.ambient_occlusion"), Input(Inputs->AmbientOcclusion));
	}

	bool Project(const FProjectWorldVerifyScope& Scope, FProjectWorldProjection& Projection, FString& OutError)
	{
		TArray<const UStaticMesh*> Meshes;
		for (TActorIterator<AStaticMeshActor> It(Scope.World); It; ++It)
		{
			FString CellId;
			FString Semantic;
			if (!ProjectWorldWaterRealization::ReadActorIdentity(*It, CellId, Semantic))
			{
				continue;
			}
			const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
			if (Component == nullptr)
			{
				OutError = FString::Printf(TEXT("Water actor has no mesh component: %s"), *CellId);
				return false;
			}
			FProjectWorldProjectionRecord& Record = Projection.Add(TEXT("actor"), TEXT("water:") + CellId);
			ProjectWorldProjection::AddActorCommon(Record, **It);
			ProjectWorldProjection::AddStaticMeshComponent(Record, TEXT("component."), *Component, Scope.ArtifactRoot);
			if (Component->GetStaticMesh() != nullptr)
			{
				Meshes.AddUnique(Component->GetStaticMesh());
			}
		}
		TArray<const UMaterial*> Materials;
		for (const UStaticMesh* Mesh : Meshes)
		{
			FProjectWorldProjectionRecord& Record = Projection.Add(
				TEXT("mesh"), ProjectWorldProjection::RootRelative(Mesh->GetOutermost()->GetName(), Scope.ArtifactRoot));
			ProjectWorldProjection::AddStaticMesh(Record, *Mesh, Scope.ArtifactRoot);
			for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
			{
				const UMaterial* Material = Cast<UMaterial>(StaticMaterial.MaterialInterface);
				if (Material != nullptr &&
					Material->GetOutermost()->GetName().StartsWith(Scope.ArtifactRoot, ESearchCase::CaseSensitive))
				{
					Materials.AddUnique(Material);
				}
			}
		}
		for (const UMaterial* Material : Materials)
		{
			AddMaterial(Projection, *Material, Scope.ArtifactRoot);
		}
		return true;
	}

	const FProjectWorldVerifyProjectionRegistrar Registrar(TEXT("project_water_mesh"), 1, 1, &Project);
}
