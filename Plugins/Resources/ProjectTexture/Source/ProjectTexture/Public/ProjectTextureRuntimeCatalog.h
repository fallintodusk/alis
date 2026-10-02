// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Engine/DataAsset.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ProjectTextureRuntimeCatalog.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct PROJECTTEXTURE_API FProjectTextureRuntimeNodeDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName NodeId;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FString StructuralIdentity;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	TArray<FName> ParentNodeIds;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	TObjectPtr<UMaterialInterface> GeneratorMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName ParentTextureParameter;

	UPROPERTY(EditAnywhere, Category = "Project Texture", meta = (ClampMin = "1"))
	int32 Width = 1;

	UPROPERTY(EditAnywhere, Category = "Project Texture", meta = (ClampMin = "1"))
	int32 Height = 1;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	TEnumAsByte<ETextureRenderTargetFormat> Format = RTF_RGBA8;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	bool bGenerateMips = true;
};

UCLASS(BlueprintType)
class PROJECTTEXTURE_API UProjectTextureRuntimeCatalog final : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName PatternId;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FString SemanticIdentity;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName OutputContract;

	UPROPERTY(EditAnywhere, Category = "Project Texture", meta = (ClampMin = "1"))
	int64 BudgetBytes = 8 * 1024 * 1024;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	TArray<FProjectTextureRuntimeNodeDescriptor> Nodes;
};

UCLASS(BlueprintType)
class PROJECTTEXTURE_API UProjectTextureOutputSlot final : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Project Texture")
	TObjectPtr<UProjectTextureRuntimeCatalog> Catalog = nullptr;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName SlotName;

	UPROPERTY(EditAnywhere, Category = "Project Texture")
	FName NodeId;

	virtual void PostLoad() override;
	UTextureRenderTarget2D* GetRenderTarget() const;
};
