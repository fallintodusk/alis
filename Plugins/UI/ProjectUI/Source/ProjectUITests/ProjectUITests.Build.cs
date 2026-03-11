// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectUITests : ModuleRules
{
	public ProjectUITests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tier 1 Test Module: Only depend on the plugin being tested
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",              // For widget testing
			"Slate",
			"SlateCore",
			"ProjectCore",      // For logging
			"ProjectUI"         // Module being tested
		});

		// Test framework + UI input simulation
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AutomationDriver"  // For UI input simulation (Click, Type, Drag)
		});
	}
}
