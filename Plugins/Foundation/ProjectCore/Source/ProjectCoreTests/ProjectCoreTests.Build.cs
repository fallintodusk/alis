// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectCoreTests : ModuleRules
{
	public ProjectCoreTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tier 1 Test Module: Only depend on the plugin being tested
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore"   // Module being tested
		});
	}
}
