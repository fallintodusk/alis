#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "ProjectUIScreenLayoutData.generated.h"

class UUserWidget;

/**
 * Named slot description used by the screen layout asset.
 */
USTRUCT(BlueprintType)
struct FProjectUIScreenSlot
{
	GENERATED_BODY()

	/** Logical name consumed by screen controllers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName SlotName = NAME_None;

	/** Widget class to spawn in this slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (AllowedClasses = "/Script/UMG.UserWidget"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** Whether the widget should be visible when the layout initializes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	bool bVisibleOnInit = true;

	/** Optional z-order override for layered UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	int32 ZOrder = 0;
};

/**
 * Entry describing a screen pushed on the activatable stack when the layout becomes active.
 */
USTRUCT(BlueprintType)
struct FProjectUIScreenStackEntry
{
	GENERATED_BODY()

	/** Screen widget class to push. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (AllowedClasses = "/Script/UMG.UserWidget"))
	TSoftClassPtr<UUserWidget> ScreenClass;

	/** If true the screen is displayed immediately when the layout activates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	bool bPushOnInit = true;

	/** Optional transition preset to use when presenting this screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FPrimaryAssetId TransitionPreset;
};

/**
 * Declarative description of the menu or loading screen layout.
 * Screen controllers read this asset to determine which widgets to spawn and how to arrange them.
 */
UCLASS(BlueprintType)
class PROJECTUI_API UProjectUIScreenLayoutData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectUIScreenLayoutData();

	/** Root widget instantiated when the layout loads. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (AllowedClasses = "/Script/UMG.UserWidget"))
	TSoftClassPtr<UUserWidget> RootWidgetClass;

	/** Optional overlay widget that stays above the root (e.g. notifications). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (AllowedClasses = "/Script/UMG.UserWidget"))
	TSoftClassPtr<UUserWidget> OverlayWidgetClass;

	/** Named slots configured for this layout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FProjectUIScreenSlot> Slots;

	/** Screens pushed on initialization. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FProjectUIScreenStackEntry> InitialScreenStack;

	/** Input configuration identifier understood by CommonUI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName InputConfigId = NAME_None;

	/** Optional theme override applied while this layout is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (AllowedClasses = "/Script/ProjectUI.ProjectUIThemeData"))
	TSoftObjectPtr<class UProjectUIThemeData> ThemeOverride;

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
