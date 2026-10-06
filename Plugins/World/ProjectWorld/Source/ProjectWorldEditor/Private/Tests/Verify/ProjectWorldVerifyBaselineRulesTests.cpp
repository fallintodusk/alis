// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Utilities/ProjectSha256.h"

#if WITH_DEV_AUTOMATION_TESTS

// Baseline rules on a disposable baseline: compare never writes, record writes only a missing
// baseline or a changed verification identity, and refuses an unchanged identity.
namespace ProjectWorldVerifyBaselineRulesTests
{
	FString Root()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("tmp/world/realization_verify/baseline_rules/")));
	}

	FProjectWorldVerifyIdentity MakeIdentity()
	{
		FProjectWorldVerifyIdentity Identity;
		Identity.Verify = TEXT("Project.World.Realization.Verify.BaselineRules");
		Identity.GeneratorId = TEXT("baseline_rules_fixture");
		Identity.GeneratorVersion = 1;
		Identity.OutputRevision = 1;
		Identity.PipelineRevision = 1;
		Identity.ComparisonContractVersion = 1;
		Identity.EngineIdentity = FString::ChrN(64, TEXT('e'));
		Identity.AddFixtureText(TEXT("memory:fixture"), TEXT("first fixture"));
		Identity.BaselinePath = FPaths::Combine(Root(), TEXT("baseline_rules_fixture.verify.json"));
		Identity.DescriptorPath = TEXT("Data/Producers/baseline_rules_fixture.json");
		return Identity;
	}

	FProjectWorldProjection MakeProjection(const TCHAR* Geometry)
	{
		FProjectWorldProjection Projection;
		Projection.Add(TEXT("mesh"), TEXT("Cells/SM_Fixture")).Fields.Add(TEXT("mesh_description"), Geometry);
		FProjectWorldProjectionRecord& Actor = Projection.Add(TEXT("actor"), TEXT("fixture:cell"));
		Actor.Fields.Add(TEXT("transform"), ProjectWorldProjection::Transform(FTransform(FVector(1.0, 2.0, 3.0))));
		Actor.Fields.Add(TEXT("tags"), ProjectWorldProjection::Tags({TEXT("B"), TEXT("A")}));
		return Projection;
	}

	FString Digest(const FString& Path)
	{
		FString Hash;
		return FProjectSha256::HashFile(Path, Hash) ? Hash : FString(TEXT("absent"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldVerifyBaselineRulesTest,
	"Project.World.Realization.Verify.BaselineRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldVerifyBaselineRulesTest::RunTest(const FString& Parameters)
{
	using namespace ProjectWorldVerifyBaselineRulesTests;
	using ProjectWorldVerify::EMode;
	IFileManager::Get().DeleteDirectory(*Root(), false, true);
	const FProjectWorldVerifyIdentity Identity = MakeIdentity();
	const FProjectWorldProjection First = MakeProjection(TEXT("first"));
	const FProjectWorldProjection Changed = MakeProjection(TEXT("changed"));
	const FString& Path = Identity.BaselinePath;

	TestEqual(TEXT("Records serialize in (kind, key) order with sorted fields."), First.ToCanonicalJson(),
		FString(TEXT("[{\"kind\":\"actor\",\"key\":\"fixture:cell\",\"fields\":{\"tags\":\"A,B\",")
			TEXT("\"transform\":\"t=100,200,300;r=0,0,0,1000000;s=100000,100000,100000\"}},")
			TEXT("{\"kind\":\"mesh\",\"key\":\"Cells/SM_Fixture\",\"fields\":{\"mesh_description\":\"first\"}}]")));
	const TArray<FVector3f> Vertices = {FVector3f(0.0f, 0.0f, 0.0f), FVector3f(100.0f, 0.0f, 0.0f), FVector3f(0.0f, 100.0f, 0.0f)};
	TArray<FVector3f> Raised = Vertices;
	Raised[2].Z += 0.1f;
	const FString Collision = ProjectWorldProjection::TriMesh(Vertices, {0, 1, 2});
	TestEqual(TEXT("A collision digest is stable for the same triangles."), ProjectWorldProjection::TriMesh(Vertices, {0, 1, 2}), Collision);
	TestNotEqual(TEXT("A collision digest sees a 1 mm vertex move."), ProjectWorldProjection::TriMesh(Raised, {0, 1, 2}), Collision);
	TestNotEqual(TEXT("A collision digest sees a winding change."), ProjectWorldProjection::TriMesh(Vertices, {0, 2, 1}), Collision);
	FProjectWorldProjection Duplicate = MakeProjection(TEXT("first"));
	Duplicate.Add(TEXT("actor"), TEXT("fixture:cell"));
	FString DuplicateError;
	TestFalse(TEXT("A duplicate record key is an error."), Duplicate.IsValid(DuplicateError));

	ProjectWorldVerify::FResult Result = ProjectWorldVerify::CompareOrRecord(Identity, First, EMode::Compare);
	TestFalse(TEXT("Compare without a baseline fails."), Result.bPassed);
	TestTrue(TEXT("The missing-baseline failure names the record entry."), Result.Message.Contains(TEXT("record_verify_baseline.ps1")));
	TestFalse(TEXT("Compare without a baseline writes nothing."), IFileManager::Get().FileExists(*Path));

	Result = ProjectWorldVerify::CompareOrRecord(Identity, First, EMode::Record);
	TestTrue(TEXT("Record writes a missing baseline."), Result.bPassed);
	const FString Recorded = Digest(Path);
	TestEqual(TEXT("The recorded baseline exists."), Recorded.Len(), 64);
	FString Text;
	FFileHelper::LoadFileToString(Text, *Path);
	TestFalse(TEXT("The baseline is written with LF line endings."), Text.Contains(TEXT("\r")));
	TestTrue(TEXT("The baseline carries the producer id and the projection digest."),
		Text.Contains(TEXT("\"producer_id\": \"baseline_rules_fixture:v1\"")) &&
		Text.Contains(TEXT("\"projection_sha256\": \"") + First.Sha256() + TEXT("\"")));

	Result = ProjectWorldVerify::CompareOrRecord(Identity, First, EMode::Compare);
	TestTrue(TEXT("Same identity and same projection pass."), Result.bPassed);
	Result = ProjectWorldVerify::CompareOrRecord(Identity, Changed, EMode::Compare);
	TestFalse(TEXT("Same identity and changed projection fail."), Result.bPassed);
	TestTrue(TEXT("The failure names all three causes and the changed field."),
		Result.Message.Contains(TEXT("fixture or comparison change")) &&
		Result.Message.Contains(TEXT("producer output change")) &&
		Result.Message.Contains(TEXT("nondeterminism")) &&
		Result.Message.Contains(TEXT("mesh:Cells/SM_Fixture.mesh_description")));
	TestEqual(TEXT("Compare never writes the baseline."), Digest(Path), Recorded);

	Result = ProjectWorldVerify::CompareOrRecord(Identity, First, EMode::Record);
	TestFalse(TEXT("Record refuses an unchanged identity with unchanged output."), Result.bPassed);
	TestTrue(TEXT("The refusal says nothing is recorded."), Result.Message.Contains(TEXT("nothing to record")));
	Result = ProjectWorldVerify::CompareOrRecord(Identity, Changed, EMode::Record);
	TestFalse(TEXT("Record refuses an unchanged identity with changed output."), Result.bPassed);
	TestTrue(TEXT("The refusal names the three causes."), Result.Message.Contains(TEXT("record refused")) &&
		Result.Message.Contains(TEXT("producer output change")));
	TestEqual(TEXT("A refused record leaves the baseline bytes unchanged."), Digest(Path), Recorded);

	const FString ProbeRoot = FPaths::Combine(Root(), TEXT("probe"));
	TMap<FString, FString> Packages;
	Packages.Add(TEXT("L_Verify.umap"), FString::ChrN(64, TEXT('f')));
	Result = ProjectWorldVerify::WriteProbe(Identity, Changed, Packages, ProbeRoot);
	TArray<FString> ProbeFiles;
	IFileManager::Get().FindFiles(ProbeFiles, *FPaths::Combine(ProbeRoot, TEXT("*.json")), true, false);
	TestTrue(TEXT("Probe writes its projection and package digests."), Result.bPassed && ProbeFiles.Num() == 2);
	TestEqual(TEXT("Probe never writes the baseline."), Digest(Path), Recorded);

	FProjectWorldVerifyIdentity FixtureChanged = Identity;
	FixtureChanged.FixtureInputs.Reset();
	FixtureChanged.AddFixtureText(TEXT("memory:fixture"), TEXT("second fixture"));
	Result = ProjectWorldVerify::CompareOrRecord(FixtureChanged, Changed, EMode::Compare);
	TestFalse(TEXT("Compare fails on a changed fixture input."), Result.bPassed);
	TestTrue(TEXT("The failure names the changed identity component."),
		Result.Message.Contains(TEXT("verification identity differs")) && Result.Message.Contains(TEXT("memory:fixture")));
	TestEqual(TEXT("Compare on a changed identity writes nothing."), Digest(Path), Recorded);
	Result = ProjectWorldVerify::CompareOrRecord(FixtureChanged, Changed, EMode::Record);
	TestTrue(TEXT("A changed fixture input permits a re-record."), Result.bPassed);
	TestNotEqual(TEXT("The re-record replaces the baseline."), Digest(Path), Recorded);
	TestTrue(TEXT("The re-recorded baseline compares green."),
		ProjectWorldVerify::CompareOrRecord(FixtureChanged, Changed, EMode::Compare).bPassed);

	FProjectWorldVerifyIdentity ContractChanged = FixtureChanged;
	ContractChanged.ComparisonContractVersion = 2;
	const FString BeforeContract = Digest(Path);
	TestTrue(TEXT("A comparison-contract change permits a re-record."),
		ProjectWorldVerify::CompareOrRecord(ContractChanged, Changed, EMode::Record).bPassed);
	TestNotEqual(TEXT("The contract re-record updates the stored identity."), Digest(Path), BeforeContract);
	TestTrue(TEXT("The new identity compares green."),
		ProjectWorldVerify::CompareOrRecord(ContractChanged, Changed, EMode::Compare).bPassed);

	FString Tampered;
	FFileHelper::LoadFileToString(Tampered, *Path);
	Tampered.ReplaceInline(TEXT("\"changed\""), TEXT("\"edited\""));
	FFileHelper::SaveStringToFile(Tampered, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	Result = ProjectWorldVerify::CompareOrRecord(ContractChanged, Changed, EMode::Compare);
	TestFalse(TEXT("A hand-edited record fails closed."), Result.bPassed);
	TestTrue(TEXT("The failure names the digest mismatch."), Result.Message.Contains(TEXT("projection_sha256")));

	FProjectWorldVerifyIdentity Water;
	FString Error;
	TestTrue(TEXT("The water producer identity resolves from its descriptor."),
		FProjectWorldVerifyIdentity::ForProducer(
			TEXT("Project.World.Realization.Verify.Water"), TEXT("project_water_mesh"), 1, 1, Water, Error));
	TestTrue(TEXT("The identity carries revisions, the engine digest, and the owning plugin's baseline path."),
		Water.OutputRevision >= 1 && Water.PipelineRevision >= 1 && Water.EngineIdentity.Len() == 64 &&
		FPaths::GetCleanFilename(Water.BaselinePath) == TEXT("project_water_mesh.verify.json") &&
		Water.DescriptorPath == TEXT("Plugins/World/ProjectWorld/Data/Producers/project_water_mesh.json"));
	FProjectWorldVerifyIdentity Unknown;
	TestFalse(TEXT("An unknown producer fails closed."),
		FProjectWorldVerifyIdentity::ForProducer(TEXT("Unknown"), TEXT("unknown_producer"), 1, 1, Unknown, Error));
	TestFalse(TEXT("A producer version without a descriptor fails closed."),
		FProjectWorldVerifyIdentity::ForProducer(TEXT("Unknown"), TEXT("project_building_massing"), 1, 1, Unknown, Error));

	IFileManager::Get().DeleteDirectory(*Root(), false, true);
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldVerifyBaselineRulesTest,
	"Project.World.Realization.Verify.BaselineRules",
	"[Fast][Unit][World]")

#endif
