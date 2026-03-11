using UnrealBuildTool;

public class ProjectWorld : ModuleRules
{
	public ProjectWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectCore"
		});

		// PrivateDependencyModuleNames.AddRange(new[]
		// {
		// 	// Add private dependencies here
		// });
	}
}