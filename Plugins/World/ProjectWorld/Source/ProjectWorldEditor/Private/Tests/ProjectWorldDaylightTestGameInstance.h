#pragma once

#include "Engine/GameInstance.h"
#include "ProjectWorldDaylightTestGameInstance.generated.h"

UCLASS()
class UProjectWorldDaylightTestGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	// Bind the fixture's existing context without bootstrapping unrelated game services.
	void BindFixtureContext(FWorldContext& InContext) { WorldContext = &InContext; }
};
