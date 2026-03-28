// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "ProjectRadialProgress.generated.h"

class SProjectRadialProgress;

/**
 * Lightweight circular progress indicator for JSON-driven HUD/widgets.
 *
 * Renders a background ring plus a progress arc using Slate paint.
 * Intended as a reusable ProjectUI primitive, not a HUD-specific widget.
 */
UCLASS()
class PROJECTUI_API UProjectRadialProgress : public UWidget
{
	GENERATED_BODY()

public:
	UProjectRadialProgress(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetPercent(float InPercent);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetThickness(float InThickness);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetFillColor(FLinearColor InFillColor);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetTrackColor(FLinearColor InTrackColor);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetStartAngleDegrees(float InStartAngleDegrees);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetClockwise(bool bInClockwise);

	UFUNCTION(BlueprintCallable, Category = "Project UI|Radial Progress")
	void SetShowTrack(bool bInShowTrack);

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	float GetPercent() const { return Percent; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	float GetThickness() const { return Thickness; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	FLinearColor GetFillColor() const { return FillColor; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	FLinearColor GetTrackColor() const { return TrackColor; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	float GetStartAngleDegrees() const { return StartAngleDegrees; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	bool GetClockwise() const { return bClockwise; }

	UFUNCTION(BlueprintPure, Category = "Project UI|Radial Progress")
	bool GetShowTrack() const { return bShowTrack; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void ApplyToSlate();

	UPROPERTY(EditAnywhere, Category = "Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Percent = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Appearance", meta = (ClampMin = "1.0"))
	float Thickness = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor FillColor = FLinearColor(0.2f, 0.6f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor TrackColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);

	UPROPERTY(EditAnywhere, Category = "Appearance")
	float StartAngleDegrees = -90.0f;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	bool bClockwise = true;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	bool bShowTrack = true;

	TSharedPtr<SProjectRadialProgress> MyRadialProgress;
};
