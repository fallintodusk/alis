// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldVerify.h"

#include "ProjectWorldVerifyInternal.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Utilities/ProjectSha256.h"

DEFINE_LOG_CATEGORY(LogProjectWorldVerify);

namespace ProjectWorldVerifyBaselineDetail
{
	using ProjectWorldVerifyJson::Quote;
	using ProjectWorldVerifyJson::ReadPositiveInteger;
	using ProjectWorldVerifyPaths::RepoRelative;
	using ProjectWorldVerifyPaths::ShortVerifyName;

	const TCHAR* RecordScript = TEXT("scripts/ue/world/record_verify_baseline.ps1");
	const TCHAR* SchemaFilename = TEXT("project_world_verify_baseline.schema.json");
	constexpr int32 MaximumReportedDifferences = 10;

	TArray<TPair<FString, FString>> SortedInputs(const TArray<TPair<FString, FString>>& Inputs)
	{
		TArray<TPair<FString, FString>> Sorted = Inputs;
		Sorted.Sort([](const TPair<FString, FString>& Left, const TPair<FString, FString>& Right)
		{
			return ProjectWorldVerifyJson::Less(Left.Key, Right.Key);
		});
		return Sorted;
	}

	/** The stored part of a baseline, read back for comparison. */
	struct FStoredBaseline
	{
		FString ProducerId;
		int32 OutputRevision = 0;
		int32 PipelineRevision = 0;
		int32 ComparisonContractVersion = 0;
		FString EngineIdentity;
		TArray<TPair<FString, FString>> FixtureInputs;
		TArray<FString> Embedded;
		FProjectWorldProjection Projection;
	};

