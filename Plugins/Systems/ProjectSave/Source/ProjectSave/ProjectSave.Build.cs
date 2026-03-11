using UnrealBuildTool;

public class ProjectSave : ModuleRules
{
	public ProjectSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProjectCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
