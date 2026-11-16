// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.
#pragma once
#include "Dom/JsonObject.h"

namespace ProjectWorldCanonicalJson
{
    inline bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
    {
        if (!A || !B || A->Type!=B->Type) return false;
        if (A->Type==EJson::Object)
        {
            const auto X=A->AsObject(),Y=B->AsObject();
            if (!X || !Y || X->Values.Num()!=Y->Values.Num()) return false;
            for (const auto& Pair:X->Values) if (!Equal(Pair.Value,Y->TryGetField(Pair.Key))) return false;
            return true;
        }
        if (A->Type==EJson::Array)
        {
            const auto& X=A->AsArray();const auto& Y=B->AsArray();if (X.Num()!=Y.Num()) return false;
            for (int32 I=0;I<X.Num();++I) if (!Equal(X[I],Y[I])) return false;
            return true;
        }
        if (A->Type==EJson::Number) return A->AsNumber()==B->AsNumber();
        if (A->Type==EJson::String) return A->AsString()==B->AsString();
        if (A->Type==EJson::Boolean) return A->AsBool()==B->AsBool();
        return A->Type==EJson::Null;
    }
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
