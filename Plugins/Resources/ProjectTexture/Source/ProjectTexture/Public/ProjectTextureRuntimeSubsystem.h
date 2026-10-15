// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/EngineSubsystem.h"
#include "RHIResources.h"
#include "Containers/Ticker.h"
#include "ProjectTextureAsyncBundle.h"
#include "ProjectTextureRuntimeSubsystem.generated.h"

class FProjectTextureRuntimeCache;
class UProjectTextureOutputSlot;
class UProjectTextureRuntimeCatalog;
class UTextureRenderTarget2D;
class UWorld;
class UProjectTextureBundleProducer;
enum class EProjectTextureBundleRequest : uint8 { Pending, Ready, Rejected };

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

UCLASS(Config=Engine)
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
	int64 GetBudgetBytes() const;

	static void NotifyOutputSlotLoaded(UProjectTextureOutputSlot* OutputSlot);

	// Explicit opt-in only; automatic generation continues to use the material producer.
	EProjectTextureBundleRequest RequestBundle(UProjectTextureOutputSlot* Slot,
		UProjectTextureBundleProducer* Producer, bool bPin, FString& Error);
	bool ReleaseBundle(const FString& Identity, FString& Error);
	bool CancelBundle(const FString& Identity, FString& Error);
	int32 EvictReleasedBundles();
	bool IsBundleReady(const UProjectTextureOutputSlot* Slot) const;

private:
	// One engine-lifetime policy; catalogs bound their own operations, not this cache.
	UPROPERTY(Config)
	int64 GlobalBudgetBytes = 32 * 1024 * 1024;

	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues InitializationValues);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void QueueOrGenerate(UProjectTextureOutputSlot* OutputSlot);
	void GeneratePendingOutputs();
	UWorld* FindGenerationWorld() const;
	void ReleaseTransientTargets();
	bool TickBundles(float Delta);
	void ReleaseBundleResources(const FString& Identity);
	void DrainBundles();
	void ReconcileBundles();
	bool AreOutputsReady(const UProjectTextureRuntimeCatalog* Catalog,
		const TMap<FName, UTextureRenderTarget2D*>& Targets) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProjectTextureOutputSlot>> PendingOutputs;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTextureRenderTarget2D>> TransientTargets;

	struct FReadyOutput
	{
		TWeakObjectPtr<UTextureRenderTarget2D> Target;
		FTextureRHIRef Resource;
	};
	TMap<FString, FReadyOutput> ReadyOutputs;
	TMap<FString, int32> NodeGenerationCounts;
	TUniquePtr<FProjectTextureRuntimeCache, FProjectTextureRuntimeCacheDeleter> Cache;
	FProjectTextureRuntimeTelemetry Telemetry;
	FDelegateHandle WorldInitializedHandle;
	FDelegateHandle WorldCleanupHandle;
	UPROPERTY(Transient) TMap<FString, FProjectTextureAsyncBundle> Bundles;
	FTSTicker::FDelegateHandle BundleTicker;
};
