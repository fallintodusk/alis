#include "SinglePlayerGameMode.h"
#include "ProjectServiceLocator.h"
#include "Services/ILoadingService.h"
#include "Experience/ProjectExperienceDefinition.h"
#include "Experience/ProjectExperienceDefinitionDescriptor.h"
#include "Experience/ProjectExperienceRegistry.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	class FTimePolicyLoading final : public ILoadingService
	{
	public:
		const UWorld* World = nullptr;
		FName Experience;
		bool bLoading = false;
		virtual TSharedPtr<ILoadingHandle> StartLoad(const FLoadRequest&) override { return nullptr; }
		virtual bool CancelActiveLoad(bool) override { return false; }
		virtual bool IsLoadInProgress() const override { return bLoading; }
		virtual TSharedPtr<ILoadingHandle> GetActiveLoadHandle() const override { return nullptr; }
		virtual bool BuildLoadRequestForExperience(FName, FLoadRequest&, FText&) override { return false; }
		virtual FName GetLastLoadedExperienceName() const override { return Experience; }
		virtual bool TryGetLoadedExperienceContext(FName& OutName, const UWorld*& OutWorld) const override
		{ OutName = Experience; OutWorld = World; return World && !Experience.IsNone(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSinglePlayTimePolicyTest, "Project.Time.Controls.ModePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSinglePlayTimePolicyTest::RunTest(const FString&)
{
	const auto Previous = FProjectServiceLocator::Resolve<ILoadingService>();
	ON_SCOPE_EXIT
	{
		FProjectServiceLocator::Unregister<ILoadingService>();
		if (Previous) FProjectServiceLocator::Register<ILoadingService>(Previous);
	};
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Mode = World->SpawnActor<ASinglePlayerGameMode>();
	if (!TestNotNull(TEXT("Real single-player policy owner"), Mode)) return false;
	FProjectServiceLocator::Unregister<ILoadingService>();
	TestFalse(TEXT("Missing loading evidence denies"), Mode->AllowsEnvironmentalTimeControl());
	auto Loading = MakeShared<FTimePolicyLoading>();
	FProjectServiceLocator::Register<ILoadingService>(StaticCastSharedRef<ILoadingService>(Loading));
	auto* Definition = NewObject<UProjectExperienceDefinition>();
	Definition->ExperienceName = TEXT("TimePolicyFixture");
	Definition->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(World));
	auto* Descriptor = NewObject<UProjectExperienceDefinitionDescriptor>();
	TestTrue(TEXT("Configured descriptor projects"), Descriptor->InitializeFromDefinition(*Definition));
	UProjectExperienceRegistry::Get()->RegisterDescriptor(Descriptor);
	Loading->Experience = Definition->ExperienceName;
	Loading->World = World;
	TestFalse(TEXT("Configured default denies"), Mode->AllowsEnvironmentalTimeControl());
	Definition->AllowEnvironmentalTimeControls = true;
	Descriptor->InitializeFromDefinition(*Definition);
	TestTrue(TEXT("Resolved permitted experience and exact world admit"), Mode->AllowsEnvironmentalTimeControl());
	Loading->World = nullptr;
	World->URL.AddOption(TEXT("AllowEnvironmentalTimeControls=true"));
	TestFalse(TEXT("URL and matching map cannot replace successful-load world"), Mode->AllowsEnvironmentalTimeControl());
	Loading->World = World;
	Loading->bLoading = true;
	TestFalse(TEXT("In-progress travel denies previous admission"), Mode->AllowsEnvironmentalTimeControl());
	Loading->bLoading = false;
	Descriptor->LoadAssets.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Other.Other")));
	TestFalse(TEXT("Mismatched map denies"), Mode->AllowsEnvironmentalTimeControl());
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(FSinglePlayTimePolicyTest, "Project.Time.Controls.ModePolicy", "[Fast][Integration][Time]")
#endif
