// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Data/ObjectCatalog.h"

FPrimaryAssetId UObjectCatalog::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectCatalog")), GetFName());
}
