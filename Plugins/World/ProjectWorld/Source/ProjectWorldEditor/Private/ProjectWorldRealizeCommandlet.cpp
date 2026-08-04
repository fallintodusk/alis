// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldRealizeCommandlet.h"

#include "ProjectWorldRealizationService.h"

#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectWorldRealization, Log, All);

UProjectWorldRealizeCommandlet::UProjectWorldRealizeCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UProjectWorldRealizeCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);

	const FString* CompileResult = Parameters.Find(TEXT("CompileResult"));
	const FString* ResultPath = Parameters.Find(TEXT("Result"));
	const FString* MapPath = Parameters.Find(TEXT("Map"));
	const FString* ModeValue = Parameters.Find(TEXT("Mode"));
	if (CompileResult == nullptr || ResultPath == nullptr)
	{
		UE_LOG(
			LogProjectWorldRealization,
			Error,
			TEXT("[ProjectWorldRealizeCommandlet::Main] Usage - require -CompileResult=<path> -Result=<path> [-Mode=validate|apply|delete] [-Map=/ProjectWorld/Generated/...]."));
		return 2;
	}

	FProjectWorldRealizationRequest Request;
	Request.CompileResultPath = FPaths::ConvertRelativePathToFull(*CompileResult);
	Request.ResultPath = FPaths::ConvertRelativePathToFull(*ResultPath);
	Request.MapPackagePath = MapPath == nullptr
		? TEXT("/ProjectWorld/Generated/P0/L_ProjectWorldSynthetic")
		: *MapPath;
	const FString Mode = ModeValue == nullptr ? TEXT("validate") : ModeValue->ToLower();
	if (Mode == TEXT("validate"))
	{
		Request.Mode = EProjectWorldRealizationMode::Validate;
	}
	else if (Mode == TEXT("apply"))
	{
		Request.Mode = EProjectWorldRealizationMode::Apply;
	}
	else if (Mode == TEXT("delete"))
	{
		Request.Mode = EProjectWorldRealizationMode::Delete;
	}
	else
	{
		UE_LOG(LogProjectWorldRealization, Error, TEXT("[ProjectWorldRealizeCommandlet::Main] Invalid mode - %s"), *Mode);
		return 2;
	}

	Request.bRequireLandscapeCompatible = Switches.ContainsByPredicate([](const FString& Switch)
	{
		return Switch.Equals(TEXT("RequireLandscape"), ESearchCase::IgnoreCase);
	});
	auto ParseFeatureLimit = [&Parameters](const TCHAR* Name, int32& OutValue)
	{
		const FString* Value = Parameters.Find(Name);
		return Value == nullptr || (LexTryParseString(OutValue, **Value) && OutValue >= 0);
	};
	if (!ParseFeatureLimit(TEXT("MaxRoads"), Request.MaxRoadFeatures) ||
		!ParseFeatureLimit(TEXT("MaxBuildings"), Request.MaxBuildingFeatures))
	{
		UE_LOG(LogProjectWorldRealization, Error, TEXT("[ProjectWorldRealizeCommandlet::Main] Feature limits must be non-negative integers."));
		return 2;
	}
	const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
	if (!FPaths::IsUnderDirectory(Request.ResultPath, EvidenceRoot))
	{
		UE_LOG(
			LogProjectWorldRealization,
			Error,
			TEXT("[ProjectWorldRealizeCommandlet::Main] Unsafe result path - evidence must stay under %s."),
			*EvidenceRoot);
		return 2;
	}

	UE_LOG(
		LogProjectWorldRealization,
		Display,
		TEXT("[ProjectWorldRealizeCommandlet::Main] Start - mode=%s input=%s map=%s"),
		*Mode,
		*Request.CompileResultPath,
		*Request.MapPackagePath);
	FProjectWorldRealizationResult Result;
	const int32 ExitCode = FProjectWorldRealizationService::Run(Request, Result);
	if (!FProjectWorldRealizationService::WriteResult(Request, Result))
	{
		UE_LOG(LogProjectWorldRealization, Error, TEXT("[ProjectWorldRealizeCommandlet::Main] Result write failed - %s"), *Request.ResultPath);
		return 7;
	}

	if (Result.Status == TEXT("accepted"))
	{
		UE_LOG(
			LogProjectWorldRealization,
			Display,
			TEXT("[ProjectWorldRealizeCommandlet::Main] Complete - status=%s actors=%d duration=%.3fs result=%s"),
			*Result.Status,
			Result.CreatedActorCount,
			Result.DurationSeconds,
			*Request.ResultPath);
	}
	else
	{
		UE_LOG(
			LogProjectWorldRealization,
			Error,
			TEXT("[ProjectWorldRealizeCommandlet::Main] Rejected - code=%s detail=%s result=%s"),
			*Result.ErrorCode,
			*Result.Detail,
			*Request.ResultPath);
	}
	return ExitCode;
}
