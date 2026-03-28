// Copyright ALIS. All Rights Reserved.

#include "ProjectHotReloadManager.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Theme/ProjectUIThemeData.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogProjectHotReload, Log, All);

void UProjectHotReloadManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize supported widget types (matches ProjectWidgetLayoutLoader)
	SupportedWidgetTypes = {
		TEXT("CanvasPanel"),
		TEXT("VerticalBox"),
		TEXT("HorizontalBox"),
		TEXT("Overlay"),
		TEXT("Button"),
		TEXT("TextBlock"),
		TEXT("Image"),
		TEXT("ProgressBar"),
		TEXT("RadialProgress"),
		TEXT("EditableText"),
		TEXT("ListView"),
		TEXT("Spacer"),
		TEXT("Border"),
		TEXT("ComboBox"),
		TEXT("CheckBox"),
		TEXT("Slider"),
		TEXT("SizeBox"),
		TEXT("ScrollBox"),
		TEXT("FireSparks"),
		TEXT("Dialog")
	};

	UE_LOG(LogProjectHotReload, Log, TEXT("ProjectHotReloadManager initialized with %d supported widget types"), SupportedWidgetTypes.Num());
}

void UProjectHotReloadManager::Deinitialize()
{
	Snapshots.Empty();
	SupportedWidgetTypes.Empty();

	Super::Deinitialize();
}

FProjectHotReloadValidationResult UProjectHotReloadManager::ValidateLayoutFile(const FString& JsonFilePath, UProjectUIThemeData* Theme)
{
	FProjectHotReloadValidationResult Result;
	Result.FilePath = JsonFilePath;
	Result.Timestamp = FDateTime::Now();

	// Read JSON file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		Result.bSuccess = false;
		Result.ErrorMessages.Add(FString::Printf(TEXT("Failed to load JSON file: %s"), *JsonFilePath));

		UE_LOG(LogProjectHotReload, Error, TEXT("Validation failed: Could not read file %s"), *JsonFilePath);
		ShowDebugNotification(Result);
		return Result;
	}

	return ValidateLayoutString(JsonString, Theme);
}

FProjectHotReloadValidationResult UProjectHotReloadManager::ValidateLayoutString(const FString& JsonString, UProjectUIThemeData* Theme)
{
	FProjectHotReloadValidationResult Result;
	Result.Timestamp = FDateTime::Now();

	// Parse JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessages.Add(TEXT("Invalid JSON syntax - failed to parse"));

		UE_LOG(LogProjectHotReload, Error, TEXT("Validation failed: Invalid JSON syntax"));
		ShowDebugNotification(Result);
		LastValidationResult = Result;
		return Result;
	}

	// Check for root object
	TSharedPtr<FJsonObject> RootObject = JsonObject->GetObjectField(TEXT("root"));
	if (!RootObject.IsValid())
	{
		Result.bSuccess = false;
		Result.ErrorMessages.Add(TEXT("JSON missing required 'root' object"));

		UE_LOG(LogProjectHotReload, Error, TEXT("Validation failed: Missing 'root' object"));
		ShowDebugNotification(Result);
		LastValidationResult = Result;
		return Result;
	}

	// Deep validation of widget tree
	TArray<FString> Errors;
	TArray<FString> Warnings;
	Result.WidgetCount = ValidateJsonObject(RootObject, Errors, Warnings, Theme);

	Result.ErrorMessages = MoveTemp(Errors);
	Result.WarningMessages = MoveTemp(Warnings);
	Result.bSuccess = Result.ErrorMessages.Num() == 0;

	if (Result.bSuccess)
	{
		UE_LOG(LogProjectHotReload, Display, TEXT("Validation succeeded: %d widgets, %d warnings"), Result.WidgetCount, Result.WarningMessages.Num());
	}
	else
	{
		UE_LOG(LogProjectHotReload, Error, TEXT("Validation failed: %d errors, %d warnings"), Result.ErrorMessages.Num(), Result.WarningMessages.Num());
	}

	ShowDebugNotification(Result);
	LastValidationResult = Result;
	return Result;
}

FProjectHotReloadValidationResult UProjectHotReloadManager::ReloadWidgetLayout(UObject* TargetWidget, const FString& JsonFilePath, UProjectUIThemeData* Theme)
{
	// Validate first
	FProjectHotReloadValidationResult Result = ValidateLayoutFile(JsonFilePath, Theme);

	if (!Result.bSuccess)
	{
		UE_LOG(LogProjectHotReload, Warning, TEXT("Reload aborted: Validation failed for %s"), *JsonFilePath);
		return Result;
	}

	// Validation succeeded - read JSON for snapshot
	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		CreateSnapshot(JsonFilePath, JsonString);
	}

	UE_LOG(LogProjectHotReload, Display, TEXT("Hot reload succeeded for %s"), *JsonFilePath);
	return Result;
}

