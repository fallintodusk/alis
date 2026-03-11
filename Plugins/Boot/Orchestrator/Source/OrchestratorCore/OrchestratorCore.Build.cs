// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OrchestratorCore : ModuleRules
{
	public OrchestratorCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;  // Required for try-catch in feature loading

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ModularGameplay",
			"ProjectCore"  // For IOrchestratorRegistry, IProjectFeatureModule, IFeatureModuleRegistry (SOLID - depend on abstractions)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json",
			"JsonUtilities",
			"Projects",
			"libzip"  // For zip extraction
		});

		// Windows Crypto API for Authenticode verification (Windows only)
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("wintrust.lib");
			PublicSystemLibraries.Add("crypt32.lib");
		}

		StageDataDir(Target);
	}

	private void StageDataDir(ReadOnlyTargetRules Target)
	{
		if (Target.Type == TargetType.Editor)
			return;

		string DataDir = Path.Combine(PluginDirectory, "Data");
		if (!Directory.Exists(DataDir))
			return;

		RuntimeDependencies.Add(Path.Combine(DataDir, "..."), StagedFileType.UFS);
	}
}