	bool LoadStored(const FString& Path, FStoredBaseline& OutStored, FString& OutError)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Identity = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Embedded = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
		int32 SchemaVersion = 0;
		FString StoredSha;
		if (!FFileHelper::LoadFileToString(Text, *Path) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() ||
			!ReadPositiveInteger(Root, TEXT("schema_version"), SchemaVersion) || SchemaVersion != 1 ||
			!Root->TryGetObjectField(TEXT("identity"), Identity) || Identity == nullptr ||
			!(*Identity)->TryGetStringField(TEXT("producer_id"), OutStored.ProducerId) ||
			!ReadPositiveInteger(*Identity, TEXT("output_revision"), OutStored.OutputRevision) ||
			!ReadPositiveInteger(*Identity, TEXT("pipeline_revision"), OutStored.PipelineRevision) ||
			!ReadPositiveInteger(*Identity, TEXT("comparison_contract_version"), OutStored.ComparisonContractVersion) ||
			!(*Identity)->TryGetStringField(TEXT("engine_identity"), OutStored.EngineIdentity) ||
			!(*Identity)->TryGetArrayField(TEXT("fixture_inputs"), Inputs) || Inputs == nullptr ||
			!(*Identity)->TryGetArrayField(TEXT("embedded"), Embedded) || Embedded == nullptr ||
			!Root->TryGetStringField(TEXT("projection_sha256"), StoredSha) ||
			!Root->TryGetArrayField(TEXT("records"), Records) || Records == nullptr)
		{
			OutError = TEXT("baseline does not match schema version 1");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Inputs)
		{
			const TSharedPtr<FJsonObject> Input = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Name;
			FString Sha;
			if (!Input.IsValid() || !Input->TryGetStringField(TEXT("name"), Name) || !Input->TryGetStringField(TEXT("sha256"), Sha))
			{
				OutError = TEXT("baseline fixture input is malformed");
				return false;
			}
			OutStored.FixtureInputs.Emplace(Name, Sha);
		}
		for (const TSharedPtr<FJsonValue>& Value : *Embedded)
		{
			FString Entry;
			if (!Value.IsValid() || !Value->TryGetString(Entry))
			{
				OutError = TEXT("baseline embedded entry is malformed");
				return false;
			}
			OutStored.Embedded.Add(Entry);
		}
		for (const TSharedPtr<FJsonValue>& Value : *Records)
		{
			const TSharedPtr<FJsonObject> Record = Value.IsValid() ? Value->AsObject() : nullptr;
			const TSharedPtr<FJsonObject>* Fields = nullptr;
			FString Kind;
			FString Key;
			if (!Record.IsValid() || !Record->TryGetStringField(TEXT("kind"), Kind) ||
				!Record->TryGetStringField(TEXT("key"), Key) || !Record->TryGetObjectField(TEXT("fields"), Fields) || Fields == nullptr)
			{
				OutError = TEXT("baseline record is malformed");
				return false;
			}
			FProjectWorldProjectionRecord& Stored = OutStored.Projection.Add(Kind, Key);
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Fields)->Values)
			{
				FString FieldValue;
				if (!Field.Value.IsValid() || !Field.Value->TryGetString(FieldValue))
				{
					OutError = FString::Printf(TEXT("baseline field is not a string: %s:%s.%s"), *Kind, *Key, *Field.Key);
					return false;
				}
				Stored.Fields.Add(Field.Key, FieldValue);
			}
		}
		if (!OutStored.Projection.IsValid(OutError))
		{
			return false;
		}
		if (OutStored.Projection.Sha256() != StoredSha)
		{
			OutError = TEXT("records do not hash to projection_sha256; a baseline is only written by record mode");
			return false;
		}
		return true;
	}

	FString ValueOr(const TMap<FString, FString>& Map, const FString& Key, const TCHAR* Default)
	{
		const FString* Value = Map.Find(Key);
		return Value != nullptr ? *Value : FString(Default);
	}

	TArray<FString> DiffIdentity(const FStoredBaseline& Stored, const FProjectWorldVerifyIdentity& Current)
	{
		TArray<FString> Differences;
		auto Compare = [&Differences](const TCHAR* Name, const FString& Before, const FString& After)
		{
			if (Before != After)
			{
				Differences.Add(FString::Printf(TEXT("%s: baseline=%s current=%s"), Name, *Before, *After));
			}
		};
		Compare(TEXT("producer_id"), Stored.ProducerId, Current.ProducerId());
		Compare(TEXT("output_revision"), FString::FromInt(Stored.OutputRevision), FString::FromInt(Current.OutputRevision));
		Compare(TEXT("pipeline_revision"), FString::FromInt(Stored.PipelineRevision), FString::FromInt(Current.PipelineRevision));
		Compare(TEXT("comparison_contract_version"), FString::FromInt(Stored.ComparisonContractVersion),
			FString::FromInt(Current.ComparisonContractVersion));
		Compare(TEXT("engine_identity"), Stored.EngineIdentity, Current.EngineIdentity);
		TMap<FString, FString> Before;
		TMap<FString, FString> After;
		for (const TPair<FString, FString>& Input : Stored.FixtureInputs)
		{
			Before.Add(Input.Key, Input.Value);
		}
		for (const TPair<FString, FString>& Input : Current.FixtureInputs)
		{
			After.Add(Input.Key, Input.Value);
		}
		TSet<FString> Names;
		for (const TPair<FString, FString>& Input : Before) { Names.Add(Input.Key); }
		for (const TPair<FString, FString>& Input : After) { Names.Add(Input.Key); }
		TArray<FString> SortedNames = Names.Array();
		SortedNames.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		for (const FString& Name : SortedNames)
		{
			Compare(*(TEXT("fixture ") + Name), ValueOr(Before, Name, TEXT("absent")), ValueOr(After, Name, TEXT("absent")));
		}
		Compare(TEXT("embedded"), FString::Join(Stored.Embedded, TEXT(",")), FString::Join(Current.Embedded, TEXT(",")));
		return Differences;
	}

	// Long values are shown from just before their first difference, where the change is.
	FString Clip(const FString& Value, const FString& Other)
	{
		int32 First = 0;
		while (First < Value.Len() && First < Other.Len() && Value[First] == Other[First])
		{
			++First;
		}
		const int32 Start = FMath::Max(0, First - 40);
		const FString Excerpt = Value.Mid(Start, 120);
		return (Start > 0 ? TEXT("...") : TEXT("")) + Excerpt + (Start + Excerpt.Len() < Value.Len() ? TEXT("...") : TEXT(""));
	}

	TArray<FString> DiffRecords(const FProjectWorldProjection& Stored, const FProjectWorldProjection& Current)
	{
		TMap<FString, const FProjectWorldProjectionRecord*> Before;
		TMap<FString, const FProjectWorldProjectionRecord*> After;
		for (const FProjectWorldProjectionRecord* Record : Stored.Sorted())
		{
			Before.Add(Record->Kind + TEXT(":") + Record->Key, Record);
		}
		for (const FProjectWorldProjectionRecord* Record : Current.Sorted())
		{
			After.Add(Record->Kind + TEXT(":") + Record->Key, Record);
		}
		TSet<FString> Keys;
		for (const TPair<FString, const FProjectWorldProjectionRecord*>& Entry : Before) { Keys.Add(Entry.Key); }
		for (const TPair<FString, const FProjectWorldProjectionRecord*>& Entry : After) { Keys.Add(Entry.Key); }
		TArray<FString> SortedKeys = Keys.Array();
		SortedKeys.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
		TArray<FString> Differences;
		for (const FString& Key : SortedKeys)
		{
			const FProjectWorldProjectionRecord* const* Old = Before.Find(Key);
			const FProjectWorldProjectionRecord* const* New = After.Find(Key);
			if (Old == nullptr || New == nullptr)
			{
				Differences.Add(FString::Printf(TEXT("%s: record %s"), *Key, Old == nullptr ? TEXT("added") : TEXT("removed")));
				continue;
			}
			TSet<FString> Fields;
			for (const TPair<FString, FString>& Field : (*Old)->Fields) { Fields.Add(Field.Key); }
			for (const TPair<FString, FString>& Field : (*New)->Fields) { Fields.Add(Field.Key); }
			TArray<FString> SortedFields = Fields.Array();
			SortedFields.Sort([](const FString& Left, const FString& Right) { return ProjectWorldVerifyJson::Less(Left, Right); });
			for (const FString& Field : SortedFields)
			{
				const FString OldValue = ValueOr((*Old)->Fields, Field, TEXT("<absent>"));
				const FString NewValue = ValueOr((*New)->Fields, Field, TEXT("<absent>"));
				if (OldValue != NewValue)
				{
					Differences.Add(FString::Printf(TEXT("%s.%s: baseline=%s current=%s"),
						*Key, *Field, *Clip(OldValue, NewValue), *Clip(NewValue, OldValue)));
				}
			}
		}
		return Differences;
	}

	FString ListFirst(const TArray<FString>& Lines)
	{
		FString Result;
		for (int32 Index = 0; Index < Lines.Num() && Index < MaximumReportedDifferences; ++Index)
		{
			Result += TEXT("\n  ") + Lines[Index];
		}
		if (Lines.Num() > MaximumReportedDifferences)
		{
			Result += FString::Printf(TEXT("\n  ... %d more"), Lines.Num() - MaximumReportedDifferences);
		}
		return Result;
	}

	FString SchemaReference(const FString& BaselinePath)
	{
		const TSharedPtr<IPlugin> Owner = IPluginManager::Get().FindPlugin(TEXT("ProjectWorld"));
		FString Schema = Owner.IsValid()
			? FPaths::Combine(FPaths::ConvertRelativePathToFull(Owner->GetBaseDir()), TEXT("Data/Schemas"), SchemaFilename)
			: FString(SchemaFilename);
		FPaths::MakePathRelativeTo(Schema, *(FPaths::GetPath(BaselinePath) + TEXT("/")));
		return Schema.Replace(TEXT("\\"), TEXT("/"));
	}

	FString Document(const FProjectWorldVerifyIdentity& Identity, const FProjectWorldProjection& Projection, const FString& BaselinePath)
	{
		FString Text = TEXT("{\n");
		Text += TEXT("  \"$schema\": ") + Quote(SchemaReference(BaselinePath)) + TEXT(",\n");
		Text += TEXT("  \"schema_version\": 1,\n");
		Text += TEXT("  \"verify\": ") + Quote(Identity.Verify) + TEXT(",\n");
		Text += TEXT("  \"identity\": {\n");
		Text += TEXT("    \"producer_id\": ") + Quote(Identity.ProducerId()) + TEXT(",\n");
		Text += FString::Printf(TEXT("    \"output_revision\": %d,\n"), Identity.OutputRevision);
		Text += FString::Printf(TEXT("    \"pipeline_revision\": %d,\n"), Identity.PipelineRevision);
		Text += FString::Printf(TEXT("    \"comparison_contract_version\": %d,\n"), Identity.ComparisonContractVersion);
		Text += TEXT("    \"engine_identity\": ") + Quote(Identity.EngineIdentity) + TEXT(",\n");
		const TArray<TPair<FString, FString>> Inputs = SortedInputs(Identity.FixtureInputs);
		Text += Inputs.IsEmpty() ? TEXT("    \"fixture_inputs\": [],\n") : TEXT("    \"fixture_inputs\": [\n");
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			Text += TEXT("      {\"name\": ") + Quote(Inputs[Index].Key) + TEXT(", \"sha256\": ") + Quote(Inputs[Index].Value) +
				(Index + 1 < Inputs.Num() ? TEXT("},\n") : TEXT("}\n"));
		}
		Text += Inputs.IsEmpty() ? TEXT("") : TEXT("    ],\n");
		Text += Identity.Embedded.IsEmpty() ? TEXT("    \"embedded\": []\n") : TEXT("    \"embedded\": [\n");
		for (int32 Index = 0; Index < Identity.Embedded.Num(); ++Index)
		{
			Text += TEXT("      ") + Quote(Identity.Embedded[Index]) + (Index + 1 < Identity.Embedded.Num() ? TEXT(",\n") : TEXT("\n"));
		}
		Text += Identity.Embedded.IsEmpty() ? TEXT("") : TEXT("    ]\n");
		Text += TEXT("  },\n");
		Text += TEXT("  \"projection_sha256\": ") + Quote(Projection.Sha256()) + TEXT(",\n");
		const TArray<const FProjectWorldProjectionRecord*> Records = Projection.Sorted();
		Text += Records.IsEmpty() ? TEXT("  \"records\": []\n") : TEXT("  \"records\": [\n");
		for (int32 Index = 0; Index < Records.Num(); ++Index)
		{
			const FProjectWorldProjectionRecord& Record = *Records[Index];
			Text += TEXT("    {\n      \"kind\": ") + Quote(Record.Kind) + TEXT(",\n      \"key\": ") + Quote(Record.Key) + TEXT(",\n");
			const TArray<FString> Fields = ProjectWorldVerifyJson::SortedKeys(Record.Fields);
			Text += Fields.IsEmpty() ? TEXT("      \"fields\": {}\n") : TEXT("      \"fields\": {\n");
			for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
			{
				Text += TEXT("        ") + Quote(Fields[FieldIndex]) + TEXT(": ") + Quote(Record.Fields.FindChecked(Fields[FieldIndex])) +
					(FieldIndex + 1 < Fields.Num() ? TEXT(",\n") : TEXT("\n"));
			}
			Text += Fields.IsEmpty() ? TEXT("") : TEXT("      }\n");
			Text += Index + 1 < Records.Num() ? TEXT("    },\n") : TEXT("    }\n");
		}
		Text += Records.IsEmpty() ? TEXT("") : TEXT("  ]\n");
		Text += TEXT("}\n");
		return Text;
	}

	bool WriteAtomically(const FString& Path, const FString& Text, FString& OutError)
	{
		const FString Staging = Path + TEXT(".tmp");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!FFileHelper::SaveStringToFile(Text, *Staging, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
			!IFileManager::Get().Move(*Path, *Staging, true, true))
		{
			IFileManager::Get().Delete(*Staging, false, true, true);
			OutError = FString::Printf(TEXT("cannot write %s"), *RepoRelative(Path));
			return false;
		}
		return true;
	}

	FString ThreeCauses(const FProjectWorldVerifyIdentity& Identity)
	{
		const FString Short = ShortVerifyName(Identity.Verify);
		return FString::Printf(
			TEXT("%s: output differs from %s at the same verification identity.\n")
			TEXT("Cause is one of:\n")
			TEXT(" 1. fixture or comparison change: bump the fixture input or comparison_contract_version, then re-record;\n")
			TEXT(" 2. producer output change: bump output_revision in %s and re-record in the same diff;\n")
			TEXT(" 3. nondeterminism: two fresh runs differ (%s -Verify %s -Probe); fix the producer."),
			*Identity.Verify, *RepoRelative(Identity.BaselinePath), *Identity.DescriptorPath, RecordScript, *Short);
	}

	ProjectWorldVerify::FResult Pass(const FString& Message)
	{
		return {true, Message};
	}

	ProjectWorldVerify::FResult Fail(const FString& Message)
	{
		return {false, Message};
	}

	ProjectWorldVerify::FResult Record(
		const FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldProjection& Projection,
		const FString& Reason)
	{
		FString Error;
		if (!WriteAtomically(Identity.BaselinePath, Document(Identity, Projection, Identity.BaselinePath), Error))
		{
			return Fail(TEXT("record failed: ") + Error);
		}
		UE_LOG(LogProjectWorldVerify, Display,
			TEXT("[ProjectWorldVerify] Recorded baseline - verify=%s path=%s projection_sha256=%s records=%d reason=%s"),
			*Identity.Verify, *RepoRelative(Identity.BaselinePath), *Projection.Sha256(), Projection.Sorted().Num(), *Reason);
		return Pass(FString::Printf(TEXT("Recorded %s (%s)."), *RepoRelative(Identity.BaselinePath), *Reason));
	}
}

