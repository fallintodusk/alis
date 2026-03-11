#include "Layout/ProjectUIScreenLayoutData.h"

UProjectUIScreenLayoutData::UProjectUIScreenLayoutData()
	: InputConfigId(NAME_None)
{
}

FPrimaryAssetId UProjectUIScreenLayoutData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ProjectUIScreenLayout"), GetFName());
}
