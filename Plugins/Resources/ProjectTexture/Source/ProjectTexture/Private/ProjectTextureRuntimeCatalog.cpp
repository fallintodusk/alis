// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectTextureRuntimeCatalog.h"

#include "ProjectTextureRuntimeSubsystem.h"

void UProjectTextureOutputSlot::PostLoad()
{
	Super::PostLoad();
	UProjectTextureRuntimeSubsystem::NotifyOutputSlotLoaded(this);
}

UTextureRenderTarget2D* UProjectTextureOutputSlot::GetRenderTarget() const
{
	return Cast<UTextureRenderTarget2D>(GetOuter());
}
