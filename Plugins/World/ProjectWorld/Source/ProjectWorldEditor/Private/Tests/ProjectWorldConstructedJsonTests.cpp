// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldConstructedJsonFixture.h"
#include "ProjectWorldVerify.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldCanonical
{
	bool ParseFeatures(const TSharedPtr<FJsonObject>& Object, const FString& Grid, const FString& Cell,
		TMap<FString, FProjectWorldCanonicalFeature>& Features, TArray<FString>& Ids,
		FProjectWorldCanonicalValidation& Validation);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedJsonTest,
	"Project.World.Realization.Constructed.CanonicalJsonAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedJsonTest::RunTest(const FString&)
{
	auto Parse = [](const FString& Text, FProjectWorldCanonicalFeature& Feature, FString& Error)
	{
		TSharedPtr<FJsonObject> Document;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Document)) return false;
		TMap<FString,FProjectWorldCanonicalFeature> Features; TArray<FString> Ids;
		FProjectWorldCanonicalValidation Validation;
		const bool Accepted=ProjectWorldCanonical::ParseFeatures(Document,Document->GetStringField(TEXT("grid_id")),
			Document->GetStringField(TEXT("cell_id")),Features,Ids,Validation);
		Error=Validation.ErrorCode;
		if (Accepted && Ids.Num()==1) Feature=Features.FindChecked(Ids[0]);
		return Accepted;
	};
	const FString Input=ProjectWorldConstructedJsonFixture::Document();
	FProjectWorldCanonicalFeature Feature; FString Error;
	if (!TestTrue(TEXT("The complete source-bound P25 owner JSON parses."),Parse(Input,Feature,Error))) return false;
	if (!TestTrue(TEXT("JSON admission retains a typed structural field."),Feature.ConstructedSurface.IsSet())) return false;
	TestEqual(TEXT("All declared parent faces survive loading."),Feature.ConstructedSurface->Triangles.Num(),13);
	TestTrue(TEXT("A qualified supporting member need not own an extra face role."),
		Feature.ConstructedSurface->SourceProviderIds==TArray<FString>{TEXT("relation/20784500"),TEXT("way/1236753478")});
	FProjectWorldCanonicalBundle Bundle; Bundle.CanonicalCrs=TEXT("EPSG:32618"); Bundle.VerticalDatum=TEXT("EPSG:3855");
	TArray<FProjectWorldSurfaceTriangle> Faces; FString Identity;
	TestTrue(TEXT("The admitted field reaches the existing native reader."),
		ProjectWorldConstructedSurface::Read(Bundle,Feature,Faces,Identity,Error));
	const FString GeometryIdentity=Identity;
	FProjectWorldCanonicalFeature MetadataOnly;
	const FString MetadataInput=Input.Replace(TEXT("\"model_provenance\":{"),TEXT("\"model_provenance\":{\"note\":\"metadata only\","));
	TestTrue(TEXT("The locality control changes actual JSON provenance."),MetadataInput!=Input && Parse(MetadataInput,MetadataOnly,Error));
	TestTrue(TEXT("The parsed control changes admission metadata while retaining the model."),
		MetadataOnly.ConstructedSurface.IsSet() && MetadataOnly.ConstructedSurface->AdmissionHash!=Feature.ConstructedSurface->AdmissionHash &&
		MetadataOnly.ConstructedSurface->ModelHash==Feature.ConstructedSurface->ModelHash);
	TestTrue(TEXT("Metadata-only changes still expose the same validated geometric field."),
		ProjectWorldConstructedSurface::Read(Bundle,MetadataOnly,Faces,Identity,Error));
	TestEqual(TEXT("Admission metadata does not dirty referenced Road geometry."),Identity,GeometryIdentity);
	Bundle.Features.Add(Feature.FeatureId,Feature); FProjectWorldVerifyIdentity OriginalFixture,ChangedFixture;
	OriginalFixture.AddFixtureBundle(TEXT("metadata"),Bundle);
	Bundle.Features[Feature.FeatureId]=MetadataOnly; ChangedFixture.AddFixtureBundle(TEXT("metadata"),Bundle);
	TestTrue(TEXT("Admission metadata still invalidates verification evidence."),
		OriginalFixture.FixtureInputs[0].Value!=ChangedFixture.FixtureInputs[0].Value);
	for (const auto& Face : Faces) for (const auto& Point : Face.Points)
		TestEqual(TEXT("The explicit 3.55m hypothesis is consumed without a lift."),Point.Z,3.55);
	for (const TCHAR* Role : {TEXT("identified_assembly"),TEXT("inferred_apron"),TEXT("inferred_gate_apron")})
	{
		auto Candidate=Feature; Candidate.ConstructedSurface->Role=Role;
		TestTrue(TEXT("Explicit identified roles reach the existing native field reader."),
			ProjectWorldConstructedSurface::Read(Bundle,Candidate,Faces,Identity,Error));
		Candidate.ConstructedSurface->Basis=TEXT("observed");
		TestFalse(TEXT("Inferred structural roles cannot claim observed evidence."),
			ProjectWorldConstructedSurface::Read(Bundle,Candidate,Faces,Identity,Error));
	}
	const TArray<TPair<FString,FString>> Mutations={
		{TEXT("\"schema_version\":1,\"source_feature_id\""),TEXT("\"schema_version\":1.5,\"source_feature_id\"")},
		{TEXT("\"basis\":\"identified_model\""),TEXT("\"basis\":\"observed\"")},
		{TEXT("\"role\":\"fixed_pier\",\"triangles\":"),TEXT("\"role\":\"inferred_apron\",\"triangles\":")},
		{TEXT("\"schema_version\":1,\"source_feature_id\""),TEXT("\"schema_version\":\"1\",\"source_feature_id\"")},
		{TEXT(",3.55]"),TEXT(",\"3.55\"]")},
		{TEXT("\"role\":\"fixed_pier\""),TEXT("\"role\":\"Land\"")},
		{TEXT("\"source_feature_id\":\"alis:osm:relation:20784500\""),TEXT("\"source_feature_id\":\"unbound\"")},
		{TEXT("\"source_ids\":[\"relation/20784500\"]"),TEXT("\"source_ids\":[\"way/999\"]")},
		{TEXT("\"triangles\":[0,1,2,3,4,5,6,7,8,9,10,11,12]"),TEXT("\"triangles\":[0,0,2,3,4,5,6,7,8,9,10,11,12]")},
		{TEXT("\"vertical_datum\":\"EPSG:3855\""),TEXT("\"vertical_datum\":\"unknown\"")},
		{TEXT("\"model_provenance\":"),TEXT("\"unexpected\":true,\"model_provenance\":")}
	};
	for (const auto& Mutation : Mutations)
	{
		const FString Invalid=Input.Replace(*Mutation.Key,*Mutation.Value);
		if (!TestTrue(TEXT("Each malformed control changes the real fixture."),Invalid!=Input)) return false;
		FProjectWorldCanonicalFeature Rejected;
		TestFalse(*FString::Printf(TEXT("Malformed structural JSON refuses: %s"),*Mutation.Value),Parse(Invalid,Rejected,Error));
		TestFalse(TEXT("A refused record cannot expose a typed field."),Rejected.ConstructedSurface.IsSet());
	}
	TestTrue(TEXT("Ordinary feature attributes keep their existing path."),
		Parse(Input.Replace(TEXT("\"feature_class\":\"constructed_surface\""),TEXT("\"feature_class\":\"building\""))
			.Replace(TEXT("\"constructed_surface\":"),TEXT("\"ignored_test_attribute\":")),Feature,Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedImportIdentityTest,
	"Project.World.Realization.Constructed.CanonicalImportIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedImportIdentityTest::RunTest(const FString&)
{
	FProjectWorldCanonicalBundle Base;
	FProjectWorldCanonicalFeature Feature; Feature.FeatureId=TEXT("binding");
	Feature.ConstructedSurface.Emplace();
	auto& Surface=Feature.ConstructedSurface.GetValue();
	Surface.AdmissionHash=FString::ChrN(64,'a'); Surface.PolicyHash=FString::ChrN(64,'b');
	Surface.SourceProviderIds={TEXT("relation/20784500")};
	Surface.SourceSnapshots.Add({TEXT("refinement/inputs/source.json"),FString::ChrN(64,'c'),100,false});
	FProjectWorldCanonicalConstructedRegion Region; Region.Role=TEXT("fixed_pier");
	Region.SourceIds={TEXT("relation/20784500")}; Region.SourceTriangles={0}; Surface.FaceRegions.Add(Region);
	Surface.OperationalPolicy.Emplace(); auto& Policy=Surface.OperationalPolicy.GetValue();
	Policy.MaximumTriangles=4096; Policy.MaximumOrdinaryGradient=.02; Policy.MaximumGangwayGrade=.125;
	Policy.MaximumCrossSlope=.02; Policy.MaximumNumericErrorMeters=1e-8;
	Base.Features.Add(Feature.FeatureId,Feature);
	auto Identity = [](const FProjectWorldCanonicalBundle& Bundle)
	{
		FProjectWorldVerifyIdentity Result; Result.AddFixtureBundle(TEXT("binding"),Bundle);
		return Result.FixtureInputs[0].Value;
	};
	const FString Baseline=Identity(Base);
	for (int32 Case=0; Case<24; ++Case)
	{
		auto Changed=Base; auto& Value=Changed.Features.FindChecked(Feature.FeatureId).ConstructedSurface.GetValue();
		if (Case==0) Value.AdmissionHash=FString::ChrN(64,'d');
		if (Case==1) Value.PolicyHash=FString::ChrN(64,'d');
		if (Case==2) Value.SourceProviderIds.Add(TEXT("way/641349899"));
		if (Case==3) Value.SourceSnapshots[0].Path+=TEXT("changed");
		if (Case==4) Value.SourceSnapshots[0].Hash=FString::ChrN(64,'d');
		if (Case==5) ++Value.SourceSnapshots[0].ByteSize;
		if (Case==6) Value.SourceSnapshots[0].bNative=true;
		if (Case==7) Value.SourceSnapshots[0].ContextId=TEXT("changed_context");
		if (Case==8) Value.FaceRegions[0].Role=TEXT("gangway");
		if (Case==9) Value.FaceRegions[0].SourceIds[0]=TEXT("way/1");
		if (Case==10) Value.FaceRegions[0].SourceTriangles[0]=1;
		if (Case==11) Value.FaceRegions[0].AxisSourceId=TEXT("way/1");
		if (Case==12) Value.FaceRegions[0].AxisStart.X=1;
		if (Case==13) Value.FaceRegions[0].AxisStart.Y=1;
		if (Case==14) Value.FaceRegions[0].AxisEnd.X=1;
		if (Case==15) Value.FaceRegions[0].AxisEnd.Y=1;
		if (Case==16) --Value.OperationalPolicy->MaximumTriangles;
		if (Case==17) Value.OperationalPolicy->MaximumOrdinaryGradient=.01;
		if (Case==18) Value.OperationalPolicy->MaximumGangwayGrade=.1;
		if (Case==19) Value.OperationalPolicy->MaximumCrossSlope=.01;
		if (Case==20) Value.OperationalPolicy->MaximumNumericErrorMeters=1e-9;
		if (Case==21) Value.OperationalPolicy.Reset();
		if (Case==22) Value.FaceRegions.Reset();
		if (Case==23) Value.FaceRegions[0].SourceTriangles.Add(1);
		TestTrue(*FString::Printf(TEXT("Fixture identity consumes structural admission field %d."),Case),Identity(Changed)!=Baseline);
	}
	return !HasAnyErrors();
}
#endif
