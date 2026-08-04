// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonSerializer.h"

/**
 * JSON serializer policy that outputs object keys in sorted (alphabetical) order.
 *
 * Problem: FJsonObject uses TMap internally, which has non-deterministic iteration order.
 * When serializing JSON, keys come out in hash order, causing entire files to appear
 * changed in git diff even when only one value was modified.
 *
 * Solution: Inherit all behavior from FJsonSerializerPolicy_JsonObject, override only
 * SerializeIfObject to sort keys before writing. This uses UE's Policy pattern as intended.
 *
 * Usage:
 *   // Instead of: FJsonSerializer::Serialize(JsonObject, Writer);
 *   FSortedJsonSerializer::Serialize(JsonObject, Writer);
 *
 * Result: Deterministic JSON output, git diff shows only changed lines.
 */
struct FSortedJsonSerializerPolicy : public FJsonSerializerPolicy_JsonObject
{
	template<class CharType, class PrintPolicy>
	static bool SerializeIfObject(
		TArray<TSharedRef<FElement>>& ElementStack,
		TSharedRef<FElement>& Element,
		TJsonWriter<CharType, PrintPolicy>& Writer,
		bool bWriteValueOnly)
	{
		if (Element->Value->Type != EJson::Object)
		{
			return false;
		}

		if (Element->bHasBeenProcessed)
		{
			Writer.WriteObjectEnd();
		}
		else
		{
			Element->bHasBeenProcessed = true;
			ElementStack.Push(Element);

			if (bWriteValueOnly)
			{
				Writer.WriteObjectStart();
			}
			else
			{
				Writer.WriteObjectStart(Element->Identifier);
			}

			FMapOfValues ObjectPtr = Element->Value->AsObject();

			// Collect key/value pairs and sort alphabetically for
			// deterministic output. UE 5.8: FJsonObject keys are
			// UE::FSharedString - copy to FString pairs instead of
			// GenerateKeyArray/Find on FString.
			TArray<TPair<FString, TSharedPtr<FJsonValue>>> SortedPairs;
			SortedPairs.Reserve(ObjectPtr->Values.Num());
			for (const auto& Pair : ObjectPtr->Values)
			{
				SortedPairs.Emplace(FString(Pair.Key), Pair.Value);
			}
			SortedPairs.Sort(
				[](const TPair<FString, TSharedPtr<FJsonValue>>& A,
				   const TPair<FString, TSharedPtr<FJsonValue>>& B)
				{ return A.Key < B.Key; });

			// Push elements in reverse sorted order (stack is LIFO)
			for (int32 i = SortedPairs.Num() - 1; i >= 0; --i)
			{
				ElementStack.Push(MakeShared<FElement>(
					SortedPairs[i].Key, SortedPairs[i].Value));
			}
		}

		return true;
	}
};

// Convenience typedef - drop-in replacement for FJsonSerializer with sorted keys
using FSortedJsonSerializer = TJsonSerializer<FSortedJsonSerializerPolicy>;