namespace ProjectWorldVerify
{
	EMode ModeFromCommandLine(FString& OutProbeDir)
	{
		OutProbeDir.Reset();
		if (FParse::Value(FCommandLine::Get(), TEXT("ProjectWorldVerifyProbe="), OutProbeDir) && !OutProbeDir.IsEmpty())
		{
			return EMode::Probe;
		}
		return FParse::Param(FCommandLine::Get(), TEXT("ProjectWorldVerifyRecord")) ? EMode::Record : EMode::Compare;
	}

	FResult CompareOrRecord(
		const FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldProjection& Projection,
		EMode Mode)
	{
		using namespace ProjectWorldVerifyBaselineDetail;
		const bool bRecord = Mode == EMode::Record;
		const FString Short = ShortVerifyName(Identity.Verify);
		UE_LOG(LogProjectWorldVerify, Display, TEXT("[ProjectWorldVerify] Mode - verify=%s mode=%s baseline=%s"),
			*Identity.Verify, bRecord ? TEXT("record") : TEXT("compare"), *RepoRelative(Identity.BaselinePath));
		FString Error;
		if (Mode == EMode::Probe)
		{
			return Fail(Identity.Verify + TEXT(": probe mode writes no baseline."));
		}
		if (Identity.BaselinePath.IsEmpty() || !Projection.IsValid(Error))
		{
			return Fail(Identity.Verify + TEXT(": cannot compare: ") + (Error.IsEmpty() ? FString(TEXT("no baseline path")) : Error));
		}
		if (Projection.IsEmpty())
		{
			return Fail(Identity.Verify + TEXT(": the projection observed no producer output."));
		}
		if (!IFileManager::Get().FileExists(*Identity.BaselinePath))
		{
			return bRecord
				? Record(Identity, Projection, TEXT("no baseline"))
				: Fail(FString::Printf(TEXT("%s: no baseline at %s; record it with %s -Verify %s in a reviewed diff."),
					*Identity.Verify, *RepoRelative(Identity.BaselinePath), RecordScript, *Short));
		}
		FStoredBaseline Stored;
		if (!LoadStored(Identity.BaselinePath, Stored, Error))
		{
			return Fail(FString::Printf(TEXT("%s: baseline %s is not usable: %s"),
				*Identity.Verify, *RepoRelative(Identity.BaselinePath), *Error));
		}
		const TArray<FString> IdentityDifferences = DiffIdentity(Stored, Identity);
		const TArray<FString> RecordDifferences = DiffRecords(Stored.Projection, Projection);
		if (!IdentityDifferences.IsEmpty())
		{
			if (bRecord)
			{
				UE_LOG(LogProjectWorldVerify, Display, TEXT("[ProjectWorldVerify] Identity changed - verify=%s%s\nprojection differences: %d%s"),
					*Identity.Verify, *ListFirst(IdentityDifferences), RecordDifferences.Num(), *ListFirst(RecordDifferences));
				return Record(Identity, Projection, TEXT("identity changed"));
			}
			return Fail(FString::Printf(
				TEXT("%s: verification identity differs from %s:%s\nRe-record with %s -Verify %s in a reviewed diff."),
				*Identity.Verify, *RepoRelative(Identity.BaselinePath), *ListFirst(IdentityDifferences), RecordScript, *Short));
		}
		if (RecordDifferences.IsEmpty())
		{
			return bRecord
				? Fail(FString::Printf(TEXT("%s: record refused: identity unchanged; nothing to record (%s untouched)."),
					*Identity.Verify, *RepoRelative(Identity.BaselinePath)))
				: Pass(FString::Printf(TEXT("%s matches %s (projection_sha256 %s)."),
					*Identity.Verify, *RepoRelative(Identity.BaselinePath), *Projection.Sha256()));
		}
		return Fail((bRecord ? TEXT("record refused (baseline untouched): ") : TEXT("")) + ThreeCauses(Identity) +
			FString::Printf(TEXT("\nDifferences (%d):"), RecordDifferences.Num()) + ListFirst(RecordDifferences));
	}

