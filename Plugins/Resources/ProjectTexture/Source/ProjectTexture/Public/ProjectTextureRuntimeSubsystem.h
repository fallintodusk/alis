// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/EngineSubsystem.h"
#include "ProjectTextureRuntimeSubsystem.generated.h"

class FProjectTextureRuntimeCache;
class UProjectTextureOutputSlot;
class UTextureRenderTarget2D;
class UWorld;

struct PROJECTTEXTURE_API FProjectTextureRuntimeCacheDeleter
{
	void operator()(FProjectTextureRuntimeCache* Cache) const;
};

USTRUCT(BlueprintType)
struct PROJECTTEXTURE_API FProjectTextureRuntimeTelemetry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int32 Requests = 0;

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int32 CacheHits = 0;

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int32 Generations = 0;

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int32 Failures = 0;

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int64 ResidentBytes = 0;

	UPROPERTY(VisibleAnywhere, Category = "Project Texture")
	int64 PeakResidentBytes = 0;
};

UCLASS()
class PROJECTTEXTURE_API UProjectTextureRuntimeSubsystem final : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual ~UProjectTextureRuntimeSubsystem() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool RequestOutput(
		UObject* WorldContextObject,
		UProjectTextureOutputSlot* OutputSlot,
		FString& OutError);

	bool IsOutputReady(const UProjectTextureOutputSlot* OutputSlot) const;
	const FProjectTextureRuntimeTelemetry& GetTelemetry() const { return Telemetry; }

	static void NotifyOutputSlotLoaded(UProjectTextureOutputSlot* OutputSlot);

private:
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void QueueOrGenerate(UProjectTextureOutputSlot* OutputSlot);
	UWorld* FindGenerationWorld() const;
	void ReleaseTransientTargets();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTextureOutputSlot>> PendingOutputs;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTextureRenderTarget2D>> TransientTargets;

	TSet<FString> ReadyOutputs;
	TMap<FString, int32> NodeGenerationCounts;
	TUniquePtr<FProjectTextureRuntimeCache, FProjectTextureRuntimeCacheDeleter> Cache;
	FProjectTextureRuntimeTelemetry Telemetry;
	FDelegateHandle WorldInitializedHandle;
	FDelegateHandle WorldCleanupHandle;
};
