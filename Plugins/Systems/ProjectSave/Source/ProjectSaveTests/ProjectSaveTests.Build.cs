// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSaveTests : ModuleRules
{
	public ProjectSaveTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tier 1 Test Module: Only depend on the plugin being tested
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectSave"   // Module being tested
		});
	}
}
