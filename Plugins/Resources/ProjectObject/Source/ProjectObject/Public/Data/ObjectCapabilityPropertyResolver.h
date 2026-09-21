// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/TopLevelAssetPath.h"

struct FObjectCapabilityEntry;
class UObjectDefinition;

/** Shared reflected-property contract for ObjectDefinition capabilities. */
namespace ProjectObjectCapabilityProperties
{
	PROJECTOBJECT_API bool ValidateEntry(
		const FObjectCapabilityEntry& Entry,
		FString& OutError);

	PROJECTOBJECT_API bool ValidateDefinition(
		const UObjectDefinition* Definition,
		FString& OutError);

	PROJECTOBJECT_API bool ApplyProperty(
		UObject* Object,
		FName PropertyName,
		const FString& Value,
		FString* OutError = nullptr);

	PROJECTOBJECT_API bool ApplyEntryProperties(
		UObject* Object,
		const FObjectCapabilityEntry& Entry,
		FString& OutError);

	PROJECTOBJECT_API bool CollectSoftReferences(
		const FObjectCapabilityEntry& Entry,
		TArray<FTopLevelAssetPath>& OutReferences,
		FString& OutError);
}
