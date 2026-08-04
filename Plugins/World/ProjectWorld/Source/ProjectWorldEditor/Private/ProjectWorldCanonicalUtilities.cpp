// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldCanonicalBundle.h"

#include "Misc/Paths.h"
#include "Utilities/ProjectSha256.h"

bool FProjectWorldCanonicalLoader::ComputeFileSha256(const FString& Path, FString& OutHash)
{
	return FProjectSha256::HashFile(Path, OutHash);
}

bool FProjectWorldCanonicalLoader::ResolveOwnedOutputPath(
	const FString& OutputRoot,
	const FString& RelativePath,
	FString& OutPath)
{
	if (RelativePath.IsEmpty() || !FPaths::IsRelative(RelativePath) ||
		RelativePath.Contains(TEXT("..")) || RelativePath.Contains(TEXT(":")))
	{
		return false;
	}

	const FString FullRoot = FPaths::ConvertRelativePathToFull(OutputRoot);
	OutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FullRoot, RelativePath));
	FPaths::NormalizeFilename(OutPath);
	return FPaths::IsUnderDirectory(OutPath, FullRoot);
}
