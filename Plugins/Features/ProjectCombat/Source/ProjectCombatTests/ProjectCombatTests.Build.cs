// Copyright ALIS. All Rights Reserved.
// License terms: see repository root LICENSE.

using UnrealBuildTool;

public class ProjectCombatTests : ModuleRules
{
	public ProjectCombatTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCombat"   // Module being tested
		});
	}
}
