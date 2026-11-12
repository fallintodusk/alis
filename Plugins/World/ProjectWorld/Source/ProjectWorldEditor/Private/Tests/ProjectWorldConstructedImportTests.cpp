// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#include "ProjectWorldCanonicalBundle.h"
#include "ProjectWorldConstructedSurface.h"
#include "ProjectWorldVerify.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProjectWorldConstructedImportTests
{
	using FObject=TSharedPtr<FJsonObject>;
	bool Read(const FString& Path,FObject& Out)
	{
		FString Text;
		return FFileHelper::LoadFileToString(Text,*Path) &&
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out) && Out.IsValid();
	}
	bool Write(const FString& Path,const FObject& Object)
	{
		FString Text;
		return FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)) &&
			FFileHelper::SaveStringToFile(Text,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	bool Reseal(const FString& Root,const FString& Receipt,const FObject& Result)
	{
		FObject Coverage; const FString CoveragePath=FPaths::Combine(Root,TEXT("canonical/coverage.json"));
		if (!Read(CoveragePath,Coverage)) return false;
		for (const auto& Value : Coverage->GetArrayField(TEXT("cells")))
		{
			const auto CellEntry=Value->AsObject();
			const FString ManifestPath=FPaths::Combine(Root,CellEntry->GetStringField(TEXT("path"))); FObject Manifest;
			if (!Read(ManifestPath,Manifest)) return false;
			const auto Feature=Manifest->GetObjectField(TEXT("feature_artifact"));
			const FString FeaturePath=FPaths::Combine(Root,Feature->GetStringField(TEXT("path"))); FString Hash;
			if (!FProjectWorldCanonicalLoader::ComputeFileSha256(FeaturePath,Hash)) return false;
			Feature->SetStringField(TEXT("content_hash"),Hash);
			Feature->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*FeaturePath));
			for (const auto& Descriptor : Coverage->GetArrayField(TEXT("artifact_descriptors")))
				if (Descriptor->AsObject()->GetStringField(TEXT("artifact_id"))==Feature->GetStringField(TEXT("artifact_id")))
				{
					Descriptor->AsObject()->SetStringField(TEXT("content_hash"),Hash);
					Descriptor->AsObject()->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*FeaturePath));
				}
			if (!Write(ManifestPath,Manifest) || !FProjectWorldCanonicalLoader::ComputeFileSha256(ManifestPath,Hash)) return false;
			CellEntry->SetStringField(TEXT("sha256"),Hash);
			CellEntry->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*ManifestPath));
		}
		if (!Write(CoveragePath,Coverage)) return false;
		for (const auto& Value : Result->GetArrayField(TEXT("outputs")))
		{
			const auto Entry=Value->AsObject(); FString Full,Hash;
			if (!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root,Entry->GetStringField(TEXT("path")),Full) ||
				!FProjectWorldCanonicalLoader::ComputeFileSha256(Full,Hash)) return false;
			Entry->SetStringField(TEXT("sha256"),Hash);
			Entry->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*Full));
		}
		return Write(Receipt,Result);
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectWorldConstructedImportTest,
	"Project.World.Realization.Constructed.CanonicalImportBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectWorldConstructedImportTest::RunTest(const FString&)
{
	using namespace ProjectWorldConstructedImportTests;
	FString Input,Expected,Actual;
	if (!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImport="),Input) ||
		!FParse::Value(FCommandLine::Get(),TEXT("ProjectWorldConstructedImportHash="),Expected) ||
		!FProjectWorldCanonicalLoader::ComputeFileSha256(Input,Actual) || Actual!=Expected)
	{ AddError(TEXT("Supply the freshly generated canonical import fixture and its exact receipt hash.")); return false; }
	FProjectWorldCanonicalBundle Bundle; FProjectWorldCanonicalValidation Validation;
	if (!TestTrue(TEXT("The fresh Python-published child reaches the complete native loader."),
		FProjectWorldCanonicalLoader::Load(Input,Bundle,Validation)))
	{ AddError(Validation.ErrorCode+TEXT(": ")+Validation.Message); return false; }
	TestEqual(TEXT("Both captured membership cells survive native admission."),Bundle.Cells.Num(),2);
	TestEqual(TEXT("The parent has exactly one native owner."),Bundle.Features.Num(),1);
	const auto* Parent=Bundle.Features.Find(TEXT("alis:osm:relation:20784500"));
	if (!TestTrue(TEXT("The P25 field and its source snapshots are admitted."),Parent && Parent->ConstructedSurface.IsSet())) return false;
	TestEqual(TEXT("No parent faces disappear across the Python/JSON/native boundary."),Parent->ConstructedSurface->Triangles.Num(),13);
	TestTrue(TEXT("The verified retained policy is projected for physical consumption."),Parent->ConstructedSurface->OperationalPolicy.IsSet());
	if (Parent->ConstructedSurface->OperationalPolicy.IsSet())
	{
		const auto& Policy=Parent->ConstructedSurface->OperationalPolicy.GetValue();
		TestEqual(TEXT("The actual source policy capacity survives loading."),Policy.MaximumTriangles,4096);
		TestEqual(TEXT("The actual ordinary norm bound survives loading."),Policy.MaximumOrdinaryGradient,.02);
		TestEqual(TEXT("The actual gangway grade bound survives loading."),Policy.MaximumGangwayGrade,.125);
		TestEqual(TEXT("The actual crossfall bound survives loading."),Policy.MaximumCrossSlope,.02);
		TestEqual(TEXT("The actual PWA numeric bound survives loading."),Policy.MaximumNumericErrorMeters,1e-8);
	}
	TestEqual(TEXT("The original face region is retained rather than inferred again."),Parent->ConstructedSurface->FaceRegions.Num(),1);
	if (!Parent->ConstructedSurface->FaceRegions.IsEmpty())
	{
		const auto& Region=Parent->ConstructedSurface->FaceRegions[0];
		TestEqual(TEXT("The complete actual source face census is available to the physical owner."),Region.SourceTriangles.Num(),13);
		TestEqual(TEXT("The actual source face role survives loading."),Region.Role,FString(TEXT("fixed_pier")));
	}
	TestEqual(TEXT("Normalized and native source snapshots remain separately bound."),Parent->ConstructedSurface->SourceSnapshots.Num(),2);
	TestTrue(TEXT("Admission retains original provider identity."),
		Parent->ConstructedSurface->SourceProviderIds.Contains(TEXT("relation/20784500")));
	FProjectWorldVerifyIdentity Before; Before.AddFixtureBundle(TEXT("import"),Bundle);
	const FString SourceRoot=FPaths::GetPath(Input);
	const FString Scratch=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
		TEXT("tmp/world/canonical_compilation/native_import"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	FObject Original;
	if (!Read(Input,Original)) return false;
	const TArray<FString> Cases={TEXT("undeclared"),TEXT("missing"),TEXT("wrong_kind"),TEXT("changed_bytes"),
		TEXT("wrong_format"),TEXT("wrong_version"),TEXT("conflicting_role"),TEXT("compile_kind"),TEXT("wrong_frame"),
		TEXT("fractional_receipt_bytes"),TEXT("string_receipt_bytes"),TEXT("native_nan"),TEXT("native_overflow"),
		TEXT("wrong_binding_context"),TEXT("mismatched_binding_context"),TEXT("wrong_policy_hash"),TEXT("stale_feature_manifest")};
	for (const FString& Case : Cases)
	{
		const FString Root=FPaths::Combine(Scratch,Case);
		for (const auto& Value : Original->GetArrayField(TEXT("outputs")))
		{
			const FString Relative=Value->AsObject()->GetStringField(TEXT("path")); FString Source,Target;
			if (!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(SourceRoot,Relative,Source) ||
				!FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(Root,Relative,Target)) return false;
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
			if (IFileManager::Get().Copy(*Target,*Source)!=COPY_OK) return false;
		}
		FObject Result,Request;
		if (!Read(Input,Result) || !Read(FPaths::Combine(Root,TEXT("refinement/request.json")),Request)) return false;
		const auto Operation=Request->GetArrayField(TEXT("operations"))[0]->AsObject();
		const auto Inputs=Operation->GetObjectField(TEXT("inputs"));
		const auto Context=Inputs->GetArrayField(TEXT("source_contexts"))[0]->AsObject();
		const auto Native=Context->GetObjectField(TEXT("native"));
		const FString NativePath=Native->GetStringField(TEXT("path"));
		auto Outputs=Result->GetArrayField(TEXT("outputs"));
		FObject OtherNormalized;
		if (Case==TEXT("mismatched_binding_context"))
		{
			const auto Normalized=Context->GetObjectField(TEXT("normalized")); FObject Document; FString Hash;
			const FString Source=FPaths::Combine(Root,Normalized->GetStringField(TEXT("path")));
			const FString Staged=FPaths::Combine(Root,TEXT("context-control.json"));
			if (!Read(Source,Document)) return false;
			Document->SetStringField(TEXT("shard_id"),TEXT("source-association-control"));
			if (!Write(Staged,Document) || !FProjectWorldCanonicalLoader::ComputeFileSha256(Staged,Hash)) return false;
			const FString Relative=TEXT("refinement/inputs/")+Hash+TEXT(".json");
			if (IFileManager::Get().Copy(*FPaths::Combine(Root,Relative),*Staged)!=COPY_OK) return false;
			OtherNormalized=MakeShared<FJsonObject>(); OtherNormalized->SetStringField(TEXT("path"),Relative);
			OtherNormalized->SetStringField(TEXT("sha256"),Hash);
			OtherNormalized->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*Staged));
			auto Output=MakeShared<FJsonObject>(*OtherNormalized); Output->SetStringField(TEXT("kind"),TEXT("compiler_document"));
			Outputs.Add(MakeShared<FJsonValueObject>(Output));
			auto OtherContext=MakeShared<FJsonObject>(*Context); OtherContext->SetStringField(TEXT("id"),TEXT("other_context"));
			OtherContext->SetObjectField(TEXT("normalized"),OtherNormalized);
			auto Contexts=Inputs->GetArrayField(TEXT("source_contexts")); Contexts.Add(MakeShared<FJsonValueObject>(OtherContext));
			Inputs->SetArrayField(TEXT("source_contexts"),Contexts);
		}
		if (Case==TEXT("undeclared")) Context->SetField(TEXT("native"),MakeShared<FJsonValueNull>());
		if (Case==TEXT("missing")) Outputs.RemoveAll([&](const auto& V){return V->AsObject()->GetStringField(TEXT("path"))==NativePath;});
		if (Case==TEXT("wrong_kind")) for (const auto& V : Outputs) if (V->AsObject()->GetStringField(TEXT("path"))==NativePath)
			V->AsObject()->SetStringField(TEXT("kind"),TEXT("compiler_document"));
		if (Case==TEXT("changed_bytes")) Native->SetNumberField(TEXT("byte_size"),Native->GetNumberField(TEXT("byte_size"))+1);
		if (Case==TEXT("wrong_format")) Native->SetStringField(TEXT("format"),TEXT("sanitized_osm_full_response"));
		if (Case==TEXT("wrong_version")) Request->SetNumberField(TEXT("schema_version"),1.5);
		if (Case==TEXT("conflicting_role"))
		{
			auto Descriptor=MakeShared<FJsonObject>(*Native); Descriptor->RemoveField(TEXT("format"));
			Inputs->SetObjectField(TEXT("model"),Descriptor);
		}
		if (Case==TEXT("compile_kind"))
		{
			Result->SetStringField(TEXT("$schema"),TEXT("https://alis.world/schemas/world-compiler/compile-result-v1.json"));
			Result->SetStringField(TEXT("operation"),TEXT("compile"));
		}
		if (Case.StartsWith(TEXT("native_")))
		{
			FString Text,Hash;
			if (!FFileHelper::LoadFileToString(Text,*FPaths::Combine(Root,NativePath))) return false;
			int32 End;
			if (!Text.FindLastChar('}',End)) return false;
			Text.InsertAt(End,Case==TEXT("native_nan") ? TEXT(",\"probe\":NaN") : TEXT(",\"probe\":1e400"));
			const FString OriginalPath=FPaths::Combine(Root,NativePath);
			if (!FFileHelper::SaveStringToFile(Text,*OriginalPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
				!FProjectWorldCanonicalLoader::ComputeFileSha256(OriginalPath,Hash)) return false;
			const FString NewPath=TEXT("refinement/inputs/")+Hash+TEXT(".json");
			if (IFileManager::Get().Copy(*FPaths::Combine(Root,NewPath),*OriginalPath)!=COPY_OK) return false;
			Native->SetStringField(TEXT("path"),NewPath); Native->SetStringField(TEXT("sha256"),Hash);
			Native->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*OriginalPath));
			for (const auto& V : Outputs) if (V->AsObject()->GetStringField(TEXT("path"))==NativePath)
				V->AsObject()->SetStringField(TEXT("path"),NewPath);
		}
		if (Case.StartsWith(TEXT("native_")) || Case==TEXT("wrong_binding_context") ||
			Case==TEXT("mismatched_binding_context") || Case==TEXT("wrong_policy_hash"))
		{
			for (const auto& V : Outputs)
			{
				const FString Path=V->AsObject()->GetStringField(TEXT("path"));
				if (!Path.StartsWith(TEXT("canonical/features/"))) continue;
				FObject Document; const FString Full=FPaths::Combine(Root,Path);
				if (!Read(Full,Document)) return false;
				for (const auto& F : Document->GetArrayField(TEXT("features")))
				{
					const auto Field=F->AsObject()->GetObjectField(TEXT("attributes"))->GetObjectField(TEXT("constructed_surface"));
					if (Case==TEXT("wrong_policy_hash")) Field->SetStringField(TEXT("policy_sha256"),FString::ChrN(64,'e'));
					for (const auto& B : Field->GetArrayField(TEXT("source_bindings")))
					{
						if (Case==TEXT("wrong_binding_context")) B->AsObject()->SetStringField(TEXT("context_id"),TEXT("another_context"));
						if (Case==TEXT("mismatched_binding_context")) B->AsObject()->SetObjectField(TEXT("normalized_input"),OtherNormalized);
						if (Case.StartsWith(TEXT("native_")))
						{
							auto Descriptor=MakeShared<FJsonObject>(*Native); Descriptor->RemoveField(TEXT("format"));
							B->AsObject()->SetObjectField(TEXT("native_input"),Descriptor);
						}
					}
				}
				if (!Write(Full,Document)) return false;
			}
		}
		if (Case==TEXT("wrong_frame"))
		{
			FObject Coverage; const FString Path=FPaths::Combine(Root,TEXT("canonical/coverage.json"));
			if (!Read(Path,Coverage)) return false;
			Coverage->GetObjectField(TEXT("grid"))->SetStringField(TEXT("canonical_crs"),TEXT("EPSG:32639"));
			if (!Write(Path,Coverage)) return false;
		}
		Result->SetArrayField(TEXT("outputs"),Outputs);
		if (!Write(FPaths::Combine(Root,TEXT("refinement/request.json")),Request)) return false;
		const FString Receipt=FPaths::Combine(Root,TEXT("refinement_result.json"));
		if (!Reseal(Root,Receipt,Result)) return false;
		if (Case==TEXT("stale_feature_manifest"))
		{
			for (const auto& V : Outputs)
			{
				const auto Entry=V->AsObject(); const FString Relative=Entry->GetStringField(TEXT("path"));
				if (!Relative.StartsWith(TEXT("canonical/features/"))) continue;
				const FString Full=FPaths::Combine(Root,Relative); FObject Document; FString Hash;
				if (!Read(Full,Document)) return false;
				if (Document->GetArrayField(TEXT("features")).IsEmpty()) continue;
				Document->SetStringField(TEXT("fixture_marker"),TEXT("manifest-left-stale"));
				if (!Write(Full,Document) || !FProjectWorldCanonicalLoader::ComputeFileSha256(Full,Hash)) return false;
				Entry->SetStringField(TEXT("sha256"),Hash);
				Entry->SetNumberField(TEXT("byte_size"),IFileManager::Get().FileSize(*Full));
			}
			if (!Write(Receipt,Result)) return false;
		}
		if (Case==TEXT("fractional_receipt_bytes") || Case==TEXT("string_receipt_bytes"))
		{
			for (const auto& V : Outputs) if (V->AsObject()->GetStringField(TEXT("path"))==NativePath)
			{
				const double Bytes=V->AsObject()->GetNumberField(TEXT("byte_size"));
				if (Case==TEXT("fractional_receipt_bytes")) V->AsObject()->SetNumberField(TEXT("byte_size"),Bytes+.25);
				else V->AsObject()->SetStringField(TEXT("byte_size"),FString::Printf(TEXT("%.0f"),Bytes));
			}
			if (!Write(Receipt,Result)) return false;
		}
		TestFalse(*FString::Printf(TEXT("Receipt-consistent bad child refuses: %s"),*Case),
			FProjectWorldCanonicalLoader::Load(Receipt,Bundle,Validation));
		TestEqual(TEXT("Refusal reaches the intended boundary."),Validation.ErrorCode,
			(Case==TEXT("compile_kind") || Case==TEXT("fractional_receipt_bytes") || Case==TEXT("string_receipt_bytes")) ? FString(TEXT("output-descriptor")) :
			(Case==TEXT("wrong_binding_context") || Case==TEXT("mismatched_binding_context") || Case==TEXT("wrong_policy_hash")) ? FString(TEXT("constructed-surface-input")) :
			Case==TEXT("stale_feature_manifest") ? FString(TEXT("cell-artifact")) :
			Case==TEXT("wrong_frame") ? FString(TEXT("constructed-surface-field")) : FString(TEXT("refinement-retained-input")));
	}
	TestTrue(TEXT("All mutation controls preserve the original imported child receipt."),
		FProjectWorldCanonicalLoader::ComputeFileSha256(Input,Actual) && Actual==Expected);
	TestTrue(TEXT("Every original output still authenticates after all refusal controls."),
		FProjectWorldCanonicalLoader::Load(Input,Bundle,Validation));
	FProjectWorldVerifyIdentity After; After.AddFixtureBundle(TEXT("import"),Bundle);
	TestTrue(TEXT("Cold repeated native admission preserves the complete consumed fixture identity."),
		Before.FixtureInputs[0].Value==After.FixtureInputs[0].Value);
	return !HasAnyErrors();
}
#endif
