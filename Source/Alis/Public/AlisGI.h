// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"


#include "AlisGI.generated.h"

/**
 * 
 */
UCLASS()
class ALIS_API UAlisGI : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
};
