// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldGeometryParsing.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldMultiPolygonPartsTest,
	"Project.World.Realization.Geometry.MultiPolygonPartsPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldMultiPolygonPartsTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT("{\"type\":\"MultiPolygon\",\"coordinates\":[[[[0,0],[2,0],[2,2],[0,0]]],[[[6,0],[8,0],[8,2],[6,0]]]]}");
	TSharedPtr<FJsonObject> Geometry;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("MultiPolygon fixture parses as JSON."), FJsonSerializer::Deserialize(Reader, Geometry));
	if (!Geometry.IsValid())
	{
		return false;
	}

	FString Type;
	TArray<FVector2D> FlattenedPoints;
	TArray<TArray<FVector2D>> Parts;
	FProjectWorldCanonicalValidation Validation;
	TestTrue(
		TEXT("Canonical geometry parser accepts the MultiPolygon."),
		ProjectWorldGeometryParsing::ReadGeometry(
			Geometry,
			Type,
			FlattenedPoints,
			Validation,
			&Parts));
	TestEqual(TEXT("Both disjoint polygon parts remain explicit."), Parts.Num(), 2);
	TestEqual(TEXT("Flattened compatibility points remain available."), FlattenedPoints.Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectWorldPointGeometryTest,
	"Project.World.Realization.Geometry.PointInputsAccepted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectWorldPointGeometryTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Fixtures = {
		TEXT("{\"type\":\"Point\",\"coordinates\":[1,2]}"),
		TEXT("{\"type\":\"MultiPoint\",\"coordinates\":[[1,2],[3,4]]}"),
	};
	for (const FString& Json : Fixtures)
	{
		TSharedPtr<FJsonObject> Geometry;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		TestTrue(TEXT("Point fixture parses as JSON."), FJsonSerializer::Deserialize(Reader, Geometry));
		FString Type;
		TArray<FVector2D> Points;
		TArray<TArray<FVector2D>> Parts;
		FProjectWorldCanonicalValidation Validation;
		TestTrue(
			TEXT("Canonical geometry parser accepts the point input."),
			ProjectWorldGeometryParsing::ReadGeometry(Geometry, Type, Points, Validation, &Parts));
		TestEqual(TEXT("Every point remains explicit."), Points.Num(), Parts.Num());
	}
	return true;
}

#endif
