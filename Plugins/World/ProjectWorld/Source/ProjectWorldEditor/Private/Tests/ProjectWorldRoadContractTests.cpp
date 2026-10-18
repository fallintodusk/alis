// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldRoadContractParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldRoadContractTest,
	"Project.World.Realization.Road.ElevationAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldRoadContractTest::RunTest(const FString& Parameters)
{
	const FString H = FString::ChrN(64, 'a');
	const FString Text = FString::Printf(LR"({"road_elevation_profile":{"version":1,"base_feature_sha256":"%s","support_terrain_artifacts":{"cell":"%s"},"surface":{"$schema":"https://alis.world/schemas/world-compiler/road-elevation-v1.json","schema_version":1,"interpolation":"affine_uv_triangles_v1","coordinate_rule":"source_arclength_m_signed_lateral_left_m","height_rule":"canonical_absolute_m_without_presentation_offset","model":"source_abutment_support_candidate_not_surveyed","source_feature_id":"road","source_points":[[0,0],[10,0]],"length_m":10,"width_m":4,"triangles":[[[0,-2,5],[10,-2,5],[0,2,5]],[[10,-2,5],[10,2,5],[0,2,5]]],"consumed_feature_sha256":"%s","consumed_terrain":{"cell":"%s"},"implementation_sha256":"%s","execution_identity":"%s"}}})", *H, *H, *H, *H, *H, *H);
	TSharedPtr<FJsonObject> Attributes;
	if (!TestTrue(TEXT("Fixture JSON parses."), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Attributes))) return false;
	auto Feature = [] { FProjectWorldCanonicalFeature F; F.FeatureId = TEXT("road"); F.FeatureClass = TEXT("road");
		F.GeometryType = TEXT("LineString"); F.WidthMeters = 4; F.GeometryPoints = {FVector2D(0, 0), FVector2D(10, 0)}; return F; };
	auto F = Feature(); FProjectWorldCanonicalValidation Validation;
	TestTrue(TEXT("Supported typed elevation is admitted."), ProjectWorldRoadContractParsing::Read(Attributes, F, Validation));
	TestTrue(TEXT("Road consumer receives the actual field."), F.RoadElevation.IsSet() && F.RoadElevation->Triangles.Num() == 2);
	for (const auto& Bad : {Text.Replace(TEXT("affine_uv_triangles_v1"), TEXT("future_v2")),
		Text.Replace(TEXT("source_abutment_support_candidate_not_surveyed"), TEXT("observed_constructed_tin_candidate_not_surveyed")),
		Text.Replace(TEXT("\"version\":1"), TEXT("\"version\":true")),
		Text.Replace(TEXT("\"surface\":{"), TEXT("\"surface\":null,\"unknown\":{")),
		Text.Replace(TEXT("\"length_m\":10"), TEXT("\"length_m\":\"10\"")),
		Text.Replace(TEXT("[0,-2,5]"), TEXT("[\"0\",-2,5]")),
		Text.Replace(TEXT("\"source_points\":[[0,0]"), TEXT("\"source_points\":[[0,1]"))})
	{
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bad), Attributes);
		F = Feature(); Validation = {};
		TestFalse(TEXT("Present malformed or unsupported field refuses, without drape fallback."),
			ProjectWorldRoadContractParsing::Read(Attributes, F, Validation));
		TestEqual(TEXT("Owning admission error."), Validation.ErrorCode, FString(TEXT("road-elevation-contract")));
	}
	F = Feature(); Attributes = MakeShared<FJsonObject>();
	TestTrue(TEXT("Absent profile preserves the current Road contract."), ProjectWorldRoadContractParsing::Read(Attributes, F, Validation));
	TestFalse(TEXT("Absent profile stays absent."), F.RoadElevation.IsSet());
	return true;
}
#endif
