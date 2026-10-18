// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class ACharacter;
class UWorld;

class FProjectWorldProductBuildingEvidence
{
public:
	bool Load(const FString& Path, const FString& Hash, const FString& Map,
		const FString& RuntimeHash, FString& OutError);
	bool Probe(UWorld& World, ACharacter& Character, FString& OutError);
	void AppendReceipt(FJsonObject& Receipt) const;

private:
	FString ReferenceHash;
	TSharedPtr<FJsonObject> Reference;
	TSharedPtr<FJsonObject> Evidence;
};
