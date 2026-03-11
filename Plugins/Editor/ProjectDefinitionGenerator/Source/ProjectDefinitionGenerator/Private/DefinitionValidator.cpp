// Copyright ALIS. All Rights Reserved.

#include "DefinitionValidator.h"

#include "CapabilityRegistry.h"
#include "CapabilityValidationRegistry.h"
#include "Data/ObjectDefinition.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/CoreStyle.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogDefinitionValidator, Log, All);

TArray<FDefinitionValidationError> FDefinitionValidator::ValidateAsset(
	const FDefinitionTypeInfo& TypeInfo,
	UObject* Asset,
	const FString& AssetId)
{
	TArray<FDefinitionValidationError> Errors;

	if (!Asset)
	{
		FDefinitionValidationError Error;
		Error.AssetId = AssetId;
		Error.Message = TEXT("Asset is null");
		Errors.Add(Error);
		return Errors;
	}

	// Validate soft object references
	ValidateSoftObjectReferences(Asset, AssetId, Errors);

	// Validate soft class references
	ValidateSoftClassReferences(Asset, AssetId, Errors);

	// Object-specific validation: capabilities (scope references, mode exclusivity)
	if (const UObjectDefinition* ObjectDef = Cast<UObjectDefinition>(Asset))
	{
		ValidateObjectCapabilities(ObjectDef, AssetId, Errors);
	}

	return Errors;
}

void FDefinitionValidator::ShowValidationNotification(
	const TArray<FDefinitionValidationError>& Errors,
	const FString& TypeName)
{
	if (Errors.Num() == 0)
	{
		return;
	}

	// Count errors vs warnings
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	for (const FDefinitionValidationError& Error : Errors)
	{
		if (Error.bIsWarning)
		{
			WarningCount++;
		}
		else
		{
			ErrorCount++;
		}
	}

	// Log all errors to output log
	for (const FDefinitionValidationError& Error : Errors)
	{
		if (Error.bIsWarning)
		{
			UE_LOG(LogDefinitionValidator, Warning, TEXT("[%s] %s.%s: %s"),
				*TypeName, *Error.AssetId, *Error.PropertyPath, *Error.Message);
		}
		else
		{
			UE_LOG(LogDefinitionValidator, Error, TEXT("[%s] %s.%s: %s"),
				*TypeName, *Error.AssetId, *Error.PropertyPath, *Error.Message);
		}
	}

	// Build details string for notification (first few errors with messages)
	FString Details;
	const int32 MaxDetailsToShow = 5;
	int32 ShownCount = 0;
	for (const FDefinitionValidationError& Error : Errors)
	{
		if (ShownCount >= MaxDetailsToShow)
		{
			break;
		}
		if (!Details.IsEmpty())
		{
			Details += TEXT("\n");
		}
		Details += FString::Printf(TEXT("%s.%s: %s"),
			*Error.AssetId, *Error.PropertyPath, *Error.Message);
		ShownCount++;
	}
	if (Errors.Num() > MaxDetailsToShow)
	{
		Details += FString::Printf(TEXT("\n(+%d more - see Output Log)"), Errors.Num() - MaxDetailsToShow);
	}

	// Show ERROR notification (red popup with Dismiss button)
	FNotificationInfo Info(FText::Format(
		NSLOCTEXT("DefinitionValidator", "ValidationNotification", "ERROR: [{0}] {1} errors, {2} warnings\n{3}"),
		FText::FromString(TypeName),
		FText::AsNumber(ErrorCount),
		FText::AsNumber(WarningCount),
		FText::FromString(Details)
	));
	Info.bFireAndForget = false;
	Info.bUseSuccessFailIcons = true;
	Info.ExpireDuration = 60.0f;
	Info.FadeOutDuration = 0.5f;
	Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.Error"));

	// Dismiss button - 4th param sets completion state which triggers dismiss
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		NSLOCTEXT("DefinitionValidator", "DismissButton", "Dismiss"),
		NSLOCTEXT("DefinitionValidator", "DismissTooltip", "Dismiss this notification"),
		FSimpleDelegate(),
		SNotificationItem::CS_None
	));

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

