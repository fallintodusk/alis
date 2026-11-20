#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ProjectTimeView.generated.h"

class UWorld;

struct FProjectTimeSnapshot
{
	FDateTime Utc;
	double Rate = 1.0;
	bool bDemo = false;
};

UINTERFACE(MinimalAPI)
class UProjectTimeView : public UInterface
{
	GENERATED_BODY()
};

/** Read-only environmental time for one explicitly bound world. */
class PROJECTCORE_API IProjectTimeView
{
	GENERATED_BODY()
public:
	virtual UWorld* GetTimeWorld() const = 0;
	virtual bool TryReadTime(FProjectTimeSnapshot& OutTime) = 0;
	virtual FSimpleMulticastDelegate& TimeChanged() = 0;
};

UINTERFACE(MinimalAPI)
class UProjectTimeControlPolicy : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by the resolved gameplay composition, never by a widget. */
class PROJECTCORE_API IProjectTimeControlPolicy
{
	GENERATED_BODY()
public:
	virtual bool AllowsEnvironmentalTimeControl() const = 0;
};
