// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectMaterialCompilerIdentity.h"

#include "Utilities/ProjectSha256.h"

FString FProjectMaterialCompilerIdentity::ComputeSha256(const TArrayView<const uint8> Bytes)
{
	TArray<uint8> Copy;
	Copy.Append(Bytes.GetData(), Bytes.Num());
	FString Hash;
	FProjectSha256::HashBuffer(Copy, Hash);
	return Hash;
}

FString FProjectMaterialCompilerIdentity::ComputeStringSha256(const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	return ComputeSha256(MakeArrayView(
		reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()));
}

FString FProjectMaterialCompilerIdentity::GetCompilerFingerprint()
{
#ifndef PROJECT_MATERIAL_COMPILER_SOURCE_SHA256
#error ProjectMaterialEditor must bind its compiler fingerprint to the admitted source set.
#endif
	return FString(UTF8_TO_TCHAR(PROJECT_MATERIAL_COMPILER_SOURCE_SHA256));
}