void UProjectHotReloadManager::CreateSnapshot(const FString& JsonFilePath, const FString& JsonString)
{
	FProjectHotReloadSnapshot Snapshot;
	Snapshot.LastValidJson = JsonString;
	Snapshot.FilePath = JsonFilePath;
	Snapshot.Timestamp = FDateTime::Now();
	Snapshot.bIsValid = true;

	Snapshots.Add(JsonFilePath, Snapshot);

	UE_LOG(LogProjectHotReload, Verbose, TEXT("Created snapshot for %s"), *JsonFilePath);
}

UWidget* UProjectHotReloadManager::RollbackToSnapshot(UObject* Outer, const FString& JsonFilePath, UProjectUIThemeData* Theme)
{
	FProjectHotReloadSnapshot* Snapshot = Snapshots.Find(JsonFilePath);
	if (!Snapshot || !Snapshot->bIsValid)
	{
		UE_LOG(LogProjectHotReload, Warning, TEXT("No valid snapshot found for %s"), *JsonFilePath);
		return nullptr;
	}

	UWidget* RestoredWidget = UProjectWidgetLayoutLoader::LoadLayoutFromString(Outer, Snapshot->LastValidJson, Theme);
	if (RestoredWidget)
	{
		UE_LOG(LogProjectHotReload, Display, TEXT("Rolled back to snapshot for %s (timestamp: %s)"), *JsonFilePath, *Snapshot->Timestamp.ToString());
	}
	else
	{
		UE_LOG(LogProjectHotReload, Error, TEXT("Failed to rollback %s - snapshot was invalid"), *JsonFilePath);
	}

	return RestoredWidget;
}

