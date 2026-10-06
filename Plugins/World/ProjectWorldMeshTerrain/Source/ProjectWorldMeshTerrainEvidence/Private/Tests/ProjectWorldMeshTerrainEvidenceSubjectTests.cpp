#include "ProjectWorldMeshTerrainEvidenceSubject.h"

#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMeshTerrainEvidenceSubjectRegisteredTest,
	"Project.World.MeshTerrain.EvidenceSubjectRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectWorldMeshTerrainEvidenceSubjectRegisteredTest::RunTest(const FString& Parameters)
{
	const IProjectWorldEvidenceSubject* MeshTerrainSubject = &FProjectWorldMeshTerrainEvidenceSubject::Get();
	int32 Registrations = 0;
	{
		IModularFeatures::FScopedLockModularFeatureList ScopedLock;
		for (const IProjectWorldEvidenceSubject* Subject :
			IModularFeatures::Get().GetModularFeatureImplementations<IProjectWorldEvidenceSubject>(
				IProjectWorldEvidenceSubject::GetModularFeatureName()))
		{
			Registrations += Subject == MeshTerrainSubject ? 1 : 0;
		}
	}
	TestEqual(TEXT("The module registers the Mesh Terrain evidence subject exactly once"), Registrations, 1);

	UWorld* World = UWorld::CreateWorld(EWorldType::Inactive, false, TEXT("ProjectWorldMeshTerrainEvidenceSubjectTest"));
	if (!TestNotNull(TEXT("A transient world is created"), World))
	{
		return false;
	}
	const FProjectWorldEvidenceSubjectReport Report = MeshTerrainSubject->Inspect(*World);
	World->DestroyWorld(false);
	TestEqual(TEXT("The subject names its adapter"), Report.SubjectName, FString(TEXT("ProjectWorldMeshTerrain")));
	TestFalse(TEXT("A world without a Mesh Terrain partition reports the subject absent"), Report.bPresent);
	TestEqual(TEXT("An absent subject expects no unit"), Report.ExpectedUnits, 0);
	return true;
}

#endif
