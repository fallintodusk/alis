// Copyright ALIS. All Rights Reserved.

#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"
#include "Services/ObjectDefinitionCache.h"

void UProjectObjectDefinitionCacheSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Cache = NewObject<UObjectDefinitionCache>(this);
}

void UProjectObjectDefinitionCacheSubsystem::Deinitialize()
{
	Cache = nullptr;

	Super::Deinitialize();
}
