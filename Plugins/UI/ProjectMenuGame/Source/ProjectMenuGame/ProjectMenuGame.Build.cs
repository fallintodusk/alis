using UnrealBuildTool;

public class ProjectMenuGame : ModuleRules
{
	public ProjectMenuGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"ProjectCore",
			"ProjectUI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ProjectSettingsUI"
		});
	}
}
