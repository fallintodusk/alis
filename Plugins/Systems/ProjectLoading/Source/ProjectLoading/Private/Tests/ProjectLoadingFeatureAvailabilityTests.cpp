// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "Misc/AutomationTest.h"
#include "Executors/ActivateFeaturesPhaseExecutor.h"
#include "Interfaces/IOrchestratorRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
class FScopedOrchestratorRegistry final : public IOrchestratorRegistry
{
public:
	explicit FScopedOrchestratorRegistry(TSet<FName> InAvailableFeatures)
		: PreviousRegistry(GetOrchestratorRegistry())
		, AvailableFeatures(MoveTemp(InAvailableFeatures))
	{
		SetOrchestratorRegistry(this);
	}

	~FScopedOrchestratorRegistry() override
	{
		SetOrchestratorRegistry(PreviousRegistry);
	}

	bool IsFeatureAvailable(FName PluginName) const override
	{
		return AvailableFeatures.Contains(PluginName);
	}

	TArray<FName> GetLoadedFeatures() const override
	{
		return AvailableFeatures.Array();
	}

	FString GetFeatureVersion(FName PluginName) const override
	{
		return IsFeatureAvailable(PluginName) ? TEXT("test") : FString();
	}

private:
	IOrchestratorRegistry* PreviousRegistry = nullptr;
	TSet<FName> AvailableFeatures;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectLoadingFeatureAvailabilityTest,
	"ProjectLoading.Unit.FeatureAvailability.OrchestratorRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectLoadingFeatureAvailabilityTest::RunTest(const FString& Parameters)
{
	FActivateFeaturesPhaseExecutor Executor;
	FLoadRequest EmptyRequest;
	TestTrue(TEXT("Phase skips when no features are requested."), Executor.ShouldSkip(EmptyRequest));

	FProjectPhaseContext Context;
	Context.Request.FeaturesToActivate = {TEXT("ProjectCombat"), TEXT("Combat")};
	TestFalse(TEXT("Phase runs when features are requested."), Executor.ShouldSkip(Context.Request));

	{
		FScopedOrchestratorRegistry Registry({FName(TEXT("ProjectCombat"))});
		AddExpectedError(TEXT("Required feature unavailable: Combat"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Required features unavailable: Combat"), EAutomationExpectedErrorFlags::Contains, 1);
		const FProjectPhaseResult Result = Executor.Execute(Context);
		TestFalse(TEXT("Gameplay feature identities do not satisfy plugin readiness."), Result.bSuccess);
		TestEqual(TEXT("Feature validation reports its owned error code."), Result.ErrorCode,
			ProjectLoadingErrors::FeatureActivationFailed);
	}

	Context.Request.FeaturesToActivate = {TEXT("ProjectCombat")};
	{
		FScopedOrchestratorRegistry Registry({FName(TEXT("ProjectCombat"))});
		const FProjectPhaseResult Result = Executor.Execute(Context);
		TestTrue(TEXT("Ready plugin identities pass."), Result.bSuccess);
	}

	IOrchestratorRegistry* PreviousRegistry = GetOrchestratorRegistry();
	SetOrchestratorRegistry(nullptr);
	AddExpectedError(TEXT("Feature readiness authority unavailable."), EAutomationExpectedErrorFlags::Contains, 1);
	const FProjectPhaseResult MissingRegistryResult = Executor.Execute(Context);
	SetOrchestratorRegistry(PreviousRegistry);
	TestFalse(TEXT("Missing readiness authority fails closed."), MissingRegistryResult.bSuccess);

	return true;
}

#endif
