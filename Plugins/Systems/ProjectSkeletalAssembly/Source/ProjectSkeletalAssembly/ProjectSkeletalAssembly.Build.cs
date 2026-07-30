// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

// ProjectSkeletalAssembly: Generic skeletal actor assembly framework.
// Provides lifecycle state machine and component graph assembly.
// No gameplay or skeleton-specific dependencies.
public class ProjectSkeletalAssembly : ModuleRules
{
	public ProjectSkeletalAssembly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"RenderCore",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
