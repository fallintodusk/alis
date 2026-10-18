#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeometryParsing.h"
#include "ProjectWorldWaterContractParsing.h"

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Result;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldNativeWaterCanonicalContractTwinTest,
	"Project.World.Realization.NativeTwin.WaterCanonicalContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldNativeWaterCanonicalContractTwinTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Geometry = ParseObject(TEXT(R"json(
		{"type":"Polygon","coordinates":[
			[[10,8],[55,8],[55,22],[10,22],[10,8]],
			[[25.25,12.25],[25.25,18.25],[40.25,18.25],[40.25,12.25],[25.25,12.25]]
		]})json"));
	FProjectWorldCanonicalFeature Lake;
	Lake.FeatureClass = TEXT("water");
	FProjectWorldCanonicalValidation Validation;
	TestTrue(TEXT("The canonical parser accepts the polygon with a hole."),
		ProjectWorldGeometryParsing::ReadGeometry(
			Geometry, Lake.GeometryType, Lake.GeometryPoints, Validation,
			&Lake.GeometryParts, &Lake.GeometryPolygons));
	TestEqual(TEXT("The canonical lake remains one polygon."), Lake.GeometryPolygons.Num(), 1);
	if (!Lake.GeometryPolygons.IsEmpty())
	{
		TestEqual(TEXT("The canonical hole survives loading."), Lake.GeometryPolygons[0].Holes.Num(), 1);
	}
	const TSharedPtr<FJsonObject> Standing = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"standing_lake",
			"surface_group_members":["way/1001"],
			"surface_geometry":"polygon",
			"surface_behavior":"standing",
			"surface_role":"area",
			"surface_function":{"function_id":"standing_polygon_quantile","function_version":1,"level_m":27.5}
		})json"));
	TestTrue(TEXT("The loader retains standing water authority."),
		ProjectWorldWaterContractParsing::Read(Standing, Lake, Validation));
	TestTrue(TEXT("Standing water authority is valid."), Lake.WaterSurface.bValid);
	TestEqual(TEXT("The standing function version survives loading."), Lake.WaterSurface.FunctionVersion, 1);
	TestEqual(TEXT("The standing level survives loading."), Lake.WaterSurface.LevelMeters, 27.5);

	FProjectWorldCanonicalFeature River;
	River.FeatureClass = TEXT("water");
	const TSharedPtr<FJsonObject> Flowing = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"cross_cell_river",
			"surface_group_members":["relation/1002","way/1003"],
			"surface_geometry":"polygon",
			"surface_behavior":"flowing",
			"surface_role":"area",
			"surface_function":{"function_id":"rolling_quantile_l1_isotonic","function_version":1,
				"knots":[[25,37.5,26.8],[65,37.5,25.1],[125,37.5,21.8]]}
		})json"));
	TestTrue(TEXT("The loader retains flowing water authority."),
		ProjectWorldWaterContractParsing::Read(Flowing, River, Validation));
	TestEqual(TEXT("All flowing knots survive loading."), River.WaterSurface.Knots.Num(), 3);
	TestEqual(TEXT("The flowing function version survives loading."), River.WaterSurface.FunctionVersion, 1);
	TestTrue(TEXT("The flowing authority slopes downstream."),
		River.WaterSurface.Knots.Num() == 3 &&
		River.WaterSurface.Knots[0].Z > River.WaterSurface.Knots.Last().Z);

	const TSharedPtr<FJsonObject> FutureFunction = ParseObject(TEXT(R"json(
		{
			"surface_group_id":"future-lake",
			"surface_group_members":["way/future"],
			"surface_geometry":"polygon",
			"surface_behavior":"standing",
			"surface_role":"area",
			"surface_function":{"function_id":"standing_polygon_quantile","function_version":2,"level_m":27.5}
		})json"));
	FProjectWorldCanonicalFeature FutureLake;
	FutureLake.FeatureClass = TEXT("water");
	FProjectWorldCanonicalValidation FutureValidation;
	TestFalse(TEXT("An unsupported water function version is rejected."),
		ProjectWorldWaterContractParsing::Read(FutureFunction, FutureLake, FutureValidation));
	return true;
}

REGISTER_SIMPLE_AUTOMATION_TEST_TAGS(
	FProjectWorldNativeWaterCanonicalContractTwinTest,
	"Project.World.Realization.NativeTwin.WaterCanonicalContract",
	"[Fast][Architecture][World]")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstrainedWaterFunctionTest,
	"Project.World.Realization.NativeTwin.WaterConstrainedFunctionAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstrainedWaterFunctionTest::RunTest(const FString& Parameters)
{
	const FString Template = TEXT(R"json({"surface_group_id":"fixture-channel","surface_group_members":["way/1"],
		"surface_geometry":"ribbon","surface_behavior":"flowing","surface_role":"flow_axis",
		"surface_function":{"function_id":"constrained_flowing_knots","function_version":1,
		"knots":[[0,0,54],[5,0,53.5],[10,0,53.2]]}})json");
	auto Read = [](const FString& Text, FProjectWorldCanonicalFeature& Out)
	{
		Out.FeatureClass = TEXT("water");
		FProjectWorldCanonicalValidation Validation;
		return ProjectWorldWaterContractParsing::Read(ParseObject(Text), Out, Validation);
	};
	FProjectWorldCanonicalFeature Constrained, Original;
	TestTrue(TEXT("A known constrained channel function is admitted."), Read(Template, Constrained));
	TestTrue(TEXT("The original estimator family remains admitted."),
		Read(Template.Replace(TEXT("constrained_flowing_knots"), TEXT("rolling_quantile_l1_isotonic")), Original));
	TestTrue(TEXT("The shared knot representation is retained exactly."), Original.WaterSurface.Knots == Constrained.WaterSurface.Knots);
	TestTrue(TEXT("Different canonical model identities remain distinct."), Original.WaterSurface.FunctionId != Constrained.WaterSurface.FunctionId);
	for (const FString& Family : {FString(TEXT("constrained_flowing_knots")), FString(TEXT("rolling_quantile_l1_isotonic"))})
	{
		const FString Valid = Template.Replace(TEXT("constrained_flowing_knots"), *Family);
		for (const TCHAR* Version : {TEXT("1.5"), TEXT("2"), TEXT("-1"), TEXT("\"1\""), TEXT("true"), TEXT("null")})
		{
			FProjectWorldCanonicalFeature Rejected;
			TestFalse(*FString::Printf(TEXT("%s refuses non-current integer version %s."), *Family, Version),
				Read(Valid.Replace(TEXT("\"function_version\":1"), *(FString(TEXT("\"function_version\":")) + Version)), Rejected));
		}
		for (const TCHAR* Knot : {TEXT("[5,0,\"53.5\"]"), TEXT("[5,0,true]"), TEXT("[5,0,55]")})
		{
			FProjectWorldCanonicalFeature Rejected;
			TestFalse(*FString::Printf(TEXT("%s refuses malformed or increasing knot %s."), *Family, Knot),
				Read(Valid.Replace(TEXT("[5,0,53.5]"), Knot), Rejected));
		}
	}
	FProjectWorldCanonicalFeature Unknown;
	TestFalse(TEXT("Unknown constrained estimators are not a generic admission escape."),
		Read(Template.Replace(TEXT("constrained_flowing_knots"), TEXT("unknown_flowing_knots")), Unknown));
	return true;
}

#endif
