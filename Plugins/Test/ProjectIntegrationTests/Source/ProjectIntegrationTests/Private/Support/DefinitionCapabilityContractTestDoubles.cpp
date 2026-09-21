// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Support/DefinitionCapabilityContractTestDoubles.h"

FPrimaryAssetId UProjectDefinitionCapabilityContractTestComponent::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(
		FPrimaryAssetType(TEXT("CapabilityComponent")),
		FName(TEXT("DefinitionCapabilityContractTest")));
}
