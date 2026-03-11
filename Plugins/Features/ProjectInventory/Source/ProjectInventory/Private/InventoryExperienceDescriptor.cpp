#include "InventoryExperienceDescriptor.h"

#include "Types/ProjectLoadRequest.h"

UInventoryExperienceDescriptor::UInventoryExperienceDescriptor()
{
	ExperienceName = TEXT("ProjectInventory");
	// Add soft refs when inventory assets are ready (UI/data/etc.).
}

void UInventoryExperienceDescriptor::BuildLoadRequest(FLoadRequest& OutRequest) const
{
	Super::BuildLoadRequest(OutRequest);
}
