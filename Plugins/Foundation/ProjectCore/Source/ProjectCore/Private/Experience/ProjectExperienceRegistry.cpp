// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Experience/ProjectExperienceRegistry.h"

#include "Experience/ProjectExperienceDescriptorBase.h"

UProjectExperienceRegistry* UProjectExperienceRegistry::Get()
{
	static UProjectExperienceRegistry* Singleton = nullptr;
	if (!Singleton)
	{
		Singleton = NewObject<UProjectExperienceRegistry>(GetTransientPackage(), TEXT("ProjectExperienceRegistry"));
		Singleton->AddToRoot();
	}
	return Singleton;
}

void UProjectExperienceRegistry::RegisterDescriptor(UProjectExperienceDescriptorBase* Descriptor)
{
	if (!Descriptor || Descriptor->ExperienceName.IsNone())
	{
		return;
	}

	// Root the descriptor rather than relying on reflection: see the Descriptors comment for
	// why this registry cannot hold reflected references to ordinary objects.
	if (!Descriptor->IsRooted())
	{
		Descriptor->AddToRoot();
	}
	Descriptors.Add(Descriptor->ExperienceName, Descriptor);
}

UProjectExperienceDescriptorBase* UProjectExperienceRegistry::FindDescriptor(FName ExperienceName) const
{
	if (UProjectExperienceDescriptorBase* const* Found = Descriptors.Find(ExperienceName))
	{
		return *Found;
	}
	return nullptr;
}