	FResult WriteProbe(
		const FProjectWorldVerifyIdentity& Identity,
		const FProjectWorldProjection& Projection,
		const TMap<FString, FString>& PackageDigests,
		const FString& ProbeDir)
	{
		using namespace ProjectWorldVerifyBaselineDetail;
		FString Error;
		if (!Projection.IsValid(Error) || Projection.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("%s: probe has no valid projection: %s"), *Identity.Verify, *Error));
		}
		const FString Stem = FPaths::Combine(FPaths::ConvertRelativePathToFull(ProbeDir),
			FString::Printf(TEXT("%s.%u"), *Identity.GeneratorId, FPlatformProcess::GetCurrentProcessId()));
		FString Packages = TEXT("{\n");
		const TArray<FString> Names = ProjectWorldVerifyJson::SortedKeys(PackageDigests);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			Packages += TEXT("  ") + Quote(Names[Index]) + TEXT(": ") + Quote(PackageDigests.FindChecked(Names[Index])) +
				(Index + 1 < Names.Num() ? TEXT(",\n") : TEXT("\n"));
		}
		Packages += TEXT("}\n");
		if (!WriteAtomically(Stem + TEXT(".projection.json"), Document(Identity, Projection, Stem + TEXT(".projection.json")), Error) ||
			!WriteAtomically(Stem + TEXT(".packages.json"), Packages, Error))
		{
			return Fail(TEXT("probe failed: ") + Error);
		}
		UE_LOG(LogProjectWorldVerify, Display,
			TEXT("[ProjectWorldVerify] Probe written - verify=%s projection=%s.projection.json projection_sha256=%s packages=%d"),
			*Identity.Verify, *Stem, *Projection.Sha256(), Names.Num());
		return Pass(FString::Printf(TEXT("Probe written: %s.projection.json"), *Stem));
	}
}
