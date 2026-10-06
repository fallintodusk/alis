// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldLayerDirtyInput.h"
#include "Tests/ProjectWorldSchemaTestUtilities.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldRequestIdentityContractTest,
	"Project.World.Realization.Layers.RequestIdentityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldRequestIdentityContractTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("tmp/world/locality/s2_identity/request_contract/input.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Hash = FString::ChrN(64, TEXT('a'));
	const FString Fingerprint = FString::ChrN(64, TEXT('b'));
	const FString Valid = FString::Printf(
		TEXT("{\"$schema\":\"%s\",\"schema_version\":2,")
		TEXT("\"realization_profile_id\":\"fixture\",")
		TEXT("\"producer_fingerprints\":[{\"layer_id\":\"terrain\",\"generator_fingerprint\":\"%s\"}],")
		TEXT("\"base_layers\":[{\"layer_id\":\"terrain\",\"normalized_layer_contract_sha256\":\"%s\",\"canonical_inputs\":[]}],")
		TEXT("\"identity_dirty_layers\":[\"terrain\"],\"operator_additions\":[]}"),
		*ProjectWorldSchemaTestUtilities::ReferenceFor(
			Path, TEXT("project_world_layer_dirty_input.schema.json")),
		*Fingerprint, *Hash);
	auto Load = [&Path](const FString& Json, FProjectWorldLayerDirtyInput& Input, FString& Code)
	{
		FString Error;
		return FFileHelper::SaveStringToFile(Json, *Path) &&
			ProjectWorldLayerDirtyInput::Load(Path, Input, Code, Error);
	};
	FProjectWorldLayerDirtyInput Input;
	FString Code;
	TestTrue(TEXT("v2 request is accepted"), Load(Valid, Input, Code));
	TestEqual(TEXT("Fingerprint is retained"), Input.ProducerFingerprints.FindRef(TEXT("terrain")), Fingerprint);
	TestTrue(TEXT("Identity refresh is distinct from an operator addition"),
		Input.IdentityDirtyLayers.Contains(TEXT("terrain")) && Input.OperatorAdditions.IsEmpty());
	TestFalse(TEXT("v1 is rejected"),
		Load(Valid.Replace(TEXT("\"schema_version\":2"), TEXT("\"schema_version\":1")), Input, Code));
	TestFalse(TEXT("Unknown field is rejected"),
		Load(Valid.Replace(TEXT("\"operator_additions\":[]"),
			TEXT("\"operator_additions\":[],\"unknown\":1")), Input, Code));
	TestFalse(TEXT("Non-hex fingerprint is rejected"),
		Load(Valid.Replace(*Fingerprint, *FString::ChrN(64, TEXT('z'))), Input, Code));
	TestEqual(TEXT("Invalid fingerprint reports the identity boundary"), Code, TEXT("layer-dirty-input-identity"));
	const FString FingerprintItem = FString::Printf(
		TEXT("{\"layer_id\":\"terrain\",\"generator_fingerprint\":\"%s\"}"), *Fingerprint);
	TestFalse(TEXT("Duplicate fingerprint layer is rejected"),
		Load(Valid.Replace(
			*(FString(TEXT("\"producer_fingerprints\":[")) + FingerprintItem + TEXT("]")),
			*(FString(TEXT("\"producer_fingerprints\":[")) + FingerprintItem + TEXT(",") + FingerprintItem + TEXT("]"))),
			Input, Code));
	TestFalse(TEXT("Identity refresh needs a base layer"),
		Load(Valid.Replace(TEXT("\"identity_dirty_layers\":[\"terrain\"]"),
			TEXT("\"identity_dirty_layers\":[\"missing\"]")), Input, Code));
	IFileManager::Get().Delete(*Path, false, true, true);
	return true;
}

#endif
