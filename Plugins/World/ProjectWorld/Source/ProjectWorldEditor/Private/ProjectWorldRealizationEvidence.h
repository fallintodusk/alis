// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#pragma once

#include "ProjectWorldRealizationService.h"
#include "Misc/Paths.h"

namespace ProjectWorldRealizationEvidence
{
	inline bool IsInputDisjoint(const FProjectWorldRealizationRequest& Request)
	{
		if (Request.CompileResultPath.IsEmpty() || Request.ResultPath.IsEmpty()) return false;
		auto FullPath = [](const FString& Path)
		{
			FString Full = FPaths::ConvertRelativePathToFull(Path);
			FPaths::NormalizeFilename(Full);
			FPaths::CollapseRelativeDirectories(Full);
			return Full;
		};
		const FString Result = FullPath(Request.ResultPath);
		const FString CanonicalRoot = FPaths::GetPath(FullPath(Request.CompileResultPath));
		const TArray<FString> Inputs = {Request.PresentationProfilePath, Request.RuntimeProfilePath,
			Request.AuthoredOverlayProfilePath, Request.RealizationProfilePath, Request.LayerDirtyInputPath};
		TSet<FString> Outputs;
		for (const FString& Output : {Result, FPaths::ChangeExtension(Result, TEXT("log")), Result + TEXT(".tmp")})
		{
			const FString Key = Output.ToLower();
			if (Outputs.Contains(Key)) return false;
			Outputs.Add(Key);
			if (FPaths::IsUnderDirectory(Output, CanonicalRoot)) return false;
			for (const FString& Input : Inputs)
			{
				if (!Input.IsEmpty() && Output.Equals(FullPath(Input), ESearchCase::IgnoreCase)) return false;
			}
		}
		return true;
	}
}
