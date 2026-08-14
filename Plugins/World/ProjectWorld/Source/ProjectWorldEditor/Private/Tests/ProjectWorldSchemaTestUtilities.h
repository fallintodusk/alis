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
		const FString Marker = TEXT("\"$schema\": \"");
		const int32 MarkerIndex = Source.Find(Marker, ESearchCase::CaseSensitive);
		if (MarkerIndex == INDEX_NONE)
		{
			return Source;
		}
		const int32 ValueIndex = MarkerIndex + Marker.Len();
		const int32 ValueEnd = Source.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueIndex);
		if (ValueEnd == INDEX_NONE)
		{
			return Source;
		}
		return Source.Left(ValueIndex) + ReferenceFor(DocumentPath, SchemaFilename) + Source.Mid(ValueEnd);
	}
}