bool FDefinitionValidator::ValidateAndNotify(
	const FDefinitionTypeInfo& TypeInfo,
	UObject* Asset,
	const FString& AssetId,
	const FString& TypeName)
{
	TArray<FDefinitionValidationError> Errors = ValidateAsset(TypeInfo, Asset, AssetId);

	if (Errors.Num() > 0)
	{
		ShowValidationNotification(Errors, TypeName);
	}

	// Return true if no errors (warnings are OK)
	for (const FDefinitionValidationError& Error : Errors)
	{
		if (!Error.bIsWarning)
		{
			return false;
		}
	}
	return true;
}

void FDefinitionValidator::ValidateSoftObjectReferences(
	UObject* Asset,
	const FString& AssetId,
	TArray<FDefinitionValidationError>& OutErrors)
{
	if (!Asset)
	{
		return;
	}

	UClass* AssetClass = Asset->GetClass();

	// Iterate all properties looking for TSoftObjectPtr
	for (TFieldIterator<FProperty> PropIt(AssetClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Check for FSoftObjectProperty (TSoftObjectPtr<T>)
		if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr SoftPtr = SoftObjProp->GetPropertyValue_InContainer(Asset);
			if (!SoftPtr.IsNull() && SoftPtr.IsPending())
			{
				// Try to resolve
				UObject* Resolved = SoftPtr.LoadSynchronous();
				if (!Resolved)
				{
					FDefinitionValidationError Error;
					Error.AssetId = AssetId;
					Error.PropertyPath = Property->GetName();
					Error.Message = FString::Printf(TEXT("Failed to resolve asset: %s"), *SoftPtr.ToString());
					Error.bIsWarning = true;
					OutErrors.Add(Error);
				}
			}
		}

		// Check for array of soft object ptrs
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			if (FSoftObjectProperty* InnerSoftObjProp = CastField<FSoftObjectProperty>(ArrayProp->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Asset));
				for (int32 i = 0; i < ArrayHelper.Num(); ++i)
				{
					FSoftObjectPtr SoftPtr = InnerSoftObjProp->GetPropertyValue(ArrayHelper.GetRawPtr(i));
					if (!SoftPtr.IsNull() && SoftPtr.IsPending())
					{
						UObject* Resolved = SoftPtr.LoadSynchronous();
						if (!Resolved)
						{
							FDefinitionValidationError Error;
							Error.AssetId = AssetId;
							Error.PropertyPath = FString::Printf(TEXT("%s[%d]"), *Property->GetName(), i);
							Error.Message = FString::Printf(TEXT("Failed to resolve asset: %s"), *SoftPtr.ToString());
							Error.bIsWarning = true;
							OutErrors.Add(Error);
						}
					}
				}
			}
		}
	}
}

void FDefinitionValidator::ValidateSoftClassReferences(
	UObject* Asset,
	const FString& AssetId,
	TArray<FDefinitionValidationError>& OutErrors)
{
	if (!Asset)
	{
		return;
	}

	UClass* AssetClass = Asset->GetClass();

	// Iterate all properties looking for TSoftClassPtr
	for (TFieldIterator<FProperty> PropIt(AssetClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Check for FSoftClassProperty (TSoftClassPtr<T>)
		if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			FSoftObjectPtr SoftPtr = SoftClassProp->GetPropertyValue_InContainer(Asset);
			if (!SoftPtr.IsNull() && SoftPtr.IsPending())
			{
				// Try to resolve
				UObject* Resolved = SoftPtr.LoadSynchronous();
				if (!Resolved)
				{
					FDefinitionValidationError Error;
					Error.AssetId = AssetId;
					Error.PropertyPath = Property->GetName();
					Error.Message = FString::Printf(TEXT("Failed to resolve class: %s"), *SoftPtr.ToString());
					Error.bIsWarning = true;
					OutErrors.Add(Error);
				}
			}
		}

		// Check for array of soft class ptrs
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			if (FSoftClassProperty* InnerSoftClassProp = CastField<FSoftClassProperty>(ArrayProp->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Asset));
				for (int32 i = 0; i < ArrayHelper.Num(); ++i)
				{
					FSoftObjectPtr SoftPtr = InnerSoftClassProp->GetPropertyValue(ArrayHelper.GetRawPtr(i));
					if (!SoftPtr.IsNull() && SoftPtr.IsPending())
					{
						UObject* Resolved = SoftPtr.LoadSynchronous();
						if (!Resolved)
						{
							FDefinitionValidationError Error;
							Error.AssetId = AssetId;
							Error.PropertyPath = FString::Printf(TEXT("%s[%d]"), *Property->GetName(), i);
							Error.Message = FString::Printf(TEXT("Failed to resolve class: %s"), *SoftPtr.ToString());
							Error.bIsWarning = true;
							OutErrors.Add(Error);
						}
					}
				}
			}
		}
	}
}

