// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectEditorCore : ModuleRules
{
	public ProjectEditorCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
