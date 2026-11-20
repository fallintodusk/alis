#include "ProjectLoadingSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectLoadingExperienceContextTest,
	"Project.Loading.Experience.ArrivalContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectLoadingExperienceContextTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>();
	auto* Loading = NewObject<UProjectLoadingSubsystem>(Instance);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Other = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); Other->DestroyWorld(false); };
	World->SetGameInstance(Instance);
	Other->SetGameInstance(NewObject<UGameInstance>());
	FLoadRequest Request;
	Request.ExperienceName = TEXT("ExplicitDemo");
	Request.MapSoftPath = FSoftObjectPath(World);
	FName Name;
	const UWorld* Loaded = nullptr;
	Loading->BeginExperienceContext(Request);
	Loading->CompleteExperienceContext(true);
	TestFalse(TEXT("Pipeline success before arrival cannot grant permission"),
		Loading->TryGetLoadedExperienceContext(Name, Loaded));
	Loading->HandleExperienceMapLoaded(Other);
	TestFalse(TEXT("Other GameInstance cannot complete context"),
		Loading->TryGetLoadedExperienceContext(Name, Loaded));
	Loading->HandleExperienceMapLoaded(World);
	TestTrue(TEXT("Matching arrival completes successful context"), Loading->TryGetLoadedExperienceContext(Name, Loaded));
	TestEqual(TEXT("Exact resolved experience retained"), Name, Request.ExperienceName);
	TestTrue(TEXT("Exact arrived world retained"), Loaded == World);
	Loading->ConfirmedExperienceName = NAME_None;
	Loading->ConfirmedExperienceWorld.Reset();
	Loading->BeginExperienceContext(Request);
	Loading->HandleExperienceMapLoaded(World);
	TestFalse(TEXT("Arrival before pipeline success cannot grant permission"),
		Loading->TryGetLoadedExperienceContext(Name, Loaded));
	Loading->CompleteExperienceContext(true);
	TestTrue(TEXT("Reverse event order also completes"), Loading->TryGetLoadedExperienceContext(Name, Loaded));
	Loading->BeginExperienceContext(Request);
	Loading->CompleteExperienceContext(false);
	Loading->HandleExperienceMapLoaded(World);
	TestTrue(TEXT("Failed request preserves prior confirmed context"), Loading->TryGetLoadedExperienceContext(Name, Loaded));
	Request.ExperienceName = NAME_None;
	Loading->ConfirmedExperienceName = NAME_None;
	Loading->ConfirmedExperienceWorld.Reset();
	Loading->BeginExperienceContext(Request);
	Loading->HandleExperienceMapLoaded(World);
	Loading->CompleteExperienceContext(true);
	TestFalse(TEXT("Unnamed map request never grants permission"), Loading->TryGetLoadedExperienceContext(Name, Loaded));
	return true;
}
REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FProjectLoadingExperienceContextTest,
	"Project.Loading.Experience.ArrivalContext", "[Fast][Integration][Loading]")
#endif
