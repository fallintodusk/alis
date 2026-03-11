// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectDialogueTests : ModuleRules
{
	public ProjectDialogueTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ProjectDialogue"   // Module being tested
		});
	}
}
