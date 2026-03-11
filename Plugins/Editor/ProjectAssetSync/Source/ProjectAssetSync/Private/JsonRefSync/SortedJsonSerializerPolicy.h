// Copyright ALIS. All Rights Reserved.

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

			// Get keys and sort alphabetically for deterministic output
			TArray<FString> Keys;
			ObjectPtr->Values.GenerateKeyArray(Keys);
			Keys.Sort();

			// Push elements in reverse sorted order (stack is LIFO)
			// Look up each value by key - safe, no assumption about parallel array ordering
			for (int32 i = Keys.Num() - 1; i >= 0; --i)
			{
				const FString& Key = Keys[i];
				if (const TSharedPtr<FJsonValue>* FoundValue = ObjectPtr->Values.Find(Key))
				{
					ElementStack.Push(MakeShared<FElement>(Key, *FoundValue));
				}
			}
		}

		return true;
	}
};

// Convenience typedef - drop-in replacement for FJsonSerializer with sorted keys
using FSortedJsonSerializer = TJsonSerializer<FSortedJsonSerializerPolicy>;
