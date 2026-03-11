// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSoundSystem : ModuleRules
{
	public ProjectSoundSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
