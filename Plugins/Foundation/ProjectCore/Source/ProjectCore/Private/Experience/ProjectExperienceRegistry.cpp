// Copyright ALIS. All Rights Reserved.

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
