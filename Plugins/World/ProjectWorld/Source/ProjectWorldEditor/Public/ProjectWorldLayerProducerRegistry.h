#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "ProjectWorldRealizationProfile.h"
#include "ProjectWorldRealizationService.h"

class AActor;
class FJsonObject;
class UMaterialInterface;
class UWorld;
struct FProjectWorldAuthoredOverlaySet;
struct FProjectWorldCanonicalBundle;

struct FProjectWorldLayerInputs
{
	const FProjectWorldCanonicalBundle& Bundle;
	const FProjectWorldRealizationProfile& Profile;
	const FProjectWorldRealizationLayer& Layer;
	const FProjectWorldAuthoredOverlaySet& AuthoredOverlays;
};

struct FProjectWorldLayerUnitInputs
{
	TMap<FString, FString> HashByUnit;
	TMap<FString, TSet<FString>> UnitsByCanonicalCell;
};

struct FProjectWorldLayerApplyContext
{
	UWorld& World;
	const FProjectWorldLayerInputs& Inputs;
	const TArray<FString>& FinalDirtyUnits;
	const TMap<FName, UMaterialInterface*>& PresentationMaterials;
};

struct FProjectWorldLayerActorOwnership
{
	TArray<FName> OwnedTags;
	FString CellTagPrefix;
	bool bCellExtentActors = false;
};

struct FProjectWorldLayerProducerDeclaration
{
	FString GeneratorId;
	int32 GeneratorVersion = 0;
	EProjectWorldLayerKind LayerKind = EProjectWorldLayerKind::GeneratedGeography;
	EProjectWorldDirtyGranularity DirtyGranularity = EProjectWorldDirtyGranularity::CanonicalCell;
	TArray<FString> CanonicalSelectors;
	FString SpatialOwnership;
	FString RuntimeMapping;
	TOptional<int32> DependencyHaloCells;
	TOptional<TArray<FString>> PermittedDependencies;
	FProjectWorldLayerActorOwnership OwnedActors;
	TOptional<FProjectWorldPostApplyBuilder> PostApplyBuilder;
};

class PROJECTWORLDEDITOR_API IProjectWorldLayerProducer : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static const FName Name(TEXT("ProjectWorldLayerProducer"));
		return Name;
	}

	virtual const FProjectWorldLayerProducerDeclaration& GetDeclaration() const = 0;
	virtual bool ValidateSettings(const FString& NormalizedSettings, FString& OutError) const = 0;
	virtual bool HashUnitInputs(const FProjectWorldLayerInputs& Inputs,
		FProjectWorldLayerUnitInputs& OutUnits, FString& OutError) const = 0;
	virtual bool Apply(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
		FString& OutError) const = 0;
	virtual bool CaptureArtifacts(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldLayerInventory& Inventory, FProjectWorldRealizationResult& OutResult,
		FString& OutError) const = 0;
	virtual bool Delete(const FProjectWorldLayerApplyContext& Context,
		FProjectWorldRealizationResult& OutResult, FString& OutError) const;
};

namespace ProjectWorldLayerProducerRegistry
{
	PROJECTWORLDEDITOR_API const IProjectWorldLayerProducer* Find(
		const FString& GeneratorId, int32 GeneratorVersion, FString& OutError);
	PROJECTWORLDEDITOR_API bool GetAll(TArray<const IProjectWorldLayerProducer*>& OutProducers, FString& OutError);
	PROJECTWORLDEDITOR_API bool ValidateLayer(const FProjectWorldRealizationLayer& Layer, FString& OutError);
	PROJECTWORLDEDITOR_API bool FindActorOwner(const AActor& Actor,
		const IProjectWorldLayerProducer*& OutOwner, FString& OutError);
	PROJECTWORLDEDITOR_API bool SweepOwnedActors(UWorld& World,
		const FProjectWorldLayerActorOwnership& OwnedActors,
		FProjectWorldRealizationResult& OutResult, FString& OutError);
	PROJECTWORLDEDITOR_API bool AddPackageArtifact(const FString& PackageName, const FString& Kind,
		const FString& SemanticHash, FProjectWorldLayerInventory& Inventory, FString& OutError);
	PROJECTWORLDEDITOR_API bool ParseExactSettings(const FString& NormalizedSettings,
		TConstArrayView<const TCHAR*> Fields, TSharedPtr<FJsonObject>& OutSettings, FString& OutError);
}
