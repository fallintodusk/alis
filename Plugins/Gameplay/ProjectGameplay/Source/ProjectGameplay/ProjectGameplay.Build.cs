using UnrealBuildTool;

public class ProjectGameplay : ModuleRules
{
	public ProjectGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore",
			"ProjectFeature"
		});
	}
}
