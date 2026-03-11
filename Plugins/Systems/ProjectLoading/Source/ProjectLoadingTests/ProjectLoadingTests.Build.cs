// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectLoadingTests : ModuleRules
{
	public ProjectLoadingTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tier 1 Test Module: Only depend on the plugin being tested
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",      // For types (FLoadRequest, ILoadingHandle)
			"ProjectLoading"    // Module being tested
		});

		// Test framework
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// UE5 Automation Framework is included by default
		});
	}
}
