// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldSchemaReference.h"

#include "ProjectPaths.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace ProjectWorldSchemaReference
{
	bool ResolvesToCanonical(
		const FString& DocumentPath,
		const FString& DeclaredReference,
		const TCHAR* ExpectedSchemaFilename,
		FString& OutError)
	{
		if (DeclaredReference.IsEmpty() || !FPaths::IsRelative(DeclaredReference) ||
			DeclaredReference.Contains(TEXT("://")) || DeclaredReference.Contains(TEXT("\\")))
		{
			OutError = TEXT("$schema must be a forward-slash relative path.");
			return false;
		}

		const TSharedPtr<IPlugin> SchemaOwner = IPluginManager::Get().FindPlugin(TEXT("ProjectWorld"));
		const FString SchemaDataRoot = FProjectPaths::GetPluginDataDir(TEXT("ProjectWorld"));
		if (!SchemaOwner.IsValid() || SchemaOwner->GetType() != EPluginType::Project ||
			SchemaDataRoot.IsEmpty())
		{
			OutError = TEXT("Canonical ProjectWorld schema owner is unavailable.");
			return false;
		}

		FString ExpectedPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			SchemaDataRoot,
			TEXT("Schemas"),
			ExpectedSchemaFilename));
		FString ResolvedPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::GetPath(DocumentPath),
			DeclaredReference));
		FPaths::NormalizeFilename(ExpectedPath);
		FPaths::NormalizeFilename(ResolvedPath);
		if (!FPaths::IsSamePath(ResolvedPath, ExpectedPath))
		{
			OutError = FString::Printf(
				TEXT("$schema does not resolve to the canonical ProjectWorld schema: %s"),
				ExpectedSchemaFilename);
			return false;
		}
		return true;
	}
}
