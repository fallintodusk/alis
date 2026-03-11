#pragma once

#include "CoreMinimal.h"
#include "OrchestratorVersion.generated.h"

USTRUCT(BlueprintType)
struct ORCHESTRATORCORE_API FOrchestratorVersion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orchestrator")
    int32 Major = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orchestrator")
    int32 Minor = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orchestrator")
    int32 Patch = 0;

    FOrchestratorVersion() = default;
    FOrchestratorVersion(int32 InMajor, int32 InMinor, int32 InPatch)
        : Major(InMajor), Minor(InMinor), Patch(InPatch) {}

    bool operator>(const FOrchestratorVersion& Other) const
    {
        if (Major != Other.Major) return Major > Other.Major;
        if (Minor != Other.Minor) return Minor > Other.Minor;
        return Patch > Other.Patch;
    }
    bool operator<(const FOrchestratorVersion& Other) const
    {
        if (Major != Other.Major) return Major < Other.Major;
        if (Minor != Other.Minor) return Minor < Other.Minor;
        return Patch < Other.Patch;
    }
    bool operator==(const FOrchestratorVersion& Other) const
    {
        return Major == Other.Major && Minor == Other.Minor && Patch == Other.Patch;
    }
    bool operator>=(const FOrchestratorVersion& Other) const { return *this == Other || *this > Other; }
    bool operator<=(const FOrchestratorVersion& Other) const { return *this == Other || *this < Other; }

    bool IsValid() const { return (Major | Minor | Patch) != 0; }

    FString ToString() const
    {
        return FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Patch);
    }

    static FOrchestratorVersion FromString(const FString& Str)
    {
        TArray<FString> Parts;
        Str.ParseIntoArray(Parts, TEXT("."));
        FOrchestratorVersion Out;
        if (Parts.Num() > 0) Out.Major = FCString::Atoi(*Parts[0]);
        if (Parts.Num() > 1) Out.Minor = FCString::Atoi(*Parts[1]);
        if (Parts.Num() > 2) Out.Patch = FCString::Atoi(*Parts[2]);
        return Out;
    }
};

USTRUCT()
struct ORCHESTRATORCORE_API FOrchestratorMetadata
{
    GENERATED_BODY()

    FString PluginName;
    FString PluginURL;
    FString MinEngineVersion;
    FString Changelog;
    FOrchestratorVersion Version;
    TArray<FString> Dependencies;
};

USTRUCT()
struct ORCHESTRATORCORE_API FOrchestratorRegistryEntry
{
    GENERATED_BODY()

    FString PluginName;
    FOrchestratorVersion InstalledVersion;
    FOrchestratorVersion LatestVersion;
    int64 LastUpdateCheckTimestamp = 0;
    bool bUpdateAvailable = false;
};

