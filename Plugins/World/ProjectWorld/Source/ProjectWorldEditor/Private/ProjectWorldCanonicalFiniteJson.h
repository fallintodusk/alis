// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "Dom/JsonObject.h"

namespace ProjectWorldCanonicalJson
{
	inline bool FiniteDocument(const TSharedPtr<FJsonObject>& Object)
	{
		if (!Object) return false;
		TArray<TSharedPtr<FJsonValue>> Pending{MakeShared<FJsonValueObject>(Object)};
		while (!Pending.IsEmpty())
		{
			const auto Value=Pending.Pop(EAllowShrinking::No);
			if (!Value || Value->Type==EJson::None) return false;
			if (Value->Type==EJson::Number)
			{
				double Number;
				if (!Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
			}
			else if (Value->Type==EJson::Array) Pending.Append(Value->AsArray());
			else if (Value->Type==EJson::Object)
			{
				const auto Child=Value->AsObject();
				if (!Child) return false;
				for (const auto& Pair : Child->Values) Pending.Add(Pair.Value);
			}
		}
		return true;
	}
}