void UProjectHotReloadManager::SetDebugOverlayEnabled(bool bEnabled)
{
	bDebugOverlayEnabled = bEnabled;
	UE_LOG(LogProjectHotReload, Log, TEXT("Debug overlay %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

int32 UProjectHotReloadManager::ValidateJsonObject(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& OutErrors, TArray<FString>& OutWarnings, UProjectUIThemeData* Theme)
{
	if (!JsonObject.IsValid())
	{
		OutErrors.Add(TEXT("Invalid JSON object (null)"));
		return 0;
	}

	int32 WidgetCount = 1; // Count this widget

	// Validate required 'type' field
	FString Type;
	if (!JsonObject->TryGetStringField(TEXT("type"), Type))
	{
		OutErrors.Add(TEXT("Widget missing required 'type' field"));
		return WidgetCount;
	}

	// Validate widget type
	if (!IsValidWidgetType(Type))
	{
		OutErrors.Add(FString::Printf(TEXT("Unknown widget type: '%s'"), *Type));
		return WidgetCount;
	}

	// Validate type-specific properties
	if (Type == TEXT("Button"))
	{
		// Check for font reference
		FString FontName;
		if (JsonObject->TryGetStringField(TEXT("font"), FontName) && Theme)
		{
			if (!IsValidThemeFont(FontName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme font: '%s' (will fallback to default)"), *FontName));
			}
		}
	}
	else if (Type == TEXT("TextBlock"))
	{
		// Check for color reference
		FString ColorName;
		if (JsonObject->TryGetStringField(TEXT("color"), ColorName) && Theme)
		{
			if (!IsValidThemeColor(ColorName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme color: '%s' (will fallback to white)"), *ColorName));
			}
		}

		// Check for font reference
		FString FontName;
		if (JsonObject->TryGetStringField(TEXT("font"), FontName) && Theme)
		{
			if (!IsValidThemeFont(FontName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme font: '%s' (will fallback to default)"), *FontName));
			}
		}
	}
	else if (Type == TEXT("ProgressBar"))
	{
		// Check for color reference
		FString ColorName;
		if (JsonObject->TryGetStringField(TEXT("fillColor"), ColorName) && Theme)
		{
			if (!IsValidThemeColor(ColorName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme color: '%s' (will fallback to white)"), *ColorName));
			}
		}

		// Validate percent value
		double Percent;
		if (JsonObject->TryGetNumberField(TEXT("percent"), Percent))
		{
			if (Percent < 0.0 || Percent > 1.0)
			{
				OutWarnings.Add(FString::Printf(TEXT("ProgressBar percent out of range: %.2f (should be 0.0-1.0)"), Percent));
			}
		}
	}
	else if (Type == TEXT("RadialProgress"))
	{
		FString ColorName;
		if (JsonObject->TryGetStringField(TEXT("fillColor"), ColorName) && Theme)
		{
			if (!IsValidThemeColor(ColorName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme color: '%s' (will fallback to white)"), *ColorName));
			}
		}

		FString TrackColorName;
		if (JsonObject->TryGetStringField(TEXT("trackColor"), TrackColorName) && Theme)
		{
			if (!IsValidThemeColor(TrackColorName, Theme))
			{
				OutWarnings.Add(FString::Printf(TEXT("Unknown theme color: '%s' (will fallback to white)"), *TrackColorName));
			}
		}

		double Percent = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("percent"), Percent))
		{
			if (Percent < 0.0 || Percent > 1.0)
			{
				OutWarnings.Add(FString::Printf(TEXT("RadialProgress percent out of range: %.2f (should be 0.0-1.0)"), Percent));
			}
		}

		double Thickness = 0.0;
		if (JsonObject->TryGetNumberField(TEXT("thickness"), Thickness) && Thickness < 1.0)
		{
			OutWarnings.Add(FString::Printf(TEXT("RadialProgress thickness too small: %.2f (minimum 1.0)"), Thickness));
		}
	}

	// Validate children (recursively)
	const TArray<TSharedPtr<FJsonValue>>* ChildrenArray;
	if (JsonObject->TryGetArrayField(TEXT("children"), ChildrenArray))
	{
		for (int32 i = 0; i < ChildrenArray->Num(); ++i)
		{
			TSharedPtr<FJsonObject> ChildObject = (*ChildrenArray)[i]->AsObject();
			if (ChildObject.IsValid())
			{
				WidgetCount += ValidateJsonObject(ChildObject, OutErrors, OutWarnings, Theme);
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("Child [%d] is not a valid JSON object"), i));
			}
		}
	}

	return WidgetCount;
}

bool UProjectHotReloadManager::IsValidWidgetType(const FString& Type) const
{
	return SupportedWidgetTypes.Contains(Type);
}

bool UProjectHotReloadManager::IsValidThemeColor(const FString& ColorName, UProjectUIThemeData* Theme) const
{
	if (!Theme)
	{
		return false;
	}

	// Check if color exists in theme
	static const TArray<FString> ValidColors = {
		TEXT("Primary"),
		TEXT("Secondary"),
		TEXT("Background"),
		TEXT("Surface"),
		TEXT("Error"),
		TEXT("Warning"),
		TEXT("Success"),
		TEXT("Text"),
		TEXT("TextSecondary"),
		TEXT("Border")
	};

	return ValidColors.Contains(ColorName);
}

bool UProjectHotReloadManager::IsValidThemeFont(const FString& FontName, UProjectUIThemeData* Theme) const
{
	if (!Theme)
	{
		return false;
	}

	// Check if font exists in theme
	static const TArray<FString> ValidFonts = {
		TEXT("HeadingLarge"),
		TEXT("HeadingMedium"),
		TEXT("HeadingSmall"),
		TEXT("BodyLarge"),
		TEXT("BodyMedium"),
		TEXT("BodySmall"),
		TEXT("Button"),
		TEXT("Label")
	};

	return ValidFonts.Contains(FontName);
}

void UProjectHotReloadManager::ShowDebugNotification(const FProjectHotReloadValidationResult& Result)
{
	if (!bDebugOverlayEnabled)
	{
		return;
	}

#if WITH_EDITOR
	FText NotificationText;
	if (Result.bSuccess)
	{
		if (Result.WarningMessages.Num() > 0)
		{
			NotificationText = FText::FromString(FString::Printf(TEXT("Hot Reload: %d widgets, %d warnings"), Result.WidgetCount, Result.WarningMessages.Num()));
		}
		else
		{
			NotificationText = FText::FromString(FString::Printf(TEXT("Hot Reload: Success (%d widgets)"), Result.WidgetCount));
		}
	}
	else
	{
		NotificationText = FText::FromString(FString::Printf(TEXT("Hot Reload: Failed (%d errors)"), Result.ErrorMessages.Num()));
	}

	FNotificationInfo Info(NotificationText);
	Info.ExpireDuration = 3.0f;
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = true;

	if (Result.bSuccess)
	{
		if (Result.WarningMessages.Num() > 0)
		{
			// Success with warnings - show as info
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		else
		{
			// Full success
			FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
		}
	}
	else
	{
		// Failure
		FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
	}
#endif
}
