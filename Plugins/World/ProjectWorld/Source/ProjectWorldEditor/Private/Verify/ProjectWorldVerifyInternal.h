// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProjectWorldVerify, Log, All);

namespace ProjectWorldVerifyJson
{
	/** JSON string literal with every non-ASCII or control character escaped, so files stay ASCII. */
	inline FString Quote(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len() + 2);
		Result.AppendChar(TEXT('"'));
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('"'): Result += TEXT("\\\""); break;
			case TEXT('\\'): Result += TEXT("\\\\"); break;
			case TEXT('\n'): Result += TEXT("\\n"); break;
			case TEXT('\r'): Result += TEXT("\\r"); break;
			case TEXT('\t'): Result += TEXT("\\t"); break;
			default:
				if (Character < 0x20 || Character > 0x7e)
				{
					Result += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Character) & 0xffff);
				}
				else
				{
					Result.AppendChar(Character);
				}
				break;
			}
		}
		Result.AppendChar(TEXT('"'));
		return Result;
	}

	/** Ordinal, case-sensitive order used for records, fields, and fixture names. */
	inline bool Less(const FString& Left, const FString& Right)
	{
		return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
	}

	inline TArray<FString> SortedKeys(const TMap<FString, FString>& Map)
	{
		TArray<FString> Keys;
		Map.GetKeys(Keys);
		Keys.Sort([](const FString& Left, const FString& Right) { return Less(Left, Right); });
		return Keys;
	}

	inline bool ReadPositiveInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& OutValue)
	{
		double Value = 0.0;
		if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value) || Value < 1.0 ||
			Value > static_cast<double>(MAX_int32) || Value != FMath::FloorToDouble(Value))
		{
			return false;
		}
		OutValue = static_cast<int32>(Value);
		return true;
	}
}

namespace ProjectWorldVerifyPaths
{
	inline FString ProjectDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	inline FString RepoRelative(const FString& AbsolutePath)
	{
		FString Relative = AbsolutePath;
		FPaths::MakePathRelativeTo(Relative, *ProjectDirectory());
		return Relative.Replace(TEXT("\\"), TEXT("/"));
	}

	/** Project.World.Realization.Verify.Water -> Water, the record script's -Verify value. */
	inline FString ShortVerifyName(const FString& Verify)
	{
		FString Left;
		FString Right;
		return Verify.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Right : Verify;
	}
}
