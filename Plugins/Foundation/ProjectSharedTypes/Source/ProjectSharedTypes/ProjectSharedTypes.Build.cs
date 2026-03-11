// Copyright ALIS. All Rights Reserved.

using UnrealBuildTool;

public class ProjectSharedTypes : ModuleRules
{
	public ProjectSharedTypes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"GameplayTags"
		});
	}
}
