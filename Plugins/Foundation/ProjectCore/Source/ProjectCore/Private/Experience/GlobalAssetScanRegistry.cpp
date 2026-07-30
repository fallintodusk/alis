// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/GlobalAssetScanRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogGlobalAssetScan, Log, All);

FGlobalAssetScanRegistry& FGlobalAssetScanRegistry::Get()
{
	static FGlobalAssetScanRegistry Instance;
	return Instance;
}

void FGlobalAssetScanRegistry::RegisterScanSpec(const FExperienceAssetScanSpec& Spec)
{
	if (Spec.PrimaryAssetType.IsNone() || Spec.Directories.Num() == 0)
	{
		UE_LOG(LogGlobalAssetScan, Warning,
			TEXT("RegisterScanSpec: Skipped invalid spec (Type=%s, Dirs=%d)"),
			*Spec.PrimaryAssetType.ToString(), Spec.Directories.Num());
		return;
	}

	if (IsDuplicate(Spec))
	{
		UE_LOG(LogGlobalAssetScan, Verbose,
			TEXT("RegisterScanSpec: Skipped duplicate spec (Type=%s)"),
			*Spec.PrimaryAssetType.ToString());
		return;
	}

	Specs.Add(Spec);

	UE_LOG(LogGlobalAssetScan, Log,
		TEXT("RegisterScanSpec: Type='%s', Dirs=%d, SpecCount=%d"),
		*Spec.PrimaryAssetType.ToString(), Spec.Directories.Num(), Specs.Num());
}

const TArray<FExperienceAssetScanSpec>& FGlobalAssetScanRegistry::GetAllSpecs() const
{
	return Specs;
}

int32 FGlobalAssetScanRegistry::GetSpecCount() const
{
	return Specs.Num();
}

bool FGlobalAssetScanRegistry::IsDuplicate(const FExperienceAssetScanSpec& Spec) const
{
	for (const FExperienceAssetScanSpec& Existing : Specs)
	{
		if (Existing.PrimaryAssetType == Spec.PrimaryAssetType
			&& Existing.Directories == Spec.Directories
			&& Existing.BaseClass == Spec.BaseClass
			&& Existing.bHasBlueprintClasses == Spec.bHasBlueprintClasses
			&& Existing.bIsEditorOnly == Spec.bIsEditorOnly
			&& Existing.bForceSynchronousScan == Spec.bForceSynchronousScan
			&& Existing.bRequireNonEmpty == Spec.bRequireNonEmpty)
		{
			return true;
		}
	}
	return false;
}
