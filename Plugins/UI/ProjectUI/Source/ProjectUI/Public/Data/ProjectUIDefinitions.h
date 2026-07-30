// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/UILayerTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ProjectUIDefinitions.generated.h"

/**
 * When a UI widget should be first created.
 */
UENUM(BlueprintType)
enum class EProjectUILoadPolicy : uint8
{
	Preload,
	OnDemand
};

/**
 * How long a UI widget instance should live after creation.
 */
UENUM(BlueprintType)
enum class EProjectUISpawnPolicy : uint8
{
	Persistent,
	Transient
};

/**
 * Scope of a UI widget instance.
 */
UENUM(BlueprintType)
enum class EProjectUIScope : uint8
{
	PerPlayer,
	Global
};

/**
 * Slot sizing policy for HUD slot composition.
 */
UENUM(BlueprintType)
enum class EProjectUISizePolicy : uint8
{
	Fill,
	Desired,
	AutoSize
};

/**
 * ViewModel creation policy for a widget definition.
 */
UENUM(BlueprintType)
enum class EProjectUIViewModelCreationPolicy : uint8
{
	CreateInstance,
	Global,
	PropertyPath
};

/**
 * UI definition loaded from ui_definitions.json.
 * This is data-only and interpreted by ProjectUI.
 */
USTRUCT(BlueprintType)
struct PROJECTUI_API FProjectUIDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	FGameplayTag LayerTag;

	UPROPERTY()
	FGameplayTag SlotTag;

	UPROPERTY()
	FSoftClassPath WidgetClassPath;

	UPROPERTY()
	FSoftClassPath ViewModelClassPath;

	UPROPERTY()
	FString LayoutJson;

	UPROPERTY()
	FString LayoutJsonPath;

	UPROPERTY()
	EProjectUILoadPolicy LoadPolicy = EProjectUILoadPolicy::OnDemand;

	UPROPERTY()
	EProjectUISpawnPolicy SpawnPolicy = EProjectUISpawnPolicy::Persistent;

	UPROPERTY()
	EProjectUIScope Scope = EProjectUIScope::PerPlayer;

	UPROPERTY()
	EProjectUISizePolicy SizePolicy = EProjectUISizePolicy::Fill;

	UPROPERTY()
	EProjectWidgetInputMode RequestedInput = EProjectWidgetInputMode::Default;

	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	EProjectUIViewModelCreationPolicy ViewModelCreationPolicy = EProjectUIViewModelCreationPolicy::CreateInstance;

	UPROPERTY()
	FString ViewModelPropertyPath;

	// Data-driven show/hide: LayerHost watches this bool VIEWMODEL_PROPERTY
	// on the Global ViewModel via UE reflection (FBoolProperty).
	// Requires vm_creation=Global. Eliminates per-feature controller boilerplate.
	// Example: "auto_visibility": "bIsActive" in ui_definitions.json
	UPROPERTY()
	FName AutoVisibilityProperty;

	UPROPERTY()
	FString SourcePluginName;

	UPROPERTY()
	FString SourceFilePath;
};
