// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Components/ProjectInventoryComponent.h"
#include "Services/ObjectDefinitionCache.h"
#include "Subsystems/ProjectObjectDefinitionCacheSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWorld* ResolveInventoryCacheArchitectureWorld(FAutomationTestBase* Test)
{
	UWorld* World = AutomationCommon::GetAnyGameWorld();
	if (World)
	{
		return World;
	}

	if (!AutomationOpenMap(TEXT("/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent")))
	{
		if (Test)
		{
			Test->AddError(TEXT("Failed to open MainMenu_Persistent for inventory cache architecture test"));
		}
		return nullptr;
	}

	return AutomationCommon::GetAnyGameWorld();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDefinitionCacheSharedBindingTest,
	"ProjectIntegrationTests.Inventory.Cache.SharedGameInstanceBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDefinitionCacheSharedBindingTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryCacheArchitectureWorld(this);
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectObjectDefinitionCacheSubsystem* CacheSubsystem =
		GameInstance->GetSubsystem<UProjectObjectDefinitionCacheSubsystem>();
	if (!TestNotNull(TEXT("Cache subsystem should exist"), CacheSubsystem))
	{
		return false;
	}

	UObjectDefinitionCache* SharedCache = CacheSubsystem->GetCache();
	if (!TestNotNull(TEXT("Shared cache should exist"), SharedCache))
	{
		return false;
	}

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn"), OwnerActor))
	{
		return false;
	}

	UProjectInventoryComponent* FirstComponent = NewObject<UProjectInventoryComponent>(OwnerActor);
	UProjectInventoryComponent* SecondComponent = NewObject<UProjectInventoryComponent>(OwnerActor);
	OwnerActor->AddInstanceComponent(FirstComponent);
	OwnerActor->AddInstanceComponent(SecondComponent);
	FirstComponent->RegisterComponent();
	SecondComponent->RegisterComponent();

	TestEqual(TEXT("First component should bind the shared cache"), FirstComponent->GetObjectDefinitionCache(), SharedCache);
	TestEqual(TEXT("Second component should bind the shared cache"), SecondComponent->GetObjectDefinitionCache(), SharedCache);
	TestEqual(TEXT("Both components should share one cache"), FirstComponent->GetObjectDefinitionCache(), SecondComponent->GetObjectDefinitionCache());

	OwnerActor->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryDefinitionCacheRebindsFallbackTest,
	"ProjectIntegrationTests.Inventory.Cache.RebindsFallbackToSharedCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDefinitionCacheRebindsFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = ResolveInventoryCacheArchitectureWorld(this);
	if (!TestNotNull(TEXT("Automation world should exist"), World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!TestNotNull(TEXT("GameInstance should exist"), GameInstance))
	{
		return false;
	}

	UProjectObjectDefinitionCacheSubsystem* CacheSubsystem =
		GameInstance->GetSubsystem<UProjectObjectDefinitionCacheSubsystem>();
	if (!TestNotNull(TEXT("Cache subsystem should exist"), CacheSubsystem))
	{
		return false;
	}

	UObjectDefinitionCache* SharedCache = CacheSubsystem->GetCache();
	if (!TestNotNull(TEXT("Shared cache should exist"), SharedCache))
	{
		return false;
	}

	AActor* OwnerActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner actor should spawn"), OwnerActor))
	{
		return false;
	}

	UProjectInventoryComponent* Component = NewObject<UProjectInventoryComponent>(OwnerActor);
	UObjectDefinitionCache* LocalFallback = NewObject<UObjectDefinitionCache>(Component);
	Component->SetObjectDefinitionCache(LocalFallback);

	OwnerActor->AddInstanceComponent(Component);
	Component->RegisterComponent();

	TestNotEqual(TEXT("Component should not retain a component-local fallback cache"),
		Component->GetObjectDefinitionCache(), LocalFallback);
	TestEqual(TEXT("Component should rebind to the shared GameInstance cache"),
		Component->GetObjectDefinitionCache(), SharedCache);

	OwnerActor->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
