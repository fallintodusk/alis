// Copyright ALIS. All Rights Reserved.

#include "CapabilityValidationRegistry.h"
#include "ProjectLogging.h"

TMap<FName, FCapabilityValidateFunc> FCapabilityValidationRegistry::ValidationRegistry;

void FCapabilityValidationRegistry::RegisterValidation(FName CapabilityId, FCapabilityValidateFunc&& ValidateFunc)
{
	if (ValidateFunc)
	{
		ValidationRegistry.Add(CapabilityId, MoveTemp(ValidateFunc));
		UE_LOG(LogProjectCore, Verbose, TEXT("Registered capability validation: %s"), *CapabilityId.ToString());
	}
}

const FCapabilityValidateFunc* FCapabilityValidationRegistry::GetValidationFunc(FName CapabilityId)
{
	return ValidationRegistry.Find(CapabilityId);
}

bool FCapabilityValidationRegistry::HasValidation(FName CapabilityId)
{
	return ValidationRegistry.Contains(CapabilityId);
}
