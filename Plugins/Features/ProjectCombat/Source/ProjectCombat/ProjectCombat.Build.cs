using UnrealBuildTool;

public class ProjectCombat : ModuleRules
{
	public ProjectCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectFeature"  // Feature registry
		});
	}
}
