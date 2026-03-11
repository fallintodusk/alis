// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"
#include "ProjectExperienceDescriptorBase.h"
#include "ProjectExperienceRegistry.h"

namespace ProjectExperience
{
	inline TArray<TSubclassOf<UProjectExperienceDescriptorBase>>& GetPendingDescriptors()
	{
		static TArray<TSubclassOf<UProjectExperienceDescriptorBase>> Pending;
		return Pending;
	}

	inline void FlushPendingDescriptors()
	{
		if (UProjectExperienceRegistry* Registry = UProjectExperienceRegistry::Get())
		{
			TArray<TSubclassOf<UProjectExperienceDescriptorBase>>& Pending = GetPendingDescriptors();
			for (TSubclassOf<UProjectExperienceDescriptorBase> PendingClass : Pending)
			{
				if (PendingClass)
				{
					Registry->RegisterDescriptor(GetMutableDefault<UProjectExperienceDescriptorBase>(PendingClass));
				}
			}
			Pending.Reset();
		}
	}

	inline void RegisterDescriptorClass(UClass* DescriptorClass)
	{
		static bool bPostEngineBound = false;

		if (!UObjectInitialized())
		{
			// Queue until UObject system is ready.
			GetPendingDescriptors().Add(DescriptorClass);

			if (!bPostEngineBound)
			{
				bPostEngineBound = true;
				FCoreDelegates::OnPostEngineInit.AddStatic(&FlushPendingDescriptors);
			}
			return;
		}

		// Ensure any queued descriptors are flushed, then register this one immediately.
		FlushPendingDescriptors();

		if (DescriptorClass)
		{
			if (UProjectExperienceRegistry* Registry = UProjectExperienceRegistry::Get())
			{
				Registry->RegisterDescriptor(GetMutableDefault<UProjectExperienceDescriptorBase>(DescriptorClass));
			}
		}
	}

	template <typename TDescriptor>
	inline void RegisterDescriptor()
	{
		RegisterDescriptorClass(TDescriptor::StaticClass());
	}
} // namespace ProjectExperience
