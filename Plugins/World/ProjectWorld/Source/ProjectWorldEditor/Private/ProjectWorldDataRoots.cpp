// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

#include "ProjectWorldDataRoots.h"

#include "ProjectPaths.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace
{
	bool IsPluginToken(const FString& Value)
	{
		if (!Value.StartsWith(TEXT("Project")) || Value.Len() <= 7)
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			const bool bUpperAscii = Character >= TEXT('A') && Character <= TEXT('Z');
			const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
			const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
			if (!bUpperAscii && !bLowerAscii && !bDigitAscii)
			{
				return false;
			}
		}
		return true;
	}

	bool IsPackageUnder(const FString& PackagePath, const FString& Root)
	{
		if (!PackagePath.StartsWith(Root) || PackagePath.Len() <= Root.Len() ||
			PackagePath.EndsWith(TEXT("/")) || PackagePath.Contains(TEXT("//")))
		{
			return false;
		}
		for (int32 Index = Root.Len(); Index < PackagePath.Len(); ++Index)
		{
			const TCHAR Character = PackagePath[Index];
			const bool bUpperAscii = Character >= TEXT('A') && Character <= TEXT('Z');
			const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
			const bool bDigitAscii = Character >= TEXT('0') && Character <= TEXT('9');
			if (!bUpperAscii && !bLowerAscii && !bDigitAscii &&
				Character != TEXT('_') && Character != TEXT('/'))
			{
				return false;
			}
		}
		return true;
	}
}

bool FProjectWorldDataRoots::IsGeneratedPackage(const FString& PackagePath) const
{
	return IsPackageUnder(PackagePath, GeneratedPackageRoot);
}

bool FProjectWorldDataRoots::IsAuthoredPackage(const FString& PackagePath) const
{
	return IsPackageUnder(PackagePath, AuthoredPackageRoot);
}

bool FProjectWorldDataRoots::Resolve(
	const FString& PluginName,
	FProjectWorldDataRoots& OutRoots,
	FString& OutError)
{
	OutRoots = FProjectWorldDataRoots();
	if (!IsPluginToken(PluginName))
	{
		OutError = TEXT("World-data owner is not a valid Project* plugin token.");
		return false;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid() || Plugin->GetType() != EPluginType::Project ||
		!Plugin->CanContainContent())
	{
		OutError = FString::Printf(
			TEXT("World-data owner is not a content-capable project plugin: %s"),
			*PluginName);
		return false;
	}

	const FString DataRoot = FProjectPaths::GetPluginDataDir(PluginName);
	const FString MountRoot = Plugin->GetMountedAssetPath();
	if (DataRoot.IsEmpty() || MountRoot.IsEmpty() || !MountRoot.StartsWith(TEXT("/")) ||
		!MountRoot.EndsWith(TEXT("/")))
	{
		OutError = FString::Printf(
			TEXT("World-data plugin exposes no valid data or mounted-content root: %s"),
			*PluginName);
		return false;
	}

	OutRoots.PluginName = PluginName;
	OutRoots.MountRoot = MountRoot;
	OutRoots.GeneratedPackageRoot = MountRoot + TEXT("Generated/");
	OutRoots.AuthoredPackageRoot = MountRoot + TEXT("Authored/");
	OutRoots.ContentRoot = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
	OutRoots.DataRoot = FPaths::ConvertRelativePathToFull(DataRoot);
	OutRoots.ManifestRoot = FPaths::Combine(OutRoots.DataRoot, TEXT("Manifests"));
	return true;
}
