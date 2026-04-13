// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSkeletalCapabilities : ModuleRules
{
	public ProjectSkeletalCapabilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Public/LocalBody"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ProjectObjectCapabilities",
			"CustomizableObject",
			"AnimGraphRuntime",
			"AnimationCore",
			"Json",
			"JsonUtilities"
		});
	}
}
