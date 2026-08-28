// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FProjectWorldPerformanceConsumerRegistration
{
public:
	void MarkRegistered()
	{
		check(!bRegistered);
		bRegistered = true;
	}

	bool Consume()
	{
		const bool bWasRegistered = bRegistered;
		bRegistered = false;
		return bWasRegistered;
	}

	bool IsRegistered() const
	{
		return bRegistered;
	}

private:
	bool bRegistered = false;
};
