// Copyright ALIS. All Rights Reserved.

#include "ProjectServiceLocator.h"

FCriticalSection FProjectServiceLocator::Mutex;
TMap<FName, TSharedPtr<void>> FProjectServiceLocator::Services;

void FProjectServiceLocator::Reset()
{
	FScopeLock ScopeLock(&Mutex);
	Services.Reset();
}
