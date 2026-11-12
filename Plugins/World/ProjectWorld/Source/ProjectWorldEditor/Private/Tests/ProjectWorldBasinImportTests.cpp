// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ProjectWorldVerify.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	bool ScratchReceipt(FString& Path,const FString& Expected)
	{
		if (FPaths::IsRelative(Path)) Path=FPaths::ProjectDir()/Path;
		Path=FPaths::ConvertRelativePathToFull(Path);
		FString Actual;
		return FPaths::IsUnderDirectory(Path,FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("tmp"))) &&
			FProjectWorldCanonicalLoader::ComputeFileSha256(Path,Actual) && Actual==Expected;
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBasinImportTest,
	"Project.World.Realization.Water.CanonicalBasinBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBasinImportTest::RunTest(const FString&)
{
	FString Input,Expected,Actual;
	if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldBasinImport="),Input) ||
		!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldBasinImportHash="),Expected) ||
		!ScratchReceipt(Input,Expected))
	{AddError(TEXT("Supply the fresh scratch-only joint basin child and its exact receipt hash."));return false;}
	FProjectWorldCanonicalBundle Bundle; FProjectWorldCanonicalValidation Validation;
	if (!TestTrue(TEXT("One source-shaped child admits modeled Water and final Terrain together."),
		FProjectWorldCanonicalLoader::Load(Input,Bundle,Validation)))
	{AddError(Validation.ErrorCode+TEXT(": ")+Validation.Message+TEXT(" [")+Validation.Detail+TEXT("]"));return false;}
	TestEqual(TEXT("Both original mechanical cells survive admission."),Bundle.Cells.Num(),2);
	TestEqual(TEXT("The original source Water has one final feature."),Bundle.Features.Num(),1);
	const auto* Feature=Bundle.Features.Find(TEXT("alis:osm:way:394395237"));
	if (!TestNotNull(TEXT("The original source ID remains the modeled Water owner."),Feature)) return false;
	TestEqual(TEXT("Inferred Water keeps its explicit versioned identity."),Feature->WaterSurface.FunctionId,FString(TEXT("identified_static_lined_basin")));
	TestEqual(TEXT("The declared static model stage survives JSON parsing."),Feature->WaterSurface.LevelMeters,20.2);
	TestTrue(TEXT("The inferred spill is retained separately from source observations."),Feature->WaterSurface.ContainmentSpillMeters.IsSet());
	TestTrue(TEXT("Native parsing retains the distinct inferred admission contract."),Feature->WaterSurface.BasinAdmission.IsSet());
	FProjectWorldVerifyIdentity Before,After;
	Before.AddFixtureBundle(TEXT("joint-basin"),Bundle);
	Bundle.Features[Feature->FeatureId].WaterSurface.BasinAdmission->AdmissionHash=FString::ChrN(64,'f');
	After.AddFixtureBundle(TEXT("joint-basin"),Bundle);
	TestTrue(TEXT("Admission changes invalidate verification evidence."),Before.FixtureInputs[0].Value!=After.FixtureInputs[0].Value);
	FString Error;
	for (const auto& Cell : Bundle.Cells)
	{
		TArray<FProjectWorldSurfaceTriangle> Faces;
		const FBox2D Bounds(FVector2D(Cell.Bounds.X,Cell.Bounds.Y),FVector2D(Cell.Bounds.Z,Cell.Bounds.W));
		if (!TestTrue(TEXT("Each joint cell exposes its final refined field."),ProjectWorldTerrainSurface::Triangles(Cell,Bounds,Faces,Error)))
		{AddError(Error);return false;}
		TestTrue(TEXT("The inferred Terrain is more than its two original faces."),Faces.Num()>2);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldBasinRefusalTest,
	"Project.World.Realization.Water.CanonicalBasinRefusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldBasinRefusalTest::RunTest(const FString&)
{
	FString Input,Expected,Text;
	TSharedPtr<FJsonObject> Manifest;
	if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldBasinControls="),Input) ||
		!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldBasinControlsHash="),Expected) || !ScratchReceipt(Input,Expected) ||
		!FFileHelper::LoadFileToString(Text,*Input) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest))
	{AddError(TEXT("Supply freshly exported joint-basin controls and the exact manifest hash."));return false;}
	const auto Positive=Manifest->GetObjectField(TEXT("positive"));
	FString Path=Positive->GetStringField(TEXT("receipt"));
	if (!TestTrue(TEXT("Positive control receipt is authenticated."),ScratchReceipt(Path,Positive->GetStringField(TEXT("sha256"))))) return false;
	FProjectWorldCanonicalBundle PositiveBundle; FProjectWorldCanonicalValidation PositiveValidation;
	if (!TestTrue(TEXT("The unmodified control reaches full native admission."),FProjectWorldCanonicalLoader::Load(Path,PositiveBundle,PositiveValidation)))
	{AddError(PositiveValidation.ErrorCode+TEXT(": ")+PositiveValidation.Message+TEXT(" [")+PositiveValidation.Detail+TEXT("]"));return false;}
	const auto& Controls=Manifest->GetArrayField(TEXT("controls"));
	TestEqual(TEXT("The fixture exercises all distinct refusal paths."),Controls.Num(),15);
	for (const auto& Value : Controls)
	{
		const auto Control=Value->AsObject();
		Path=Control->GetStringField(TEXT("receipt"));
		const FString Name=Control->GetStringField(TEXT("name")),Code=Control->GetStringField(TEXT("error_code"));
		if (!TestTrue(*Name,ScratchReceipt(Path,Control->GetStringField(TEXT("sha256"))))) return false;
		FString BeforeHash,AfterHash;
		FProjectWorldCanonicalLoader::ComputeFileSha256(Path,BeforeHash);
		FProjectWorldCanonicalBundle Refused; FProjectWorldCanonicalValidation Validation;
		TestFalse(*FString::Printf(TEXT("Resealed invalid joint child refuses: %s"),*Name),FProjectWorldCanonicalLoader::Load(Path,Refused,Validation));
		TestEqual(*FString::Printf(TEXT("Owned refusal reason: %s"),*Name),Validation.ErrorCode,Code);
		FProjectWorldCanonicalLoader::ComputeFileSha256(Path,AfterHash);
		TestEqual(TEXT("Read-only native refusal preserves the candidate receipt."),AfterHash,BeforeHash);
	}
	return !HasAnyErrors();
}
#endif
