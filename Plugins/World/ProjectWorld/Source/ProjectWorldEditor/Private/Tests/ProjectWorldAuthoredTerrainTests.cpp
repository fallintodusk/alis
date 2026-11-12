#include "ProjectWorldAuthoredTerrainParsing.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldAuthoredTerrainMetadataTest,
	"Project.World.Terrain.AuthoredDryMetadata",EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldAuthoredTerrainMetadataTest::RunTest(const FString&)
{
	using namespace ProjectWorldAuthoredTerrainParsing;
	const FString Hash=FString::ChrN(64,'a'),Grid=TEXT("grid_0123456789abcdef");
	auto Parse=[](const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object);return Object;
	};
	const FString InputText=TEXT(R"({"$schema":"https://alis.world/schemas/world-compiler/authored-overlay-v2.json","schema_version":2,"overlay_id":"ground","grid_id":"grid_0123456789abcdef","provenance":[{"provenance_id":"operator","authority":"authored_operator"}],"terrain_patches":[{"patch_id":"ground","provenance_ref":"operator","center":[10,20],"radius_m":2,"delta_m":1}],"feature_overrides":[],"missing_target_policy":"reject_and_require_rebase"})");
	const auto Operation=Parse(TEXT(R"({"kind":"authored_dry_terrain","feature_ids":[],"base_features":{},"input":{}})"));
	FRetainedInputs Inputs;
	if (!TestTrue(TEXT("The existing height-only authored contract retains its authenticated input association."),ReadInput(Operation,Parse(InputText),Hash,Inputs))) return false;
	const FString WrapperText=FString::Printf(TEXT(R"({"$schema":"https://alis.world/schemas/world-compiler/terrain-refinement-v1.json","schema_version":1,"grid_id":"grid_0123456789abcdef","cell_id":"cell","base":{},"coordinate_rule":"canonical_boundary_intersections_float64","construction":"authored_dry_terrain_v1","consumed":{"authored_input_sha256":"%s","domain_sha256":"%s","implementation_sha256":"%s","execution_identity":"%s"},"policy":{"height_rule":"quantized_sum_on_immutable_final_grid_v1"},"metrics":{"changed_nodes":1,"affected_quads":1,"maximum_cut_m":0,"maximum_fill_m":1},"quads":[{}]})"),*Hash,*Hash,*Hash,*Hash);
	TestTrue(TEXT("Supported metadata binds to the retained authored input and grid."),ReadTerrain(Parse(WrapperText),Grid,Inputs));
	TestFalse(TEXT("An authenticated wrapper cannot substitute another retained-input hash."),ReadTerrain(Parse(WrapperText),Grid,{}));
	TestFalse(TEXT("An authored input from another grid cannot supply this Terrain."),ReadTerrain(Parse(WrapperText),TEXT("grid_fedcba9876543210"),Inputs));
	TestFalse(TEXT("An unused authored operation cannot decorate an unchanged child."),ValidateUsage(Inputs,Grid,{}));
	TestTrue(TEXT("Each declared authored operation supplies final Terrain in the coverage grid."),ValidateUsage(Inputs,Grid,{Hash}));
	for (const FString& Invalid : {
		InputText.Replace(TEXT("\"provenance_ref\":\"operator\""),TEXT("\"provenance_ref\":\"missing\"")),
		InputText.Replace(TEXT("\"delta_m\":1"),TEXT("\"delta_m\":1,\"surface_weights\":{\"ground\":1,\"hydro_transition\":0}")),
		InputText.Replace(TEXT("\"feature_overrides\":[]"),TEXT("\"feature_overrides\":[{\"feature_id\":\"water\",\"set\":{\"level\":1}}]"))})
	{
		FRetainedInputs Rejected;
		TestFalse(TEXT("The height-only input refuses unsupported effects or undeclared provenance."),ReadInput(Operation,Parse(Invalid),Hash,Rejected));
		TestTrue(TEXT("A refused input cannot leave a retained association."),Rejected.IsEmpty());
	}
	for (const FString& Invalid : {
		WrapperText.Replace(TEXT("\"affected_quads\":1"),TEXT("\"affected_quads\":2")),
		WrapperText.Replace(TEXT("\"height_rule\":\"quantized_sum_on_immutable_final_grid_v1\""),TEXT("\"height_rule\":\"unknown\"")),
		WrapperText.Replace(TEXT("\"maximum_fill_m\":1"),TEXT("\"maximum_fill_m\":-1")),
		WrapperText.Replace(TEXT("\"construction\":\"authored_dry_terrain_v1\""),TEXT("\"construction\":\"authored_dry_terrain_v2\""))})
		TestFalse(TEXT("The metadata boundary refuses unsupported policy, counts, metrics and constructions."),ReadTerrain(Parse(Invalid),Grid,Inputs));
	return !HasAnyErrors();
}
#endif
