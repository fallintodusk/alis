// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "ProjectHotReloadManager.generated.h"

class UWidget;
class UProjectUIThemeData;

/**
 * Hot Reload Validation Result
 *
 * Contains validation results from JSON parsing and widget creation preview.
 */
USTRUCT(BlueprintType)
struct FProjectHotReloadValidationResult
{
	GENERATED_BODY()

	/** Whether validation succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	bool bSuccess = false;

	/** Array of error messages (if validation failed) */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	TArray<FString> ErrorMessages;

	/** Array of warning messages (validation succeeded but with warnings) */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	TArray<FString> WarningMessages;

	/** File path that was validated */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	FString FilePath;

	/** Timestamp of validation */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	FDateTime Timestamp;

	/** Number of widgets that would be created */
	UPROPERTY(BlueprintReadOnly, Category = "Hot Reload")
	int32 WidgetCount = 0;
};

/**
 * Hot Reload State Snapshot
 *
 * Stores widget state for rollback capability.
 */
USTRUCT()
struct FProjectHotReloadSnapshot
{
	GENERATED_BODY()

	/** JSON string of the last valid layout */
	FString LastValidJson;

	/** File path */
	FString FilePath;

	/** Timestamp of snapshot */
	FDateTime Timestamp;

	/** Whether this snapshot is valid */
	bool bIsValid = false;
};

/**
 * Hot Reload Manager Subsystem
 *
 * Coordinates hot reload operations with validation, preview, and rollback.
 * Provides error reporting and debug visualization for designers.
 *
 * Features:
 * - JSON validation before applying changes
 * - Preview mode (validate without committing)
 * - Rollback capability for failed reloads
 * - Error reporting visible to designers
 * - Debug overlay showing reload status
 *
 * Usage:
 * @code
 * UProjectHotReloadManager* Manager = GetGameInstance()->GetSubsystem<UProjectHotReloadManager>();
 * FProjectHotReloadValidationResult Result = Manager->ValidateLayoutFile(ConfigPath);
 * if (Result.bSuccess)
 * {
 *     Manager->ReloadWidgetLayout(MyWidget, ConfigPath);
 * }
 * @endcode
 */
UCLASS()
class PROJECTUI_API UProjectHotReloadManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	/**
	 * Validate a JSON layout file without applying changes.
	 *
	 * This performs a dry-run of JSON parsing and widget creation to detect
	 * errors before they crash the editor.
	 *
	 * @param JsonFilePath - Absolute path to JSON config file
	 * @param Theme - Theme data for resolving references (optional)
	 * @return Validation result with errors/warnings
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	FProjectHotReloadValidationResult ValidateLayoutFile(const FString& JsonFilePath, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Validate a JSON layout string without applying changes.
	 *
	 * @param JsonString - JSON string to validate
	 * @param Theme - Theme data for resolving references (optional)
	 * @return Validation result with errors/warnings
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	FProjectHotReloadValidationResult ValidateLayoutString(const FString& JsonString, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Reload widget layout from JSON file with validation.
	 *
	 * This validates the JSON first, then applies changes if validation succeeds.
	 * If reload fails, the previous valid state can be restored via Rollback().
	 *
	 * @param TargetWidget - Widget to reload (e.g., UProjectUserWidget)
	 * @param JsonFilePath - Absolute path to JSON config file
	 * @param Theme - Theme data for resolving references (optional)
	 * @return Validation result (check bSuccess before assuming reload worked)
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	FProjectHotReloadValidationResult ReloadWidgetLayout(UObject* TargetWidget, const FString& JsonFilePath, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Create snapshot of current widget layout for rollback.
	 *
	 * Call this before reloading to enable rollback capability.
	 *
	 * @param JsonFilePath - File path associated with this snapshot
	 * @param JsonString - Current valid JSON string
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	void CreateSnapshot(const FString& JsonFilePath, const FString& JsonString);

	/**
	 * Rollback to last valid snapshot.
	 *
	 * Restores the last successful layout state. Returns nullptr if no snapshot exists.
	 *
	 * @param Outer - Outer object for widget creation
	 * @param JsonFilePath - File path to restore
	 * @param Theme - Theme data for resolving references (optional)
	 * @return Root widget from snapshot, or nullptr if no snapshot
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	UWidget* RollbackToSnapshot(UObject* Outer, const FString& JsonFilePath, UProjectUIThemeData* Theme = nullptr);

	/**
	 * Get the last validation result.
	 *
	 * Useful for displaying errors in debug overlay.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Hot Reload")
	FProjectHotReloadValidationResult GetLastValidationResult() const { return LastValidationResult; }

	/**
	 * Enable/disable debug overlay showing reload status.
	 *
	 * The overlay displays validation errors, warnings, and reload events in real-time.
	 *
	 * @param bEnabled - Whether to show debug overlay
	 */
	UFUNCTION(BlueprintCallable, Category = "Project UI|Hot Reload")
	void SetDebugOverlayEnabled(bool bEnabled);

	/**
	 * Check if debug overlay is enabled.
	 */
	UFUNCTION(BlueprintPure, Category = "Project UI|Hot Reload")
	bool IsDebugOverlayEnabled() const { return bDebugOverlayEnabled; }

private:
	/**
	 * Perform deep validation of JSON object recursively.
	 *
	 * Checks:
	 * - Required fields present (type, etc.)
	 * - Valid widget types
	 * - Valid property values
	 * - Children structure
	 *
	 * @param JsonObject - JSON object to validate
	 * @param OutErrors - Array to append errors to
	 * @param OutWarnings - Array to append warnings to
	 * @param Theme - Theme for validating color/font references
	 * @return Number of widgets that would be created
	 */
	int32 ValidateJsonObject(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& OutErrors, TArray<FString>& OutWarnings, UProjectUIThemeData* Theme);

	/**
	 * Validate widget type string.
	 *
	 * @param Type - Widget type string from JSON
	 * @return True if type is supported
	 */
	bool IsValidWidgetType(const FString& Type) const;

	/**
	 * Validate theme color reference.
	 *
	 * @param ColorName - Color name from JSON
	 * @param Theme - Theme to check against
	 * @return True if color exists in theme
	 */
	bool IsValidThemeColor(const FString& ColorName, UProjectUIThemeData* Theme) const;

	/**
	 * Validate theme font reference.
	 *
	 * @param FontName - Font name from JSON
	 * @param Theme - Theme to check against
	 * @return True if font exists in theme
	 */
	bool IsValidThemeFont(const FString& FontName, UProjectUIThemeData* Theme) const;

	/**
	 * Show debug notification for validation result.
	 *
	 * Displays on-screen message if debug overlay is enabled.
	 */
	void ShowDebugNotification(const FProjectHotReloadValidationResult& Result);

private:
	/** Last validation result (for debug overlay display) */
	UPROPERTY(Transient)
	FProjectHotReloadValidationResult LastValidationResult;

	/** Snapshots for rollback (keyed by file path) */
	UPROPERTY(Transient)
	TMap<FString, FProjectHotReloadSnapshot> Snapshots;

	/** Whether debug overlay is enabled */
	UPROPERTY(Transient)
	bool bDebugOverlayEnabled = false;

	/** Supported widget types (populated in Initialize) */
	TSet<FString> SupportedWidgetTypes;
};
