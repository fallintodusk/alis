using UnrealBuildTool;

public class ProjectMind : ModuleRules
{
	public ProjectMind(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"Json",
			"ProjectCore"
		});
	}
}
