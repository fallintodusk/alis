using UnrealBuildTool;

public class ProjectSinglePlayClient : ModuleRules
{
	public ProjectSinglePlayClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"InputCore",
			"ProjectCore",
			"ProjectSinglePlay",
			"ProjectUI",
			"ProjectVitalsUI",
			"ProjectInventoryUI",
			"ProjectMindUI"
		});
	}
}
