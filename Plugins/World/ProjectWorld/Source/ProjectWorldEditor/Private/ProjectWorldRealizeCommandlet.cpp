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

bool UProjectWorldRealizeCommandlet::IsSafeResultPath(const FString& ResultPath)
{
	const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
	return FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(ResultPath), EvidenceRoot);
}

int32 UProjectWorldRealizeCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Parameters;
	ParseCommandLine(*Params, Tokens, Switches, Parameters);

	const FString* CompileResult = Parameters.Find(TEXT("CompileResult"));
	const FString* PresentationProfile = Parameters.Find(TEXT("PresentationProfile"));
	const FString* RuntimeProfile = Parameters.Find(TEXT("RuntimeProfile"));
	const FString* AuthoredOverlayProfile = Parameters.Find(TEXT("AuthoredOverlayProfile"));
	const FString* RealizationProfile = Parameters.Find(TEXT("RealizationProfile"));
	const FString* LayerDirtyInput = Parameters.Find(TEXT("LayerDirtyInput"));
	const FString* ResultPath = Parameters.Find(TEXT("Result"));
	const FString* MapPath = Parameters.Find(TEXT("Map"));
	const FString* ModeValue = Parameters.Find(TEXT("Mode"));
	const FString Mode = ModeValue == nullptr ? TEXT("validate") : ModeValue->ToLower();
	EProjectWorldRealizationMode ParsedMode;
	if (Mode == TEXT("validate"))
	{
		ParsedMode = EProjectWorldRealizationMode::Validate;
	}
	else if (Mode == TEXT("apply"))
	{
		ParsedMode = EProjectWorldRealizationMode::Apply;
	}
	else if (Mode == TEXT("delete"))
	{
		ParsedMode = EProjectWorldRealizationMode::Delete;
	}
	else
	{
		UE_LOG(LogProjectWorldRealization, Error, TEXT("[ProjectWorldRealizeCommandlet::Main] Invalid mode - %s"), *Mode);
		return 2;
	}
	if (CompileResult == nullptr || ResultPath == nullptr || MapPath == nullptr ||
		(ParsedMode != EProjectWorldRealizationMode::Delete &&
			(PresentationProfile == nullptr || AuthoredOverlayProfile == nullptr)))
	{
		UE_LOG(
			LogProjectWorldRealization,
			Error,
			TEXT("[ProjectWorldRealizeCommandlet::Main] Usage - require -CompileResult=<path> -Result=<path> -Map=/<world-data-plugin>/Generated/... [-PresentationProfile=<path> -AuthoredOverlayProfile=<path> for validate/apply] [-RuntimeProfile=<path>] [-RealizationProfile=<path> -LayerDirtyInput=<path> -FirstLayerApply] [-Mode=validate|apply|delete]."));
		return 2;
	}

	FProjectWorldRealizationRequest Request;
	Request.CompileResultPath = FPaths::ConvertRelativePathToFull(*CompileResult);
	Request.PresentationProfilePath = PresentationProfile == nullptr
		? FString()
		: FPaths::ConvertRelativePathToFull(*PresentationProfile);
	Request.RuntimeProfilePath = RuntimeProfile == nullptr
		? FString()
		: FPaths::ConvertRelativePathToFull(*RuntimeProfile);
	Request.AuthoredOverlayProfilePath = AuthoredOverlayProfile == nullptr
		? FString()
		: FPaths::ConvertRelativePathToFull(*AuthoredOverlayProfile);
	Request.RealizationProfilePath = RealizationProfile == nullptr
		? FString()
		: FPaths::ConvertRelativePathToFull(*RealizationProfile);
	Request.LayerDirtyInputPath = LayerDirtyInput == nullptr
		? FString()
		: FPaths::ConvertRelativePathToFull(*LayerDirtyInput);
	Request.ResultPath = FPaths::ConvertRelativePathToFull(*ResultPath);
	Request.MapPackagePath = *MapPath;
	Request.Mode = ParsedMode;

	Request.bRequireLandscapeCompatible = Switches.ContainsByPredicate([](const FString& Switch)
	{
		return Switch.Equals(TEXT("RequireLandscape"), ESearchCase::IgnoreCase);
	});
	Request.bFirstLayerApply = Switches.ContainsByPredicate([](const FString& Switch)
	{
		return Switch.Equals(TEXT("FirstLayerApply"), ESearchCase::IgnoreCase);
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
	if (!IsSafeResultPath(Request.ResultPath))
	{
		const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/WorldRealization")));
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
		TEXT("[ProjectWorldRealizeCommandlet::Main] Start - mode=%s input=%s presentation=%s runtime=%s authored=%s realization=%s map=%s"),
		*Mode,
		*Request.CompileResultPath,
		*Request.PresentationProfilePath,
		*Request.RuntimeProfilePath,
		*Request.AuthoredOverlayProfilePath,
		*Request.RealizationProfilePath,
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
