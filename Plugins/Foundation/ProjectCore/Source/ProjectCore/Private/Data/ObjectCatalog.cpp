// Copyright ALIS. All Rights Reserved.

#include "Data/ObjectCatalog.h"

FPrimaryAssetId UObjectCatalog::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ObjectCatalog")), GetFName());
}
