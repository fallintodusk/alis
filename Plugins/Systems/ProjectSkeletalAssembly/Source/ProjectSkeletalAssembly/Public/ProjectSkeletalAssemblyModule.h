// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

PROJECTSKELETALASSEMBLY_API DECLARE_LOG_CATEGORY_EXTERN(LogSkeletalAssembly, Log, All);

class FProjectSkeletalAssemblyModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
