// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class PROJECTCORE_API FProjectSha256 final
{
public:
	static bool HashBuffer(const TArray<uint8>& Data, FString& OutHash);
	static bool HashFile(const FString& FilePath, FString& OutHash);

	/** Hashes text as UTF-8 after dropping one leading byte-order mark and converting CRLF and
	 *  CR line endings to LF, so a source file keeps one identity across checkouts. */
	static bool HashNormalizedText(const FString& Text, FString& OutHash);
};
