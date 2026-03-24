// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectInventoryTests : ModuleRules
{
	public ProjectInventoryTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ProjectCore",
			"ProjectInventory"   // Module being tested
		});
	}
}
