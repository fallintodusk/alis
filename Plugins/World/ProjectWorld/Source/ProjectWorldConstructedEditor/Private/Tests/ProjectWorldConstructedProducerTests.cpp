// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldLayerProducerRegistry.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedProducerContractTest,
	"Project.World.Realization.Constructed.RegisteredProducerContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedProducerContractTest::RunTest(const FString&)
{
	FString Error;
	const auto* Producer=ProjectWorldLayerProducerRegistry::Find(TEXT("project_constructed_surface"),1,Error);
	if (!TestNotNull(TEXT("Source-bound structural fields have a registered physical owner."),Producer))
	{ AddInfo(Error); return false; }
	const auto& Declaration=Producer->GetDeclaration();
	TestTrue(TEXT("Physical structures remain generated geography."),Declaration.LayerKind==EProjectWorldLayerKind::GeneratedGeography);
	TestTrue(TEXT("The existing planner receives actual canonical-cell dependency units."),
		Declaration.DirtyGranularity==EProjectWorldDirtyGranularity::CanonicalCell);
	TestTrue(TEXT("Structural selection uses its own closed canonical domain."),
		Declaration.CanonicalSelectors==TArray<FString>{TEXT("constructed_surfaces")});
	return !HasAnyErrors();
}
#endif
