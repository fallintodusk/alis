// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ProjectEffectWidget.h"
#include "ProjectFireSparksEffect.generated.h"

/**
 * Individual spark particle data.
 */
USTRUCT()
struct FProjectSpark
{
	GENERATED_BODY()

	/** Position in normalized widget space (0-1) */
	FVector2D Position = FVector2D::ZeroVector;

	/** Velocity in normalized units per second */
	FVector2D Velocity = FVector2D::ZeroVector;

	/** Total lifetime in seconds */
	float Lifetime = 1.0f;

	/** Current age in seconds */
	float Age = 0.0f;

	/** Rotation in degrees */
	float Rotation = 0.0f;

	/** Rotation speed in degrees per second */
	float RotationSpeed = 0.0f;

	/** Size multiplier for this particle */
	float SizeMultiplier = 1.0f;
};

/**
 * Parameters specific to fire sparks effect.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectFireSparksParams : public FProjectEffectParams
{
	GENERATED_BODY()

	/** Maximum number of sparks alive at once */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks")
	int32 MaxSparks = 40;

	/** Sparks spawned per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks")
	float SpawnRate = 15.0f;

	/** Spawn area minimum (normalized 0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Spawn")
	FVector2D SpawnAreaMin = FVector2D(0.0f, 0.8f);

	/** Spawn area maximum (normalized 0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Spawn")
	FVector2D SpawnAreaMax = FVector2D(1.0f, 1.0f);

	/** Minimum spark lifetime in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Lifetime")
	float MinLifetime = 0.7f;

	/** Maximum spark lifetime in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Lifetime")
	float MaxLifetime = 1.3f;

	/** Minimum velocity (normalized units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Velocity")
	FVector2D MinVelocity = FVector2D(-0.1f, -0.7f);

	/** Maximum velocity (normalized units per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Velocity")
	FVector2D MaxVelocity = FVector2D(0.1f, -1.2f);

	/** Gravity applied to sparks (normalized units per second squared) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Velocity")
	FVector2D Gravity = FVector2D(0.0f, -0.1f);

	/** Spark pixel size (width, height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance")
	FVector2D PixelSize = FVector2D(2.0f, 6.0f);

	/** Base spark color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance")
	FLinearColor SparkColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);

	/** Color at end of life (fade to) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance")
	FLinearColor EndColor = FLinearColor(1.0f, 0.2f, 0.0f, 0.0f);

	/** Enable random rotation per spark */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance")
	bool bEnableRotation = true;

	/** Max rotation speed in degrees per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance", meta = (EditCondition = "bEnableRotation"))
	float MaxRotationSpeed = 180.0f;

	/** Enable size variation per spark */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance")
	bool bEnableSizeVariation = true;

	/** Size variation range (0.5 = 50% to 150% of base size) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sparks|Appearance", meta = (EditCondition = "bEnableSizeVariation", ClampMin = "0.0", ClampMax = "1.0"))
	float SizeVariation = 0.3f;
};

/**
 * Fire sparks procedural UI effect.
 *
 * Renders animated spark particles rising from a spawn area.
 * Fully CPU-driven, no project assets required.
 *
 * JSON Configuration Example:
 * {
 *   "type": "FireSparks",
 *   "name": "BackgroundSparks",
 *   "anchor": "Fill",
 *   "effect": {
 *     "maxSparks": 40,
 *     "spawnRate": 15,
 *     "sparkColor": {"r": 1.0, "g": 0.8, "b": 0.3, "a": 1.0}
 *   }
 * }
 */
UCLASS(Blueprintable, meta = (DisplayName = "Fire Sparks Effect"))
class PROJECTUI_API UProjectFireSparksEffect : public UProjectEffectWidget
{
	GENERATED_BODY()

public:
	UProjectFireSparksEffect(const FObjectInitializer& ObjectInitializer);

	// UProjectEffectWidget interface
	virtual FString GetEffectTypeName() const override { return TEXT("FireSparks"); }
	virtual void ResetEffect() override;
	virtual void ApplyParams(const FProjectEffectParams& InParams) override;

	// ==========================================================================
	// Fire Sparks Specific Configuration
	// ==========================================================================

	/** Apply fire sparks specific parameters */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects|Fire Sparks")
	void ApplySparksParams(const FProjectFireSparksParams& InParams);

	/** Get current sparks parameters */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects|Fire Sparks")
	const FProjectFireSparksParams& GetSparksParams() const { return Params; }

	/** Set spark color at runtime */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects|Fire Sparks")
	void SetSparkColor(const FLinearColor& InColor);

	/** Set spawn rate at runtime */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Effects|Fire Sparks")
	void SetSpawnRate(float InRate);

	/** Get current active spark count */
	UFUNCTION(BlueprintPure, Category = "Project UI|Effects|Fire Sparks")
	int32 GetActiveSparkCount() const { return Sparks.Num(); }

protected:
	// UProjectEffectWidget interface
	virtual void UpdateEffect(float DeltaTime, const FGeometry& AllottedGeometry) override;
	virtual void RenderEffect(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, float InOpacity) const override;
	virtual void OnEffectEnabled() override;
	virtual void OnEffectDisabled() override;

	/** Current effect parameters */
	UPROPERTY(EditAnywhere, Category = "Effect Configuration")
	FProjectFireSparksParams Params;

private:
	/** Active spark particles */
	mutable TArray<FProjectSpark> Sparks;

	/** Time accumulator for spawn timing */
	float SpawnAccumulator = 0.0f;

	/** Spawn new sparks based on spawn rate */
	void SpawnSparks(float DeltaTime);

	/** Update existing spark positions and cull dead ones */
	void UpdateSparks(float DeltaTime);

	/** Create a new spark with randomized parameters */
	FProjectSpark CreateSpark() const;

	/** Calculate spark color based on age/lifetime */
	FLinearColor GetSparkColor(const FProjectSpark& Spark, float GlobalOpacity) const;

	/** Calculate spark size based on age/lifetime */
	FVector2D GetSparkSize(const FProjectSpark& Spark) const;
};
