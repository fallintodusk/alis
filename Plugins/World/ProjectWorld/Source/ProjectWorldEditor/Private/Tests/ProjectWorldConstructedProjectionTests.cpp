// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldConstructedContractParsing.h"
#include "ProjectWorldCanonicalBundle.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedProjectionTest,
	"Project.World.Realization.Constructed.OperationalProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedProjectionTest::RunTest(const FString&)
{
	// Orthogonal synthetic axes isolate mechanical projection; this is no source/geographic admission proof.
	FString Text=TEXT(R"json({"constructed_surface":{"schema_version":1,"source_feature_id":"fixture:osm:way:1",
		"model_sha256":"MODEL","policy_sha256":"POLICY","canonical_crs":"EPSG:32618","vertical_datum":"EPSG:3855",
		"basis":"identified_model","role":"identified_assembly","triangles":[[[0,0,3],[1,0,3],[0,1,3]],[[1,0,3],[1,1,3],[0,1,3]]],
		"model_provenance":{"basis":"mechanical_projection_without_source_admission"},"source_bindings":[
		{"context_id":"fixture","provider_feature_id":"way/1","qualification_identity":"SOURCE","qualified":{},
		"projected_geometry":{"type":"LineString","coordinates":[[0,0],[10,0]]},
		"normalized_input":{"path":"refinement/inputs/SOURCE.json","sha256":"SOURCE","byte_size":1},"native_input":null},
		{"context_id":"fixture","provider_feature_id":"way/2","qualification_identity":"SOURCE","qualified":{},
		"projected_geometry":{"type":"LineString","coordinates":[[0,0],[0,10]]},
		"normalized_input":{"path":"refinement/inputs/SOURCE.json","sha256":"SOURCE","byte_size":1},"native_input":null}],
		"face_roles":[{"role":"gangway","source_ids":["way/1"],"triangles":[0],"axis_source_id":"way/1"},
		{"role":"gangway","source_ids":["way/2"],"triangles":[1],"axis_source_id":"way/2"}]}})json");
	Text=Text.Replace(TEXT("MODEL"),*FString::ChrN(64,'a'),ESearchCase::CaseSensitive)
		.Replace(TEXT("POLICY"),*FString::ChrN(64,'b'),ESearchCase::CaseSensitive)
		.Replace(TEXT("SOURCE"),*FString::ChrN(64,'c'),ESearchCase::CaseSensitive);
	TSharedPtr<FJsonObject> Attributes;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Attributes)) return false;
	auto Read = [&](FProjectWorldCanonicalFeature& Feature)
	{
		Feature={}; Feature.FeatureId=TEXT("fixture:osm:way:1"); Feature.FeatureClass=TEXT("constructed_surface");
		Feature.GeometryType=TEXT("Polygon"); FProjectWorldCanonicalValidation Validation;
		return ProjectWorldConstructedContractParsing::Read(Attributes,Feature,Validation);
	};
	FProjectWorldCanonicalFeature Feature;
	if (!TestTrue(TEXT("Distinct gangway regions project through the actual field parser."),Read(Feature))) return false;
	const auto& Regions=Feature.ConstructedSurface->FaceRegions;
	if (!TestEqual(TEXT("Equal role names do not merge different source regions."),Regions.Num(),2)) return false;
	TestTrue(TEXT("The first region retains its source face and canonical X axis."),
		Regions[0].SourceTriangles==TArray<int32>{0} && Regions[0].AxisEnd-Regions[0].AxisStart==FVector2D(10,0));
	TestTrue(TEXT("The second region retains its own source face and canonical Y axis."),
		Regions[1].SourceTriangles==TArray<int32>{1} && Regions[1].AxisEnd-Regions[1].AxisStart==FVector2D(0,10));
	const auto Surface=Attributes->GetObjectField(TEXT("constructed_surface"));
	auto Bindings=Surface->GetArrayField(TEXT("source_bindings"));
	auto Repeated=MakeShared<FJsonObject>(*Bindings[1]->AsObject()); Repeated->SetStringField(TEXT("context_id"),TEXT("another"));
	Bindings.Add(MakeShared<FJsonValueObject>(Repeated)); Surface->SetArrayField(TEXT("source_bindings"),Bindings);
	TestTrue(TEXT("Repeated literal source bindings with identical projected axes agree."),Read(Feature));
	auto Conflicting=MakeShared<FJsonObject>(*Repeated->GetObjectField(TEXT("projected_geometry")));
	TArray<TSharedPtr<FJsonValue>> Coordinates;
	for (const FVector2D Point : {FVector2D(0,0),FVector2D(10,0)})
		Coordinates.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(Point.X),MakeShared<FJsonValueNumber>(Point.Y)}));
	Conflicting->SetArrayField(TEXT("coordinates"),Coordinates); Repeated->SetObjectField(TEXT("projected_geometry"),Conflicting);
	TestFalse(TEXT("Contradictory projected axes for one bound source refuse."),Read(Feature));
	TestFalse(TEXT("A failed projection exposes no partial typed declaration."),Feature.ConstructedSurface.IsSet());
	const FString PolicyText=TEXT(R"json({"$schema":"https://alis.world/schemas/world-compiler/constructed-import-policy-v1.json",
		"schema_version":1,"maximum_triangles":4096,"maximum_ordinary_gradient":0.02,"maximum_gangway_grade":0.125,
		"maximum_cross_slope":0.02,"maximum_numeric_error_m":1e-8})json");
	auto PolicyRead = [](const FString& Json)
	{
		TSharedPtr<FJsonObject> Object; FProjectWorldCanonicalConstructedPolicy Policy;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object) &&
			ProjectWorldConstructedContractParsing::ReadPolicy(Object,Policy);
	};
	TestTrue(TEXT("The current closed policy contract projects without choosing new limits."),PolicyRead(PolicyText));
	for (const auto& Change : TArray<TPair<FString,FString>>{
		{TEXT(":4096"),TEXT(":\"4096\"")},{TEXT(":4096"),TEXT(":4095.5")},
		{TEXT(":0.02"),TEXT(":0.03")},{TEXT(":0.125"),TEXT(":0.15")},
		{TEXT(":1e-8"),TEXT(":NaN")},{TEXT(":1e-8"),TEXT(":1e400")}})
		TestFalse(TEXT("Policy projection refuses coercion, fractional counts, exceeded bounds and nonfinite values."),
			PolicyRead(PolicyText.Replace(*Change.Key,*Change.Value)));
	return !HasAnyErrors();
}
#endif
