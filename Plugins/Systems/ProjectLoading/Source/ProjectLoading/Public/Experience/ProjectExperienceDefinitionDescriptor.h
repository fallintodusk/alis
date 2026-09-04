// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Experience/ProjectExperienceDescriptorBase.h"
#include "ProjectExperienceDefinitionDescriptor.generated.h"

class UProjectExperienceDefinition;

/**
 * Generic descriptor backed by a configured UProjectExperienceDefinition asset.
 *
 * One class serves every configured experience: identity, map, traversal token, and scan
 * directory all arrive as data, so adding a configured experience adds no C++.
 *
 * Plugin-owned CDO descriptors (City17, MainMenuWorld, feature descriptors) keep working
 * unchanged - this is an additional source of descriptors, not a replacement registry.
 */
UCLASS()
class PROJECTLOADING_API UProjectExperienceDefinitionDescriptor : public UProjectExperienceDescriptorBase
{
	GENERATED_BODY()

public:
	/**
	 * Project a configured definition onto this descriptor.
	 * Returns false when the definition lacks a stable identity or map, so a malformed
	 * record fails closed instead of registering an unusable experience.
	 */
	bool InitializeFromDefinition(const UProjectExperienceDefinition& Definition);

	virtual void BuildLoadRequest(FLoadRequest& OutRequest) const override;
	virtual void GetAssetScanSpecs(TArray<FExperienceAssetScanSpec>& OutSpecs) const override;

private:
	/** Generic traversal token forwarded to ProjectSinglePlay; empty means mode default. */
	UPROPERTY()
	FString TraversalMode;

	/** Package directory scanned so cooked builds can resolve this experience's map. */
	UPROPERTY()
	FString AssetScanDirectory;
};
