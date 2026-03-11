// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

/**
 * Static validation helper utilities for common validation patterns.
 *
 * Reduces boilerplate for null checks, empty checks, and validation logic.
 * All methods are static - no instance required.
 *
 * Usage:
 *   if (!FProjectValidationHelpers::IsValidName(RegionId, OutError)) { return false; }
 *   if (!FProjectValidationHelpers::IsValidSoftObjectPath(AssetPath, OutError)) { return false; }
 */
class FProjectValidationHelpers
{
public:
	/** Validate FName is not None. Appends error if invalid. */
	static bool IsValidName(FName Name, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Name == NAME_None)
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("Name is None (empty)")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is None (empty)")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate FText is not empty. Appends error if invalid. */
	static bool IsValidText(const FText& Text, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Text.IsEmpty())
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("Text is empty")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is empty")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate FString is not empty. Appends error if invalid. */
	static bool IsValidString(const FString& String, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (String.IsEmpty())
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("String is empty")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is empty")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate FSoftObjectPath is not null. Appends error if invalid. */
	static bool IsValidSoftObjectPath(const FSoftObjectPath& Path, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Path.IsNull())
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("SoftObjectPath is null")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is null")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate TSoftObjectPtr is not null. Appends error if invalid. */
	template<typename T>
	static bool IsValidSoftObjectPtr(const TSoftObjectPtr<T>& Ptr, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Ptr.IsNull())
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("SoftObjectPtr is null")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is null")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate UObject pointer is valid. Appends error if invalid. */
	template<typename T>
	static bool IsValidObject(T* Object, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (!IsValid(Object))
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("Object is null or pending kill")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is null or pending kill")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate array is not empty. Appends error if invalid. */
	template<typename T>
	static bool IsValidArray(const TArray<T>& Array, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Array.Num() == 0)
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(TEXT("Array is empty")));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} is empty")),
					FieldName
				));
			}
			return false;
		}
		return true;
	}

	/** Validate number is within range [Min, Max]. Appends error if invalid. */
	template<typename T>
	static bool IsInRange(T Value, T Min, T Max, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Value < Min || Value > Max)
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("Value {0} is out of range [{1}, {2}]")),
					FText::AsNumber(Value),
					FText::AsNumber(Min),
					FText::AsNumber(Max)
				));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} value {1} is out of range [{2}, {3}]")),
					FieldName,
					FText::AsNumber(Value),
					FText::AsNumber(Min),
					FText::AsNumber(Max)
				));
			}
			return false;
		}
		return true;
	}

	/** Validate number is greater than zero. Appends error if invalid. */
	template<typename T>
	static bool IsPositive(T Value, TArray<FText>& OutErrors, const FText& FieldName = FText())
	{
		if (Value <= 0)
		{
			if (FieldName.IsEmpty())
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("Value {0} must be positive")),
					FText::AsNumber(Value)
				));
			}
			else
			{
				OutErrors.Add(FText::Format(
					FText::FromString(TEXT("{0} value {1} must be positive")),
					FieldName,
					FText::AsNumber(Value)
				));
			}
			return false;
		}
		return true;
	}

	/** Validate all array elements pass predicate. Appends errors for failing elements. */
	template<typename T>
	static bool ValidateArrayElements(
		const TArray<T>& Array,
		TFunction<bool(const T&, TArray<FText>&)> Validator,
		TArray<FText>& OutErrors,
		const FText& ArrayName = FText())
	{
		bool bAllValid = true;
		for (int32 i = 0; i < Array.Num(); ++i)
		{
			TArray<FText> ElementErrors;
			if (!Validator(Array[i], ElementErrors))
			{
				bAllValid = false;

				// Prefix element errors with array name and index
				for (const FText& Error : ElementErrors)
				{
					if (ArrayName.IsEmpty())
					{
						OutErrors.Add(FText::Format(
							FText::FromString(TEXT("[{0}] {1}")),
							FText::AsNumber(i),
							Error
						));
					}
					else
					{
						OutErrors.Add(FText::Format(
							FText::FromString(TEXT("{0}[{1}] {2}")),
							ArrayName,
							FText::AsNumber(i),
							Error
						));
					}
				}
			}
		}
		return bAllValid;
	}
};
