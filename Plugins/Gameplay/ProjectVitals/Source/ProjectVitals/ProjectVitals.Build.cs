// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectVitals : ModuleRules
{
	public ProjectVitals(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"ProjectCore",
			"ProjectGAS"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore",
			"Json"
		});

		// Stage vitals_config.json next to the build (NonUFS = real file on disk)
		// Designers can tweak values without recooking
		RuntimeDependencies.Add(
			"$(PluginDir)/Data/vitals_config.json",
			StagedFileType.NonUFS
		);
	}
}
