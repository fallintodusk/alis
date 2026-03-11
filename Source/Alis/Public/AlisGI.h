// Fill out your copyright notice in the Description page of Project Settings.

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
