#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class UWorldPartitionStreamingSourceComponent;

class FProjectWorldMeshTerrainRuntimeGate final
{
public:
	~FProjectWorldMeshTerrainRuntimeGate();
	void StartIfRequested();

private:
	enum class EProbeOutcome : uint8
	{
		NotRun,
		NoHit,
		UntaggedHit,
		TaggedTerrainHit
	};

	struct FProbeDiagnostic
	{
		FString ProofPoint = TEXT("none");
		EProbeOutcome Outcome = EProbeOutcome::NotRun;
		int32 LoadedTaggedSectionCount = 0;
		int32 LoadedCompiledSectionCount = 0;
		int32 StaticMeshComponentCount = 0;
		int32 RenderableStaticMeshComponentCount = 0;
		int32 NonMainPassStaticMeshComponentCount = 0;
		int32 NaniteStaticMeshComponentCount = 0;
		int32 SectionMaterialInstanceCount = 0;
		TArray<FString> BuildVariants;
		TArray<FString> MaterialChains;
		FString ActorClass;
		FString ActorPackage;
		FString Component;
		TArray<FName> ActorTags;
		double ImpactZ = 0.0;
		bool bHasImpact = false;
	};

	enum class EPhase : uint8
	{
		WaitingForWorld,
		WaitingAtCenter,
		WaitingAtEdge,
		WaitingAtCenterReturn,
		Finished
	};

	bool ParseConfig(FString& OutError);
	bool Tick(float DeltaSeconds);
	bool TryAcquireWorld();
	bool CreateStreamingSource(FString& OutError);
	void DisablePlayerStreamingSources() const;
	void InspectLoadedTerrainSectionsAround(const FVector& Point, FProbeDiagnostic& OutDiagnostic) const;
	EProbeOutcome ProbeTerrain(const FVector& Point, FString& OutMarker, FProbeDiagnostic& OutDiagnostic) const;
	bool HasTerrainMarker(const FString& Marker) const;
	void MoveSource(const FVector& Point, EPhase NewPhase);
	void FinishAccepted();
	void FinishRejected(const FString& Code, const FString& Message);
	void WriteResult(const FString& Status, const FString& Code, const FString& Message) const;
	static const TCHAR* ProbeOutcomeName(EProbeOutcome Outcome);
	static const TCHAR* PhaseName(EPhase Value);

	FTSTicker::FDelegateHandle TickHandle;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
	EPhase Phase = EPhase::WaitingForWorld;
	double StartedSeconds = 0.0;
	double PhaseStartedSeconds = 0.0;
	int32 PhaseFrames = 0;
	FString MapPackage;
	FString ResultPath;
	FString SourceIdentity;
	FString ProbeDerivationSha256;
	FVector Center = FVector::ZeroVector;
	FVector Edge = FVector::ZeroVector;
	float SourceRadiusCentimeters = 0.0f;
	FProbeDiagnostic LastProbe;
	FString CenterMarker;
	FString EdgeMarker;
	bool bStreamingSourceEnabled = false;
	bool bSourceStreamingCompleted = false;
	bool bCenterCollision = false;
	bool bEdgeCollision = false;
	bool bCenterUnloadedAtEdge = false;
	bool bCenterReloaded = false;
};
