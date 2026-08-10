// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace ProjectWorldSchemaTestUtilities
{
	inline FString ReferenceFor(const FString& DocumentPath, const TCHAR* SchemaFilename)
	{
		FString Reference = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("World/ProjectWorld/Data/Schemas"),
			SchemaFilename);
		if (!FPaths::MakePathRelativeTo(Reference, *DocumentPath))
		{
			return FString();
		}
		FPaths::NormalizeFilename(Reference);
		return Reference;
	}

	inline FString Rewrite(
		const FString& Source,
		const FString& DocumentPath,
		const TCHAR* SchemaFilename)
	{
		return Source.Replace(
			*FString::Printf(TEXT("../Schemas/%s"), SchemaFilename),
			*ReferenceFor(DocumentPath, SchemaFilename),
			ESearchCase::CaseSensitive);
	}
}