// -------------------------------------------------------------------------
// Object Capability Validation
// -------------------------------------------------------------------------

void FDefinitionValidator::ValidateObjectCapabilities(
	const UObjectDefinition* ObjectDef,
	const FString& AssetId,
	TArray<FDefinitionValidationError>& OutErrors)
{
	if (!ObjectDef)
	{
		return;
	}

	// Collect valid mesh IDs from definition
	TSet<FName> ValidMeshIds;
	for (const FObjectMeshEntry& Mesh : ObjectDef->Meshes)
	{
		ValidMeshIds.Add(Mesh.Id);
	}
	// "actor" is always valid (per-actor scope)
	ValidMeshIds.Add(NAME_CapabilityScope_Actor);

	// Validate each capability entry
	for (int32 CapIndex = 0; CapIndex < ObjectDef->Capabilities.Num(); ++CapIndex)
	{
		const FObjectCapabilityEntry& Cap = ObjectDef->Capabilities[CapIndex];

		// Validate scope is not empty
		if (Cap.Scope.Num() == 0)
		{
			AddCapabilityError(OutErrors, AssetId, CapIndex, Cap.Type.ToString(),
				TEXT("Scope"), TEXT("Capability scope cannot be empty"), /*bIsWarning=*/false);
			continue;
		}

		// Validate scope mesh IDs exist
		TSet<FName> SeenScopes;
		for (const FName& ScopeId : Cap.Scope)
		{
			if (!ValidMeshIds.Contains(ScopeId))
			{
				AddCapabilityError(OutErrors, AssetId, CapIndex, Cap.Type.ToString(),
					TEXT("Scope"),
					FString::Printf(TEXT("Scope '%s' not found in meshes array"), *ScopeId.ToString()),
					/*bIsWarning=*/false);
			}

			if (SeenScopes.Contains(ScopeId))
			{
				AddCapabilityError(OutErrors, AssetId, CapIndex, Cap.Type.ToString(),
					TEXT("Scope"),
					FString::Printf(TEXT("Duplicate scope entry '%s' - component will be created twice"), *ScopeId.ToString()),
					/*bIsWarning=*/true);
			}
			SeenScopes.Add(ScopeId);
		}

		// Capability-specific validation via registry. See: CapabilityValidationRegistry.h
		if (const FCapabilityValidateFunc* ValidateFunc = FCapabilityValidationRegistry::GetValidationFunc(Cap.Type))
		{
			TArray<FCapabilityValidationResult> Results = (*ValidateFunc)(Cap.Properties);
			for (const FCapabilityValidationResult& R : Results)
			{
				AddCapabilityError(OutErrors, AssetId, CapIndex, Cap.Type.ToString(),
					R.PropertyPath, R.Message, R.bIsWarning);
			}
		}
	}
}

void FDefinitionValidator::AddCapabilityError(
	TArray<FDefinitionValidationError>& OutErrors,
	const FString& AssetId,
	int32 CapIndex,
	const FString& CapabilityType,
	const FString& PropertyPath,
	const FString& Message,
	bool bIsWarning)
{
	FDefinitionValidationError Error;
	Error.AssetId = AssetId;
	Error.PropertyPath = PropertyPath.IsEmpty()
		? FString::Printf(TEXT("Capabilities[%d](%s)"), CapIndex, *CapabilityType)
		: FString::Printf(TEXT("Capabilities[%d](%s).%s"), CapIndex, *CapabilityType, *PropertyPath);
	Error.Message = Message;
	Error.bIsWarning = bIsWarning;
	OutErrors.Add(Error);
}
